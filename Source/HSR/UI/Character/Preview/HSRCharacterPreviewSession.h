#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/Ticker.h"
#include "HSRCharacterPreviewSession.generated.h"

class AHSRCharacterPreviewStage;
class UHSRCharacterProfileSubsystem;
class UHSRCharacterDefinition;
class UTextureRenderTarget2D;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EHSRCharacterPreviewState : uint8 { Closed, Loading, Ready, Error };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHSRCharacterPreviewChanged);

/** Widget-owned, read-only presentation session. Business data stays in Profiles. */
UCLASS(Transient, BlueprintType)
class HSR_API UHSRCharacterPreviewSession : public UObject
{
	GENERATED_BODY()
public:
	/** Real-time preview capture limit; closed sessions never capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Performance", meta=(ClampMin="1", ClampMax="30"))
	float CaptureFramesPerSecond = 30.0f;
	/** Upper bound for either RT dimension; retains the viewport aspect ratio. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Preview|Performance", meta=(ClampMin="128", ClampMax="1024"))
	int32 RenderTargetMaxDimension = 1024;
	void Initialize(UWorld* InWorld, UHSRCharacterProfileSubsystem* InProfiles);
	UFUNCTION(BlueprintCallable) void RequestCharacter(FName InCharacterId);
	UFUNCTION(BlueprintCallable) void Close();
	UFUNCTION(BlueprintCallable) void Rotate(float PixelDelta);
	UFUNCTION(BlueprintCallable) void Zoom(float WheelDelta);
	UFUNCTION(BlueprintCallable) void ResetView();
	UFUNCTION(BlueprintCallable) void SetViewportSize(FVector2D Size);
	UFUNCTION(BlueprintPure) EHSRCharacterPreviewState GetState() const { return State; }
	UFUNCTION(BlueprintPure) FText GetErrorText() const { return ErrorText; }
	UFUNCTION(BlueprintPure) UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }
	UFUNCTION(BlueprintPure) FName GetCharacterId() const { return CharacterId; }
	UFUNCTION(BlueprintPure) int32 GetCaptureCount() const { return CaptureCount; }
	UFUNCTION(BlueprintPure) int64 GetRequestGeneration() const { return RequestGeneration; }
	UFUNCTION(BlueprintPure) AHSRCharacterPreviewStage* GetStage() const { return Stage; }
	UPROPERTY(BlueprintAssignable) FHSRCharacterPreviewChanged OnChanged;
protected:
	virtual void BeginDestroy() override;
private:
	void CancelLoad();
	void CompleteLoad(uint64 Generation);
	void Fail(const FText& Message);
	void ReleaseStage();
	void EnsureRenderTarget();
	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);
	bool TickPreview(float DeltaSeconds);
	void StopTicker();
	TWeakObjectPtr<UWorld> PreviewWorld;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	UPROPERTY(Transient) TObjectPtr<const UHSRCharacterDefinition> PendingDefinition;
	UPROPERTY(Transient) TObjectPtr<AHSRCharacterPreviewStage> Stage;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> RenderTarget;
	TSharedPtr<FStreamableHandle> LoadHandle;
	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle WorldCleanupHandle;
	EHSRCharacterPreviewState State = EHSRCharacterPreviewState::Closed;
	FText ErrorText;
	FName CharacterId;
	FVector2D ViewportSize = FVector2D(512.0f, 768.0f);
	uint64 RequestGeneration = 0;
	int32 CaptureCount = 0;
	float AccumulatedSeconds = 0.0f;
	bool bDirty = false;
};
