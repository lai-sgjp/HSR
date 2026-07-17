#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HSRHUD.generated.h"

class UHSRUserWidget;

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

	// Development-only Phase 2 test interface
	UFUNCTION(BlueprintCallable, Category = "HUD|Development", meta = (DevelopmentOnly))
	void RequestRebuildExplorationHUDForPhase2Test();

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HUD")
	TSubclassOf<UHSRUserWidget> ExplorationWidgetClass;

	UPROPERTY()
	TObjectPtr<UHSRUserWidget> ExplorationWidgetInstance;
};
