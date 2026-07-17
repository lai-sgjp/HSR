#include "HSRHUD.h"
#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../Character/HSRCharacterBase.h"

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

void AHSRHUD::RequestRebuildExplorationHUDForPhase2Test()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Rejected in Test/Shipping"));
	return;
#else
	UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Rebuilding ExplorationHUD"));
	RemoveExplorationHUD();

	TWeakObjectPtr<AHSRHUD> WeakThis(this);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
		{
			AHSRHUD* HUD = WeakThis.Get();
			if (!HUD)
			{
				UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - HUD destroyed before next tick"));
				return;
			}
			HUD->ShowExplorationHUD();
			// Snapshot path: new Widget Construct (BP) calls BroadcastCurrentValues as single entry point
		}));
	}
#endif
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
