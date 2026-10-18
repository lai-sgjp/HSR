#include "HSRCharacterPreviewSession.h"

#include "HSRCharacterPreviewStage.h"
#include "../../../Data/Definitions/HSRCharacterDefinition.h"
#include "../../../Progression/HSRCharacterProfileSubsystem.h"
#include "Animation/AnimSequenceBase.h"
#include "Async/Async.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "UObject/GarbageCollection.h"

void UHSRCharacterPreviewSession::Initialize(UWorld* InWorld, UHSRCharacterProfileSubsystem* InProfiles)
{
	Close();
	PreviewWorld = InWorld;
	Profiles = InProfiles;
}

void UHSRCharacterPreviewSession::CancelLoad()
{
	++RequestGeneration;
	if (LoadHandle)
	{
		LoadHandle->CancelHandle();
		LoadHandle.Reset();
	}
	PendingDefinition = nullptr;
}

void UHSRCharacterPreviewSession::RequestCharacter(FName InCharacterId)
{
	if (CharacterId == InCharacterId && (State == EHSRCharacterPreviewState::Loading || State == EHSRCharacterPreviewState::Ready)) return;
	CancelLoad();
	StopTicker();
	ReleaseStage();
	CharacterId = InCharacterId;
	ErrorText = FText::GetEmpty();
	UWorld* World = PreviewWorld.Get();
	UHSRCharacterProfileSubsystem* ProfileSource = Profiles.Get();
	if (!World || World->bIsTearingDown || !ProfileSource)
	{
		Fail(NSLOCTEXT("HSRPreview", "NoWorld", "Preview unavailable in this world."));
		return;
	}
	FHSRCharacterProfileSnapshot Snapshot;
	const UHSRCharacterDefinition* Definition = nullptr;
	if (InCharacterId.IsNone() || !ProfileSource->GetProfileSnapshot(InCharacterId, Snapshot)
		|| !ProfileSource->GetDefinition(InCharacterId, Definition) || !Definition)
	{
		Fail(NSLOCTEXT("HSRPreview", "NoCharacter", "Character is not owned or has no definition."));
		return;
	}
	if (Definition->CharacterMesh.IsNull())
	{
		Fail(NSLOCTEXT("HSRPreview", "NoMesh", "This character has no preview mesh."));
		return;
	}
	if (!WorldCleanupHandle.IsValid())
	{
		WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(this, &ThisClass::OnWorldCleanup);
	}
	PendingDefinition = Definition;
	const uint64 Generation = RequestGeneration;
	State = EHSRCharacterPreviewState::Loading;
	OnChanged.Broadcast();
	// Listeners may close or select a different character while handling Loading.
	if (Generation != RequestGeneration || State != EHSRCharacterPreviewState::Loading) return;
	TArray<FSoftObjectPath> Paths;
	Paths.Add(Definition->CharacterMesh.ToSoftObjectPath());
	if (!Definition->PreviewIdleAnimation.IsNull()) Paths.AddUnique(Definition->PreviewIdleAnimation.ToSoftObjectPath());
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths,
		FStreamableDelegate::CreateWeakLambda(this, [this, Generation]() { CompleteLoad(Generation); }));
	if (!LoadHandle)
	{
		Fail(NSLOCTEXT("HSRPreview", "LoadRequestFailed", "Unable to load preview resources."));
	}
}

void UHSRCharacterPreviewSession::CompleteLoad(uint64 Generation)
{
	UWorld* World = PreviewWorld.Get();
	if (Generation != RequestGeneration || State != EHSRCharacterPreviewState::Loading || !World || World->bIsTearingDown) return;
	const UHSRCharacterDefinition* Definition = PendingDefinition;
	USkeletalMesh* Mesh = Definition ? Definition->CharacterMesh.Get() : nullptr;
	if (!Mesh)
	{
		Fail(NSLOCTEXT("HSRPreview", "LoadMeshFailed", "The preview mesh could not be loaded."));
		return;
	}
	EnsureRenderTarget();
	FActorSpawnParameters Spawn;
	Spawn.ObjectFlags |= RF_Transient;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Stage = World->SpawnActor<AHSRCharacterPreviewStage>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
	if (!Stage)
	{
		Fail(NSLOCTEXT("HSRPreview", "SpawnFailed", "Unable to create preview stage."));
		return;
	}
	const FVector Offset = Definition->PreviewOffset.ContainsNaN() ? FVector::ZeroVector : Definition->PreviewOffset;
	const float Scale = FMath::IsFinite(Definition->PreviewScale) ? Definition->PreviewScale : 1.0f;
	Stage->SetRenderTarget(RenderTarget);
	Stage->Configure(Mesh, Definition->PreviewIdleAnimation.Get(), Definition->PreviewYaw, Offset, Scale);
	Stage->SetAspectRatio(static_cast<float>(RenderTarget->SizeX) / RenderTarget->SizeY);
	// Mesh/animation are now held by the component. No battle AnimBP is instantiated.
	PendingDefinition = nullptr;
	LoadHandle.Reset();
	State = EHSRCharacterPreviewState::Ready;
	// Populate a reused RT before exposing Ready; otherwise the UI can briefly
	// display the previous character until the first scheduled capture.
	Stage->Capture();
	++CaptureCount;
	bDirty = false;
	AccumulatedSeconds = 0.0f;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickPreview));
	OnChanged.Broadcast();
}

