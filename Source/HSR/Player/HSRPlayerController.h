#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HSRPlayerController.generated.h"

class UInputMappingContext;
struct FHSRInputModePolicy;
enum class EHSRUIInputIntent : uint8;

UENUM(BlueprintType)
enum class EHSRPlayerControlMode : uint8
{
	Exploration UMETA(DisplayName = "Exploration"),
	UIOnly      UMETA(DisplayName = "UI Only"),
	Battle      UMETA(DisplayName = "Battle")
};

UCLASS()
class HSR_API AHSRPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AHSRPlayerController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	UFUNCTION(BlueprintCallable, Category = "Control")
	void SetControlMode(EHSRPlayerControlMode NewMode);

	UFUNCTION(BlueprintPure, Category = "Control")
	EHSRPlayerControlMode GetControlMode() const { return CurrentControlMode; }

	bool ApplyUIInputPolicy(const FHSRInputModePolicy& Policy, EHSRPlayerControlMode SemanticMode);

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestOpenPauseScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestOpenCharacterDetailScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestOpenInventoryScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestBackScreen();

protected:
	virtual void SetupInputComponent() override;

	void AddExplorationContext();
	void RemoveExplorationContext();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> ExplorationMappingContext;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Control")
	EHSRPlayerControlMode CurrentControlMode;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Control")
	bool bControlModeApplied;

	bool bExplorationContextAdded;
	bool bInputSystemReady;
	EHSRUIInputIntent AppliedInputIntent;
};
