#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HSRHUD.generated.h"

class UHSRUserWidget;
class UHSRInteractionViewModel;
class UHSRInventoryRewardViewModel;
class UHSRInventoryWidget;
class UHSRRewardSummaryWidget;
class UHSRScreenWidget;
class UHSRFrontendShellWidget;
class UHSRFrontendModuleRootWidget;
class UUserWidget;

UCLASS()
class HSR_API AHSRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	static bool ShouldCaptureTravelRestore(EEndPlayReason::Type EndPlayReason, bool bAuthorizedTravelPending)
	{
		return EndPlayReason == EEndPlayReason::LevelTransition
			|| (EndPlayReason == EEndPlayReason::Destroyed && bAuthorizedTravelPending);
	}

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ShowExplorationHUD();

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void RemoveExplorationHUD();

	// Interaction observer lifecycle
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void RefreshInteractionObserver();

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void ClearInteractionObserverInstance();

	// Development-only Phase 2 test interface
	UFUNCTION(BlueprintCallable, Category = "HUD|Development", meta = (DevelopmentOnly))
	void RequestRebuildExplorationHUDForPhase2Test();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	TSubclassOf<UHSRUserWidget> ExplorationWidgetClass;

	UPROPERTY()
	TObjectPtr<UHSRUserWidget> ExplorationWidgetInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UHSRFrontendShellWidget> FrontendShellClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UHSRFrontendModuleRootWidget> FrontendModuleRootClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UHSRScreenWidget> CharacterDetailWidgetClass;

	UPROPERTY()
	TObjectPtr<UHSRInteractionViewModel> InteractionViewModel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P13")
	TSubclassOf<UHSRInventoryWidget> InventoryWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UUserWidget> PartyWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UUserWidget> MapWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UUserWidget> ChallengeWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UUserWidget> QuestWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P17")
	TSubclassOf<UUserWidget> SaveWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P13")
	TSubclassOf<UHSRRewardSummaryWidget> RewardSummaryWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> RewardSummaryViewModel;
	UPROPERTY(Transient)
	TObjectPtr<UHSRRewardSummaryWidget> RewardSummaryWidgetInstance;
	bool bUIHostAlreadyUnregistered = false;
};
