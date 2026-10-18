// Explicit local Development QA. Exercises the shipped UI/session path, not mouse input.
#if !UE_BUILD_SHIPPING
#include "../Player/HSRPlayerController.h"
#include "../UI/Character/HSRCharacterShellWidget.h"
#include "../UI/Character/HSRCharacterPreviewWidget.h"
#include "../UI/Character/Preview/HSRCharacterPreviewSession.h"
#include "../UI/Character/Preview/HSRCharacterPreviewStage.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "Serialization/JsonSerializer.h"
#include "TextureResource.h"
#include "UnrealClient.h"

namespace HSRCharacterPreviewPackagedProbe
{
static bool bRunning = false;
enum class EStep { Startup, Open, Character, CharacterShot, Animation, Rapid, LoadingClose,
	CycleOpen, CycleReady, CycleClosed, FinalClosed };

struct FRun
{
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<AHSRPlayerController> Controller;
	TWeakObjectPtr<APawn> Pawn;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	TWeakObjectPtr<UHSRCharacterShellWidget> Shell;
	TWeakObjectPtr<UHSRCharacterPreviewSession> Session;
	TArray<FHSRCharacterProfileSnapshot> InitialProfiles;
	TArray<FName> Ids;
	TArray<TSharedPtr<FJsonValue>> Checks;
	TArray<TSharedPtr<FJsonValue>> Characters;
	TArray<FString> Screenshots;
	FTransform InitialTransform;
	FString Folder;
	EStep Step = EStep::Startup;
	FDateTime BeganUtc = FDateTime::UtcNow();
	double Began = FPlatformTime::Seconds();
	double StepBegan = Began;
	int32 Index = 0;
	int32 Cycles = 0;
	int32 CaptureBefore = 0;
	bool bFailed = false;
	bool bCsvStarted = false;
	bool bBaseline = false;

