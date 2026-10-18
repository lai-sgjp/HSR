#include "HSRCharacterPreviewStage.h"

#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/TextureRenderTarget2D.h"

AHSRCharacterPreviewStage::AHSRCharacterPreviewStage()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	SetActorEnableCollision(false);
	StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
	SetRootComponent(StageRoot);
	ModelPivot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelPivot"));
	ModelPivot->SetupAttachment(StageRoot);
	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(ModelPivot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetCanEverAffectNavigation(false);
	PreviewMesh->SetReceivesDecals(false);
	PreviewMesh->SetSimulatePhysics(false);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->SetVisibleInSceneCaptureOnly(true);
	PreviewMesh->SetLightingChannels(false, false, true);
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewMesh->PrimaryComponentTick.bStartWithTickEnabled = false;
	PreviewMesh->bEnableUpdateRateOptimizations = false;
	PreviewMesh->SetDisablePostProcessBlueprint(true);
	PreviewMesh->bDisableClothSimulation = true;
	CaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	CaptureComponent->SetupAttachment(StageRoot);
	CaptureComponent->bCaptureEveryFrame = false;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	CaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureComponent->FOVAngle = 35.0f;
	CaptureComponent->ShowFlags.SetAtmosphere(false);
	CaptureComponent->ShowFlags.SetFog(false);
	CaptureComponent->ShowFlags.SetVolumetricFog(false);
	CaptureComponent->ShowFlags.SetSkyLighting(false);
	CaptureComponent->ShowFlags.SetBloom(false);
	CaptureComponent->ShowFlags.SetMotionBlur(false);
	CaptureComponent->ShowFlags.SetLumenGlobalIllumination(false);
	CaptureComponent->ShowFlags.SetLumenReflections(false);
	CaptureComponent->ShowFlags.SetScreenSpaceReflections(false);
	CaptureComponent->ShowFlags.SetAmbientOcclusion(false);
	CaptureComponent->ShowFlags.SetReflectionEnvironment(false);
	CaptureComponent->ShowFlags.SetDecals(false);
	FPostProcessSettings& Post = CaptureComponent->PostProcessSettings;
	Post.bOverride_AutoExposureMethod = true;
	Post.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
	Post.bOverride_AutoExposureBias = true;
	Post.AutoExposureBias = 0.0f;
	Post.bOverride_AutoExposureApplyPhysicalCameraExposure = true;
	Post.AutoExposureApplyPhysicalCameraExposure = false;
	Post.bOverride_DynamicGlobalIlluminationMethod = true;
	Post.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
	Post.bOverride_ReflectionMethod = true;
	Post.ReflectionMethod = EReflectionMethod::None;
	KeyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(StageRoot);
	FillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FillLight"));
	FillLight->SetupAttachment(StageRoot);
	for (UPointLightComponent* Light : {KeyLight.Get(), FillLight.Get()})
	{
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetLightingChannels(false, false, true);
		Light->SetCastShadows(false);
		Light->SetUseInverseSquaredFalloff(false);
		Light->SetIntensity(3.0f);
	}
	FillLight->SetLightColor(FLinearColor(0.65f, 0.75f, 1.0f));
	FillLight->SetIntensity(1.5f);
}

void AHSRCharacterPreviewStage::Configure(USkeletalMesh* Mesh, UAnimSequenceBase* Idle, float Yaw,
	const FVector& Offset, float Scale)
{
	PreviewMesh->Stop();
	PreviewMesh->SetAnimation(nullptr);
	PreviewMesh->SetSkeletalMesh(Mesh);
	ModelPivot->SetRelativeRotation(FRotator::ZeroRotator);
	PreviewMesh->SetRelativeLocation(FVector::ZeroVector);
	PreviewMesh->SetRelativeRotation(FRotator::ZeroRotator);
	PreviewMesh->SetRelativeScale3D(FVector(FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.01f, 100.0f) : 1.0f));
	AnimationTime = 0.0f;
	bHasAnimation = Idle && Mesh && Mesh->GetSkeleton() == Idle->GetSkeleton();
	if (bHasAnimation)
	{
		PreviewMesh->PlayAnimation(Idle, true);
		// Sample our own clock without firing asset notifies (audio, particles, gameplay callbacks).
		if (UAnimSingleNodeInstance* Instance = PreviewMesh->GetSingleNodeInstance()) Instance->SetPlaying(false);
		UpdateAnimation(0.0f);
	}
	PreviewMesh->SetComponentTickEnabled(false);
	PreviewMesh->UpdateBounds();
	const FBoxSphereBounds Bounds = PreviewMesh->Bounds;
	BoundsRadius = FMath::IsFinite(Bounds.SphereRadius) && Bounds.SphereRadius > 1.0f
		? FMath::Min(Bounds.SphereRadius, 100000.0f) : 100.0f;
	// Centre the real imported bounds before rotating. Non-centred pivots do not orbit the camera.
	const FVector SafeOffset = Offset.ContainsNaN() ? FVector::ZeroVector : Offset.GetClampedToMaxSize(10000.0);
	const FVector BoundsOrigin = Bounds.Origin.ContainsNaN() ? GetActorLocation() : Bounds.Origin;
	PreviewMesh->SetRelativeLocation(GetActorLocation() - BoundsOrigin + SafeOffset);
	BoundsRadius += SafeOffset.Size();
	DefaultYaw = FMath::IsFinite(Yaw) ? Yaw : 0.0f;
	ResetView();
	CaptureComponent->ClearShowOnlyComponents();
	CaptureComponent->ShowOnlyComponent(PreviewMesh);
	KeyLight->SetRelativeLocation(FVector(BoundsRadius * 2.0f, -BoundsRadius, BoundsRadius));
	FillLight->SetRelativeLocation(FVector(BoundsRadius, BoundsRadius * 1.5f, BoundsRadius * 0.3f));
	KeyLight->SetAttenuationRadius(BoundsRadius * 6.0f);
	FillLight->SetAttenuationRadius(BoundsRadius * 6.0f);
}

