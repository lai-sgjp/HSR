#include "HSRHUD.h"
#include "HSRUserWidget.h"
#include "Blueprint/UserWidget.h"

void AHSRHUD::BeginPlay()
{
	Super::BeginPlay();
	ShowExplorationHUD();
}

void AHSRHUD::ShowExplorationHUD()
{
	if (ExplorationWidgetInstance)
	{
		return;
	}

	if (!ExplorationWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::ShowExplorationHUD - ExplorationWidgetClass is not set"));
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::ShowExplorationHUD - No OwningPlayerController"));
		return;
	}

	ExplorationWidgetInstance = CreateWidget<UHSRUserWidget>(PC, ExplorationWidgetClass);
	if (!ExplorationWidgetInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::ShowExplorationHUD - CreateWidget failed"));
		return;
	}

	ExplorationWidgetInstance->AddToViewport();
}

void AHSRHUD::RemoveExplorationHUD()
{
	if (!ExplorationWidgetInstance)
	{
		return;
	}

	ExplorationWidgetInstance->RemoveFromParent();
	ExplorationWidgetInstance = nullptr;
}
