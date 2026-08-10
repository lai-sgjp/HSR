#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "HSRPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UInputComponent;
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

	/**
	 * Control mode implied by the world this controller currently lives in. The battle GameMode's
	 * presence is the signal, so no cross-world PlayerController lookup is needed: callers run
	 * this on themselves at a point where `this` is already valid.
	 */
	EHSRPlayerControlMode ResolveControlModeForCurrentWorld() const;

	/**
	 * Single mapping from semantic control mode to input policy, and the reason Battle used to
	 * behave exactly like Exploration: the previous either/or derivation only ever tested for
	 * UIOnly, so Battle silently fell through to GameOnly with the cursor hidden. Static and
	 * pure so the mapping is testable without a world, and it fills a reference rather than
	 * returning by value to keep FHSRInputModePolicy forward-declared for consumers.
	 */
	static void BuildPolicyForControlMode(EHSRPlayerControlMode Mode, FHSRInputModePolicy& OutPolicy);

	bool ApplyUIInputPolicy(const FHSRInputModePolicy& Policy, EHSRPlayerControlMode SemanticMode);
	static bool ShouldBindFrontendInputComponent(const UInputComponent* Bound, const UInputComponent* Current)
	{
		return Current && Bound != Current;
	}
	static bool ShouldEnsureBattleReturnConsumer(bool bReturnPending, bool bConsumerPresent)
	{
		return bReturnPending && !bConsumerPresent;
	}
	static bool ShouldRestoreFrontendNavigationContext(bool bMarkedAdded, bool bContextPresent)
	{
		return !bMarkedAdded || !bContextPresent;
	}

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestOpenPauseScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestOpenCharacterDetailScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestOpenInventoryScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestBackScreen();

	UFUNCTION(BlueprintCallable, Category = "UI|Navigation")
	void RequestCloseFrontendToRoot();

protected:
	virtual void SetupInputComponent() override;

	void AddExplorationContext();
	void RemoveExplorationContext();
	void AddFrontendNavigationContext();
	static constexpr int32 FrontendInputPriority = 100;
	void HandlePauseBack();
	void HandleInventory();
	void HandleParty();
	void HandleMap();
	void HandleChallenge();
	void HandleCloseToRoot();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> ExplorationMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputMappingContext> FrontendNavigationMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> PauseBackAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> InventoryAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> PartyAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> MapAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> ChallengeAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input|Frontend", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UInputAction> CloseToRootAction;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Control")
	EHSRPlayerControlMode CurrentControlMode;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Control")
	bool bControlModeApplied;

	bool bExplorationContextAdded;
	bool bFrontendNavigationContextAdded;
	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> FrontendBindingsInputComponent;
	bool bInputSystemReady;
	EHSRUIInputIntent AppliedInputIntent;
};