	void Enter(EStep Next) { Step = Next; StepBegan = FPlatformTime::Seconds(); }
	void Check(const FString& Name, bool bPass, const FString& Detail = FString())
	{
		auto Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Name);
		Item->SetBoolField(TEXT("pass"), bPass);
		Item->SetStringField(TEXT("detail"), Detail);
		Checks.Add(MakeShared<FJsonValueObject>(Item));
		bFailed |= !bPass;
		UE_LOG(LogTemp, Display, TEXT("CharacterPreviewProbe %s: %s %s"),
			bPass ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail);
	}
	int32 StageCount() const
	{
		int32 Count = 0;
		if (UWorld* ActiveWorld = World.Get())
			for (TActorIterator<AHSRCharacterPreviewStage> It(ActiveWorld); It; ++It)
				if (IsValid(*It) && !It->IsActorBeingDestroyed()) ++Count;
		return Count;
	}
	bool FindSession()
	{
		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World.Get(), Widgets, UHSRCharacterShellWidget::StaticClass(), false);
		for (UUserWidget* Widget : Widgets)
		{
			auto* Candidate = Cast<UHSRCharacterShellWidget>(Widget);
			// The character shell is a child of the module root, not itself added to viewport.
			// NativeDestruct clears CharacterPreview on detached previous screen instances.
			if (Candidate && Candidate->GetWorld() == World.Get() && Candidate->IsVisible() && Candidate->GetCharacterPreview())
			{
				Shell = Candidate;
				Session = Candidate->GetCharacterPreview()->GetPreviewSession();
				return Session.IsValid();
			}
		}
		return false;
	}
	bool Ready() const
	{
		return Session.IsValid() && Session->GetState() == EHSRCharacterPreviewState::Ready
			&& IsValid(Session->GetStage()) && Session->GetCaptureCount() > 0;
	}
	void Close()
	{
		if (Controller.IsValid()) Controller->RequestCloseFrontendToRoot();
	}
	bool SaveRenderTarget(const FString& Name)
	{
		UTextureRenderTarget2D* Target = Session.IsValid() ? Session->GetRenderTarget() : nullptr;
		FTextureRenderTargetResource* Resource = Target ? Target->GameThread_GetRenderTargetResource() : nullptr;
		TArray<FColor> Pixels;
		if (!Resource || !Resource->ReadPixels(Pixels) || Pixels.Num() != Target->SizeX * Target->SizeY) return false;
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(Target->SizeX, Target->SizeY, Pixels, Png);
		return Png.Num() > 0 && FFileHelper::SaveArrayToFile(Png, *(Folder / (Name + TEXT("_rt.png"))));
	}
	bool Finish(const FString& Error = FString())
	{
		if (!Error.IsEmpty()) Check(TEXT("completed_without_timeout_or_lifetime_failure"), false, Error);
		Close();
#if CSV_PROFILER
		if (bCsvStarted) FCsvProfiler::Get()->EndCapture();
#endif
		if (bBaseline)
		{
			Check(TEXT("pawn_identity_and_transform_unchanged"), Controller.IsValid() && Pawn.IsValid()
				&& Controller->GetPawn() == Pawn.Get() && Pawn->GetActorTransform().Equals(InitialTransform, 0.01f));
			TArray<FHSRCharacterProfileSnapshot> FinalProfiles;
			bool bSame = Profiles.IsValid() && Profiles->GetAllProfileSnapshots(FinalProfiles)
				&& FinalProfiles.Num() == InitialProfiles.Num();
			for (const auto& Initial : InitialProfiles)
			{
				const auto* Final = FinalProfiles.FindByPredicate([&](const auto& Item)
					{ return Item.RuntimeState.CharacterId == Initial.RuntimeState.CharacterId; });
				bSame &= Final && Final->RuntimeRevision == Initial.RuntimeRevision
					&& Final->RuntimeState.Level == Initial.RuntimeState.Level
					&& Final->RuntimeState.Experience == Initial.RuntimeState.Experience;
			}
			Check(TEXT("profile_ids_revisions_levels_xp_unchanged"), bSame);
		}
		for (const FString& Shot : Screenshots)
			Check(TEXT("native_ui_screenshot_written_this_run"), IFileManager::Get().FileSize(*Shot) > 0
				&& IFileManager::Get().GetTimeStamp(*Shot) >= BeganUtc, Shot);
		auto Report = MakeShared<FJsonObject>();
		Report->SetBoolField(TEXT("pass"), !bFailed);
		Report->SetStringField(TEXT("method"), TEXT("Opt-in native packaged UI/session API probe; NOT OS mouse/keyboard input testing. RT readback and FScreenshotRequest with UI."));
		Report->SetStringField(TEXT("map"), World.IsValid() ? World->GetMapName() : TEXT("world unavailable"));
		Report->SetNumberField(TEXT("elapsed_real_seconds"), FPlatformTime::Seconds() - Began);
		Report->SetNumberField(TEXT("ready_close_cycles"), Cycles);
		Report->SetBoolField(TEXT("csv_capture_started"), bCsvStarted);
		Report->SetArrayField(TEXT("checks"), Checks);
		Report->SetArrayField(TEXT("characters"), Characters);
		FString Json;
		FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
		const bool bWritten = FFileHelper::SaveStringToFile(Json, *(Folder / TEXT("report.json")));
		UE_LOG(LogTemp, Display, TEXT("CharacterPreviewProbe completed: %s report_written=%d %s"),
			bFailed ? TEXT("FAIL") : TEXT("PASS"), bWritten, *Folder);
		bRunning = false;
		return false;
	}
	bool Tick()
	{
		const double Now = FPlatformTime::Seconds();
		UWorld* ActiveWorld = World.Get();
		if (!ActiveWorld || ActiveWorld->bIsTearingDown) return Finish(TEXT("World ended during probe"));
		if (Now - Began > 180 || Now - StepBegan > 35) return Finish(TEXT("Bounded probe timeout"));
		if (Step != EStep::Startup && !Controller.IsValid()) return Finish(TEXT("PlayerController destroyed"));
		switch (Step)
		{
		case EStep::Startup:
		{
			if (Now - Began < 8) break;
			auto* PC = Cast<AHSRPlayerController>(ActiveWorld->GetFirstPlayerController());
			auto* Source = ActiveWorld->GetGameInstance() ? ActiveWorld->GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
			if (!PC || !PC->GetPawn() || !Source || !Source->GetAllProfileSnapshots(InitialProfiles) || InitialProfiles.IsEmpty()) break;
			Controller = PC; Pawn = PC->GetPawn(); Profiles = Source;
			InitialTransform = Pawn->GetActorTransform(); bBaseline = true;
#if CSV_PROFILER
			if (!FCsvProfiler::Get()->IsCapturing() && !FCsvProfiler::Get()->IsEndCapturePending() && !FCsvProfiler::Get()->IsWritingFile())
			{
				FCsvProfiler::Get()->BeginCapture(-1, Folder, TEXT("CharacterPreviewProbe.csv")); bCsvStarted = true;
			}
#endif
			PC->RequestOpenCharacterDetailScreen(); Enter(EStep::Open); break;
		}
		case EStep::Open:
		{
			if (!FindSession() || !Ready()) break;
			FHSRCharacterShellSnapshot Snapshot;
			if (!Shell->GetCurrentSnapshot(Snapshot)) break;
			for (const auto& Entry : Snapshot.CharacterEntries) if (Entry.bIsAvailable) Ids.AddUnique(Entry.CharacterId);
			Check(TEXT("real_character_screen_has_owned_entries"), !Ids.IsEmpty(), FString::FromInt(Ids.Num()));
			if (Ids.IsEmpty()) return Finish(TEXT("No available characters"));
			Shell->SelectCharacter(Ids[0]); Enter(EStep::Character); break;
		}
		case EStep::Character:
		{
			if (!Ready() || Session->GetCharacterId() != Ids[Index] || Now - StepBegan < 0.5) break;
			const UHSRCharacterDefinition* Definition = nullptr;
			USkeletalMesh* Mesh = Session->GetStage()->GetPreviewMesh()->GetSkeletalMeshAsset();
			const bool bMatch = Profiles.IsValid() && Profiles->GetDefinition(Ids[Index], Definition)
				&& Definition && Mesh && Mesh == Definition->CharacterMesh.Get();
			Check(TEXT("owned_character_mesh_matches_definition"), bMatch, Ids[Index].ToString());
			Check(TEXT("one_live_stage"), StageCount() == 1);
			const FString Name = FString::Printf(TEXT("character_%02d"), Index);
			const bool bSaved = SaveRenderTarget(Name);
			Check(TEXT("real_rt_png_written"), bSaved, Name);
			auto Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("id"), Ids[Index].ToString());
			Item->SetStringField(TEXT("mesh"), GetPathNameSafe(Mesh));
			Item->SetStringField(TEXT("rt_png"), Folder / (Name + TEXT("_rt.png")));
			Item->SetNumberField(TEXT("capture_count"), Session->GetCaptureCount());
			Item->SetBoolField(TEXT("has_idle"), Session->GetStage()->HasAnimation());
			Characters.Add(MakeShared<FJsonValueObject>(Item));
			const FString Shot = Folder / (Name + TEXT("_ui.png"));
			Screenshots.Add(Shot); FScreenshotRequest::RequestScreenshot(Shot, true, false);
			Enter(EStep::CharacterShot); break;
		}
		case EStep::CharacterShot:
			if (Now - StepBegan < 0.5) break;
			if (++Index < Ids.Num()) { Shell->SelectCharacter(Ids[Index]); Enter(EStep::Character); }
			else
			{
				auto* Stage = Session->GetStage();
				const float Yaw = Stage->GetPreviewYaw(), Distance = Stage->GetCameraDistance();
				Session->Rotate(80); Check(TEXT("api_rotate_changes_yaw"), !FMath::IsNearlyEqual(Yaw, Stage->GetPreviewYaw()));
				Session->Zoom(2); Check(TEXT("api_zoom_changes_distance"), Stage->GetCameraDistance() < Distance);
				Session->ResetView(); Check(TEXT("api_reset_restores_fit"), FMath::IsNearlyEqual(Stage->GetCameraDistance(), Stage->GetFitDistance(), 0.01f));
				const int64 Generation = Session->GetRequestGeneration(); Session->RequestCharacter(Session->GetCharacterId());
				Check(TEXT("duplicate_ready_request_is_noop"), Generation == Session->GetRequestGeneration());
				CaptureBefore = Session->GetCaptureCount(); Enter(EStep::Animation);
			}
			break;
		case EStep::Animation:
			if (Now - StepBegan < 0.75) break;
			Check(TEXT("real_menu_pauses_world"), ActiveWorld->IsPaused());
			Check(TEXT("idle_capture_advances_while_paused"), Ready() && Session->GetCaptureCount() > CaptureBefore,
				FString::Printf(TEXT("captures=%d real_seconds=%.3f"), Session->GetCaptureCount() - CaptureBefore, Now - StepBegan));
			for (int32 Attempt = 0; Attempt < 12; ++Attempt) Shell->SelectCharacter(Ids[Attempt % Ids.Num()]);
			Shell->SelectCharacter(Ids.Last()); Enter(EStep::Rapid); break;
		case EStep::Rapid:
		{
			if (!Ready() || Now - StepBegan < 0.5) break;
			const UHSRCharacterDefinition* Definition = nullptr;
			Check(TEXT("rapid_requests_latest_wins"), Session->GetCharacterId() == Ids.Last() && StageCount() == 1
				&& Profiles.IsValid() && Profiles->GetDefinition(Ids.Last(), Definition) && Definition
				&& Session->GetStage()->GetPreviewMesh()->GetSkeletalMeshAsset() == Definition->CharacterMesh.Get());
			Session->RequestCharacter(Ids[0] == Session->GetCharacterId() && Ids.Num() > 1 ? Ids[1] : Ids[0]);
			Check(TEXT("close_during_loading_exercised"), Session->GetState() == EHSRCharacterPreviewState::Loading);
			Close(); Enter(EStep::LoadingClose); break;
		}
		case EStep::LoadingClose:
			if (Now - StepBegan < 1.0) break;
			Check(TEXT("loading_close_no_late_stage"), StageCount() == 0 && (!Session.IsValid() || Session->GetState() == EHSRCharacterPreviewState::Closed));
			Enter(EStep::CycleOpen); break;
		case EStep::CycleOpen:
			Controller->RequestOpenCharacterDetailScreen(); Shell.Reset(); Session.Reset(); Enter(EStep::CycleReady); break;
		case EStep::CycleReady:
			if (!FindSession() || !Ready() || Now - StepBegan < 0.15) break;
			Check(FString::Printf(TEXT("cycle_%02d_ready_single_stage"), Cycles + 1), StageCount() == 1);
			CaptureBefore = Session->GetCaptureCount(); Close(); Enter(EStep::CycleClosed); break;
		case EStep::CycleClosed:
			if (Now - StepBegan < 0.15) break;
			Check(FString::Printf(TEXT("cycle_%02d_closed_no_capture_or_stage"), Cycles + 1), StageCount() == 0
				&& (!Session.IsValid() || (Session->GetState() == EHSRCharacterPreviewState::Closed && Session->GetCaptureCount() == CaptureBefore)));
			++Cycles; Enter(Cycles < 20 ? EStep::CycleOpen : EStep::FinalClosed); break;
		case EStep::FinalClosed:
			if (Now - StepBegan < 1.0) break;
			Check(TEXT("final_no_late_resurrection"), StageCount() == 0); return Finish();
		default: return Finish(TEXT("Invalid probe state"));
		}
		return true;
	}
};

static void Start(UWorld* World)
{
	if (bRunning || !World || !World->IsGameWorld() || World->bIsTearingDown) return;
	bRunning = true;
	auto Run = MakeShared<FRun>();
	Run->World = World;
	Run->Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CharacterPreviewProbe"));
	IFileManager::Get().MakeDirectory(*Run->Folder, true);
	UE_LOG(LogTemp, Display, TEXT("CharacterPreviewProbe started (native API QA, not mouse QA): %s"), *Run->Folder);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Run](float) { return Run->Tick(); }));
}

static FAutoConsoleCommandWithWorld Command(TEXT("HSR.CharacterPreviewProbe"),
	TEXT("Local Development-only character preview UI/session/render/lifecycle QA; writes Saved/CharacterPreviewProbe."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&Start));
}
#endif
