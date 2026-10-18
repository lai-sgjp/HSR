#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HSRCharacterPreviewStage.generated.h"

class USkeletalMeshComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UPointLightComponent;
class USkeletalMesh;
class UAnimSequenceBase;
class UTextureRenderTarget2D;

/** Presentation only: no Pawn, ASC, collision, replication or world gameplay tick. */
UCLASS(Transient, NotBlueprintable)
class HSR_API AHSRCharacterPreviewStage : public AActor
{
	GENERATED_BODY()
public:
	AHSRCharacterPreviewStage();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Interaction", meta=(ClampMin="0.01", ClampMax="5"))
	float RotationDegreesPerPixel = 0.4f;
	void Configure(USkeletalMesh* Mesh, UAnimSequenceBase* Idle, float Yaw, const FVector& Offset, float Scale);
	void SetRenderTarget(UTextureRenderTarget2D* Target);
	void SetAspectRatio(float Aspect);
	void Rotate(float PixelDelta);
	void Zoom(float WheelDelta);
	void ResetView();
	void UpdateAnimation(float DeltaSeconds);
	void Capture();
	void Stop();
	bool HasAnimation() const { return bHasAnimation; }
	UFUNCTION(BlueprintPure) USkeletalMeshComponent* GetPreviewMesh() const { return PreviewMesh; }
	UFUNCTION(BlueprintPure) USceneCaptureComponent2D* GetCaptureComponent() const { return CaptureComponent; }
	UFUNCTION(BlueprintPure) float GetPreviewYaw() const;
	UFUNCTION(BlueprintPure) float GetCameraDistance() const { return CameraDistance; }
	UFUNCTION(BlueprintPure) float GetFitDistance() const { return FitDistance; }
	UFUNCTION(BlueprintPure) float GetBoundsRadius() const { return BoundsRadius; }
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	void Reframe();
	void UpdateCamera();
	UPROPERTY() TObjectPtr<USceneComponent> StageRoot;
	UPROPERTY() TObjectPtr<USceneComponent> ModelPivot;
	UPROPERTY() TObjectPtr<USkeletalMeshComponent> PreviewMesh;
	UPROPERTY() TObjectPtr<USceneCaptureComponent2D> CaptureComponent;
	UPROPERTY() TObjectPtr<UPointLightComponent> KeyLight;
	UPROPERTY() TObjectPtr<UPointLightComponent> FillLight;
	float DefaultYaw = 0.0f;
	float AspectRatio = 2.0f / 3.0f;
	float BoundsRadius = 100.0f;
	float FitDistance = 400.0f;
	float CameraDistance = 400.0f;
	float AnimationTime = 0.0f;
	bool bHasAnimation = false;
};