void UHSRCharacterPreviewSession::EnsureRenderTarget()
{
	const float Aspect = FMath::Clamp(static_cast<float>(ViewportSize.X / ViewportSize.Y), 0.25f, 4.0f);
	float Height = 768.0f;
	float Width = Height * Aspect;
	const int32 MaxDimension = FMath::Clamp(RenderTargetMaxDimension, 128, 1024);
	const float Scale = FMath::Min(1.0f, static_cast<float>(MaxDimension) / FMath::Max(Width, Height));
	const int32 SizeX = FMath::Clamp(FMath::RoundToInt(Width * Scale), 32, MaxDimension);
	const int32 SizeY = FMath::Clamp(FMath::RoundToInt(Height * Scale), 32, MaxDimension);
	if (!RenderTarget)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this, NAME_None, RF_Transient);
		RenderTarget->ClearColor = FLinearColor(0.015f, 0.02f, 0.035f, 1.0f);
		RenderTarget->TargetGamma = 2.2f;
		RenderTarget->bAutoGenerateMips = false;
		RenderTarget->RenderTargetFormat = RTF_RGBA8;
		RenderTarget->InitCustomFormat(SizeX, SizeY, PF_B8G8R8A8, false);
		RenderTarget->UpdateResourceImmediate(true);
	}
	else if (RenderTarget->SizeX != SizeX || RenderTarget->SizeY != SizeY)
	{
		RenderTarget->ResizeTarget(SizeX, SizeY);
	}
}

bool UHSRCharacterPreviewSession::TickPreview(float DeltaSeconds)
{
	UWorld* World = PreviewWorld.Get();
	if (State != EHSRCharacterPreviewState::Ready || !IsValid(Stage) || !World || World->bIsTearingDown)
	{
		TickerHandle.Reset();
		return false;
	}
	// Core ticker uses real time, unlike world timers which freeze on this project's character page.
	AccumulatedSeconds += FMath::Max(DeltaSeconds, 0.0f);
	const float CaptureRate = FMath::IsFinite(CaptureFramesPerSecond) ? FMath::Clamp(CaptureFramesPerSecond, 1.0f, 30.0f) : 30.0f;
	if (AccumulatedSeconds < 1.0f / CaptureRate) return true;
	const float AnimationDelta = FMath::Min(AccumulatedSeconds, 1.0f);
	AccumulatedSeconds = 0.0f;
	if (Stage->HasAnimation()) Stage->UpdateAnimation(AnimationDelta);
	if (bDirty || Stage->HasAnimation())
	{
		Stage->Capture();
		++CaptureCount;
		bDirty = false;
	}
	return true;
}

void UHSRCharacterPreviewSession::Rotate(float PixelDelta)
{
	if (State == EHSRCharacterPreviewState::Ready && IsValid(Stage)) { Stage->Rotate(PixelDelta); bDirty = true; }
}

void UHSRCharacterPreviewSession::Zoom(float WheelDelta)
{
	if (State == EHSRCharacterPreviewState::Ready && IsValid(Stage)) { Stage->Zoom(WheelDelta); bDirty = true; }
}

void UHSRCharacterPreviewSession::ResetView()
{
	if (State == EHSRCharacterPreviewState::Ready && IsValid(Stage)) { Stage->ResetView(); bDirty = true; }
}

void UHSRCharacterPreviewSession::SetViewportSize(FVector2D Size)
{
	if (Size.ContainsNaN() || Size.X < 1.0 || Size.Y < 1.0 || ViewportSize.Equals(Size, 1.0)) return;
	ViewportSize = Size;
	if (RenderTarget)
	{
		EnsureRenderTarget();
		if (IsValid(Stage)) Stage->SetAspectRatio(static_cast<float>(RenderTarget->SizeX) / RenderTarget->SizeY);
		bDirty = true;
	}
}

void UHSRCharacterPreviewSession::Fail(const FText& Message)
{
	CancelLoad();
	StopTicker();
	ReleaseStage();
	ErrorText = Message;
	State = EHSRCharacterPreviewState::Error;
	OnChanged.Broadcast();
}

void UHSRCharacterPreviewSession::StopTicker()
{
	if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	AccumulatedSeconds = 0.0f;
}

void UHSRCharacterPreviewSession::ReleaseStage()
{
	if (IsValid(Stage))
	{
		Stage->Stop();
		if (!IsGarbageCollecting()) Stage->Destroy();
		else
		{
			// BeginDestroy is a fallback for owners which missed explicit Close. Do not
			// mutate the world's actor list during GC, or leave a stopped actor behind.
			const TWeakObjectPtr<AHSRCharacterPreviewStage> OrphanStage = Stage;
			AsyncTask(ENamedThreads::GameThread, [OrphanStage]()
			{
				if (OrphanStage.IsValid() && OrphanStage->GetWorld() && !OrphanStage->GetWorld()->bIsTearingDown)
				{
					OrphanStage->Destroy();
				}
			});
		}
	}
	Stage = nullptr;
}

void UHSRCharacterPreviewSession::Close()
{
	CancelLoad();
	StopTicker();
	if (WorldCleanupHandle.IsValid()) FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();
	ReleaseStage();
	RenderTarget = nullptr;
	bDirty = false;
	CharacterId = NAME_None;
	ErrorText = FText::GetEmpty();
	State = EHSRCharacterPreviewState::Closed;
	if (!HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)) OnChanged.Broadcast();
}

void UHSRCharacterPreviewSession::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	if (InWorld != PreviewWorld.Get()) return;
	Close();
	PreviewWorld.Reset();
	Profiles.Reset();
}

void UHSRCharacterPreviewSession::BeginDestroy()
{
	Close();
	Super::BeginDestroy();
}
