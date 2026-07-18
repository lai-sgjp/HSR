#include "HSRHUD.h"
#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "HSRInteractionViewModel.h"
#include "../Interaction/HSRInteractionComponent.h"
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

	// Set up interaction observation for current pawn
	RefreshInteractionObserver();
}


void AHSRHUD::RefreshInteractionObserver()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		ClearInteractionObserverInstance();
		return;
	}

	APawn* CurrentPawn = PC->GetPawn();
	if (!CurrentPawn)
	{
		ClearInteractionObserverInstance();
		return;
	}

	UHSRInteractionComponent* InteractComp = CurrentPawn->FindComponentByClass<UHSRInteractionComponent>();
	if (!InteractComp)
	{
		ClearInteractionObserverInstance();
		return;
	}

	if (!InteractionViewModel)
	{
		InteractionViewModel = NewObject<UHSRInteractionViewModel>(this);
		UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RefreshInteractionObserver - Created new VM[%d]"), InteractionViewModel->GetInstanceId());
	}

	// Observe first to establish component binding
	InteractionViewModel->Observe(InteractComp);

	// Then connect Widget — SetInteractionViewModel handles ForceCurrentSnapshot on new VM connections
	if (ExplorationWidgetInstance)
	{
		ExplorationWidgetInstance->SetInteractionViewModel(InteractionViewModel);
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RefreshInteractionObserver - VM[%d] Component=%s Pawn=%s"),
		InteractionViewModel->GetInstanceId(), *InteractComp->GetName(), *CurrentPawn->GetName());
}

void AHSRHUD::ClearInteractionObserverInstance()
{
	if (InteractionViewModel)
	{
		InteractionViewModel->Teardown();
		InteractionViewModel = nullptr;
	}
	if (ExplorationWidgetInstance)
	{
		ExplorationWidgetInstance->SetInteractionViewModel(nullptr);
	}
	UE_LOG(LogTemp, Log, TEXT("AHSRHUD::ClearInteractionObserverInstance - Cleared"));
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
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - GetWorld() is null, cannot schedule rebuild"));
		return;
	}
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		AHSRHUD* HUD = WeakThis.Get();
		if (!HUD)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - HUD destroyed before next tick"));
			return;
		}
		HUD->ShowExplorationHUD();
		// Validate PlayerController -> Pawn -> HUD chain (no second broadcast)
		APlayerController* PC = HUD->GetOwningPlayerController();
		if (!PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - PC is null after Show"));
			return;
		}
		APawn* Pawn = PC->GetPawn();
		if (!Pawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Pawn is null after Show"));
			return;
		}
		AHUD* HUDCheck = PC->GetHUD();
		UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Chain valid: PC=%s, Pawn=%s, HUD=%s"),
			*PC->GetName(), *Pawn->GetName(), HUDCheck ? *HUDCheck->GetName() : TEXT("null"));
		// Snapshot path: new Widget Construct (BP) calls BroadcastCurrentValues as single entry point
	}));
#endif
}

void AHSRHUD::RemoveExplorationHUD()
{
	ClearInteractionObserverInstance();

	if (!ExplorationWidgetInstance)
	{
		return;
	}

	ExplorationWidgetInstance->RemoveFromParent();
	ExplorationWidgetInstance = nullptr;
}