void AHSRCharacterPreviewStage::SetRenderTarget(UTextureRenderTarget2D* Target)
{
	CaptureComponent->TextureTarget = Target;
}

void AHSRCharacterPreviewStage::SetAspectRatio(float Aspect)
{
	AspectRatio = FMath::IsFinite(Aspect) ? FMath::Clamp(Aspect, 0.25f, 4.0f) : 2.0f / 3.0f;
	Reframe();
}

void AHSRCharacterPreviewStage::Reframe()
{
	const float HalfHorizontal = FMath::DegreesToRadians(CaptureComponent->FOVAngle * 0.5f);
	const float HalfVertical = FMath::Atan(FMath::Tan(HalfHorizontal) / AspectRatio);
	FitDistance = BoundsRadius * 1.12f / FMath::Sin(FMath::Min(HalfHorizontal, HalfVertical));
	CameraDistance = FitDistance;
	UpdateCamera();
}

void AHSRCharacterPreviewStage::UpdateCamera()
{
	CaptureComponent->SetRelativeLocation(FVector(CameraDistance, 0.0f, 0.0f));
	CaptureComponent->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
}

void AHSRCharacterPreviewStage::Rotate(float PixelDelta)
{
	if (FMath::IsFinite(PixelDelta))
	{
		const float Speed = FMath::IsFinite(RotationDegreesPerPixel) ? FMath::Clamp(RotationDegreesPerPixel, 0.01f, 5.0f) : 0.4f;
		ModelPivot->AddLocalRotation(FRotator(0.0f, PixelDelta * Speed, 0.0f));
	}
}

void AHSRCharacterPreviewStage::Zoom(float WheelDelta)
{
	if (!FMath::IsFinite(WheelDelta)) return;
	// The closest view still contains the complete bounds; never intersects the mesh.
	CameraDistance = FMath::Clamp(CameraDistance * FMath::Pow(0.9f, FMath::Clamp(WheelDelta, -10.0f, 10.0f)),
		FitDistance / 1.10f, FitDistance * 1.6f);
	UpdateCamera();
}

void AHSRCharacterPreviewStage::ResetView()
{
	ModelPivot->SetRelativeRotation(FRotator(0.0f, DefaultYaw, 0.0f));
	Reframe();
}

float AHSRCharacterPreviewStage::GetPreviewYaw() const
{
	return ModelPivot->GetRelativeRotation().Yaw;
}

void AHSRCharacterPreviewStage::UpdateAnimation(float DeltaSeconds)
{
	if (!bHasAnimation) return;
	UAnimSingleNodeInstance* Instance = PreviewMesh->GetSingleNodeInstance();
	if (!Instance) return;
	const float Length = Instance->GetLength();
	if (Length > SMALL_NUMBER && FMath::IsFinite(DeltaSeconds))
	{
		AnimationTime = FMath::Fmod(AnimationTime + FMath::Max(DeltaSeconds, 0.0f), Length);
	}
	Instance->SetPosition(AnimationTime, false);
	PreviewMesh->TickAnimation(0.0f, false);
	PreviewMesh->RefreshBoneTransforms();
	PreviewMesh->MarkRenderTransformDirty();
	PreviewMesh->MarkRenderDynamicDataDirty();
}

void AHSRCharacterPreviewStage::Capture()
{
	if (CaptureComponent->TextureTarget) CaptureComponent->CaptureScene();
}

void AHSRCharacterPreviewStage::Stop()
{
	bHasAnimation = false;
	PreviewMesh->Stop();
	CaptureComponent->TextureTarget = nullptr;
	CaptureComponent->Deactivate();
}

void AHSRCharacterPreviewStage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	Super::EndPlay(EndPlayReason);
}
