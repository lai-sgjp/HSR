#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HSRHUD.generated.h"

class UHSRUserWidget;
class UHSRInteractionViewModel;
class UHSRInventoryRewardViewModel;
class UHSRInventoryWidget;
class UHSRRewardSummaryWidget;

UCLASS()
class HSR_API AHSRHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

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

	UPROPERTY()
	TObjectPtr<UHSRInteractionViewModel> InteractionViewModel;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P13")
	TSubclassOf<UHSRInventoryWidget> InventoryWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD|P13")
	TSubclassOf<UHSRRewardSummaryWidget> RewardSummaryWidgetClass;
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryRewardViewModel> InventoryRewardViewModel;
	UPROPERTY(Transient)
	TObjectPtr<UHSRInventoryWidget> InventoryWidgetInstance;
	UPROPERTY(Transient)
	TObjectPtr<UHSRRewardSummaryWidget> RewardSummaryWidgetInstance;
};
