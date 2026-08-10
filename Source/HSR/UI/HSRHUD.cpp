#include "HSRHUD.h"
#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "HSRInteractionViewModel.h"
#include "HSRInventoryRewardViewModel.h"
#include "HSRInventoryRewardWidget.h"
#include "Inventory/HSRInventoryModuleWidget.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../Character/HSRCharacterBase.h"
#include "HSRScreenWidget.h"
#include "HSRUIManagerSubsystem.h"
#include "../Player/HSRPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"

void AHSRHUD::BeginPlay()
{
	Super::BeginPlay();
	ShowExplorationHUD();
}

void AHSRHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameInstance* GI = GetGameInstance();
	const UHSRMapSubsystem* Maps = GI ? GI->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	const UHSRBattleTransitionSubsystem* BattleTravel = GI ? GI->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	const bool bAuthorizedTravelPending = (Maps && Maps->HasPendingTravel())
		|| (BattleTravel && (BattleTravel->HasPending() || BattleTravel->HasReturnPending()));
	const bool bCaptureTravel = ShouldCaptureTravelRestore(EndPlayReason, bAuthorizedTravelPending);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 HUD EndPlay Reason=%d CaptureTravel=%s"),
		static_cast<int32>(EndPlayReason), bCaptureTravel ? TEXT("true") : TEXT("false"));
	if (bCaptureTravel)
	{
		if (AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(GetOwningPlayerController()))
		{
			if (ULocalPlayer* LP = HSRPC->GetLocalPlayer())
			{
				if (UHSRUIManagerSubsystem* UIManager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
				{
					const EHSRUIScreenResult Result = UIManager->TeardownExplorationHostForTravel(this, HSRPC);
					bUIHostAlreadyUnregistered = Result != EHSRUIScreenResult::InvalidHost;
				}
			}
		}
	}
	RemoveExplorationHUD();
	Super::EndPlay(EndPlayReason);
}

TMap<EHSRFrontendModule, TSubclassOf<UUserWidget>> AHSRHUD::BuildFrontendModuleWidgetClasses() const
{
	TMap<EHSRFrontendModule, TSubclassOf<UUserWidget>> Classes;
	Classes.Add(EHSRFrontendModule::Party, PartyWidgetClass);
	Classes.Add(EHSRFrontendModule::Map, MapWidgetClass);
	Classes.Add(EHSRFrontendModule::Challenge, ChallengeWidgetClass);
	Classes.Add(EHSRFrontendModule::Quest, QuestWidgetClass);
	Classes.Add(EHSRFrontendModule::Save, SaveWidgetClass);
	return Classes;
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
	if (AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(PC))
	{
		if (ULocalPlayer* LP = HSRPC->GetLocalPlayer())
		{
			if (UHSRUIManagerSubsystem* UIManager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
			{
				UIManager->RegisterExplorationHost(this, HSRPC, ExplorationWidgetInstance, FrontendShellClass,
					FrontendModuleRootClass, CharacterDetailWidgetClass, InventoryWidgetClass,
					InventoryModuleWidgetClass,
					DialogueOverlayWidgetClass,
					BuildFrontendModuleWidgetClasses());
			}
		}
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSRInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	UHSRRewardSubsystem* Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	if (Inventory && Reward && RewardSummaryWidgetClass)
	{
		RewardSummaryViewModel = NewObject<UHSRInventoryRewardViewModel>(this);
		RewardSummaryViewModel->Initialize(Inventory, Reward);
		FHSRInventoryRewardSnapshot InitialSnapshot;
		if (RewardSummaryViewModel->GetSnapshot(InitialSnapshot))
		{
			RewardSummaryWidgetInstance = CreateWidget<UHSRRewardSummaryWidget>(PC, RewardSummaryWidgetClass);
			if (RewardSummaryWidgetInstance)
			{
				RewardSummaryWidgetInstance->SetViewModel(RewardSummaryViewModel);
				RewardSummaryWidgetInstance->AddToViewport();
			}
			else
			{
				RewardSummaryViewModel->Shutdown();
				RewardSummaryViewModel = nullptr;
			}
		}
		else
		{
			RewardSummaryViewModel->Shutdown();
			RewardSummaryViewModel = nullptr;
		}
	}

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

	if (ObservedInteractionComponent.Get() != InteractComp)
	{
		if (ObservedInteractionComponent.IsValid())
		{
			ObservedInteractionComponent->OnInteractionCompleted.RemoveDynamic(
				this, &ThisClass::HandleInteractionCompleted);
		}
		ObservedInteractionComponent = InteractComp;
	}
	InteractComp->OnInteractionCompleted.AddUniqueDynamic(this, &ThisClass::HandleInteractionCompleted);

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
	if (ObservedInteractionComponent.IsValid())
	{
		ObservedInteractionComponent->OnInteractionCompleted.RemoveDynamic(
			this, &ThisClass::HandleInteractionCompleted);
		ObservedInteractionComponent.Reset();
	}
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

void AHSRHUD::HandleInteractionCompleted(const FHSRInteractionResult& Result)
{
	if (!Result.HasDialoguePayload())
	{
		return;
	}

	AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(GetOwningPlayerController());
	ULocalPlayer* LP = HSRPC ? HSRPC->GetLocalPlayer() : nullptr;
	UHSRUIManagerSubsystem* UIManager = LP ? LP->GetSubsystem<UHSRUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::HandleInteractionCompleted - Dialogue payload has no UIManager"));
		return;
	}

	const EHSRUIScreenResult OpenResult = UIManager->OpenDialogueOverlay(
		Result.DialogueId, Result.DialogueNodeId);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue RequestOpen Result=%d Dialogue=%s Node=%s HasOverlay=%s"),
		static_cast<int32>(OpenResult), *Result.DialogueId.ToString(), *Result.DialogueNodeId.ToString(),
		UIManager->HasOpenDialogueOverlay() ? TEXT("true") : TEXT("false"));
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
	if (!bUIHostAlreadyUnregistered)
	{
		if (AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(GetOwningPlayerController()))
		{
			if (ULocalPlayer* LP = HSRPC->GetLocalPlayer())
			{
				if (UHSRUIManagerSubsystem* UIManager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
				{
					const EHSRUIScreenResult Result = UIManager->UnregisterExplorationHost(this, HSRPC);
					if (Result != EHSRUIScreenResult::Success)
					{
						UE_LOG(LogTemp, Error, TEXT("HSRUI P17 HUD host teardown Result=%d; manager forced stale-host cleanup"),
							static_cast<int32>(Result));
					}
				}
			}
		}
	}
	bUIHostAlreadyUnregistered = false;
	ClearInteractionObserverInstance();
	if (RewardSummaryWidgetInstance) { RewardSummaryWidgetInstance->RemoveFromParent(); RewardSummaryWidgetInstance = nullptr; }
	if (RewardSummaryViewModel) { RewardSummaryViewModel->Shutdown(); RewardSummaryViewModel = nullptr; }

	if (!ExplorationWidgetInstance)
	{
		return;
	}

	ExplorationWidgetInstance->RemoveFromParent();
	ExplorationWidgetInstance = nullptr;
}
