#include "HSRPlayerController.h"
#include "../UI/HSRHUD.h"
#include "../Character/HSRExplorationCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "../UI/HSRScreenStackTypes.h"
#include "../UI/HSRUIManagerSubsystem.h"

AHSRPlayerController::AHSRPlayerController()
{
	// PlayerController input, including Enhanced Input action evaluation, is
	// processed through the controller's per-frame player-input tick.
	PrimaryActorTick.bCanEverTick = true;
	CurrentControlMode = EHSRPlayerControlMode::Exploration;
	bControlModeApplied = false;
	bExplorationContextAdded = false;
	bFrontendNavigationContextAdded = false;
	bInputSystemReady = false;
	AppliedInputIntent = EHSRUIInputIntent::GameOnly;
}

void AHSRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	bInputSystemReady = true;
	AddFrontendNavigationContext();
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
		Enhanced && ShouldBindFrontendInputComponent(FrontendBindingsInputComponent, InputComponent))
	{
		const auto BindStarted = [Enhanced, this](UInputAction* Action, void (AHSRPlayerController::*Handler)())
		{
			if (Action) Enhanced->BindAction(Action, ETriggerEvent::Started, this, Handler);
			else UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend action is not configured; binding skipped"));
		};
		BindStarted(PauseBackAction, &ThisClass::HandlePauseBack);
		BindStarted(InventoryAction, &ThisClass::HandleInventory);
		BindStarted(PartyAction, &ThisClass::HandleParty);
		BindStarted(MapAction, &ThisClass::HandleMap);
		BindStarted(ChallengeAction, &ThisClass::HandleChallenge);
		BindStarted(CloseToRootAction, &ThisClass::HandleCloseToRoot);
		FrontendBindingsInputComponent = InputComponent;
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetupInputComponent - PlayerInput=%s InputComponent=%s"),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("None"),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	if (IsLocalPlayerController() && CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}
}

void AHSRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bControlModeApplied = false;

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - Controller=%s Local=%s Pawn=%s"),
		*GetName(),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - PlayerInput=%s InputComponent=%s"),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("None"),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));
	SetControlMode(EHSRPlayerControlMode::Exploration);
}

void AHSRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::OnPossess - InPawn is nullptr"));
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(InPawn);
	if (!ExplorationChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::OnPossess - Possessed Pawn is not AHSRExplorationCharacter: %s"), *InPawn->GetName());
	}

	if (ExplorationChar && ExplorationMappingContext)
	{
		for (const UInputAction* BoundAction : { ExplorationChar->GetMoveAction(), ExplorationChar->GetLookAction(),
			ExplorationChar->GetJumpAction(), ExplorationChar->GetInteractAction() })
		{
			const bool bFoundInContext = ExplorationMappingContext->GetMappings().ContainsByPredicate(
				[BoundAction](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == BoundAction; });
			UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::OnPossess - BoundAction=%s FoundInContext=%s"),
				BoundAction ? *BoundAction->GetPathName() : TEXT("None"),
				bFoundInContext ? TEXT("true") : TEXT("false"));
		}
	}

	// SetupInputComponent owns the initial add, matching the UE 5.6 templates.
	if (bInputSystemReady && CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}

	// Refresh HUD interaction observation if HUD already exists
	if (AHSRHUD* HSRHUD = Cast<AHSRHUD>(GetHUD()))
	{
		HSRHUD->RefreshInteractionObserver();
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::OnPossess - Controller=%s Pawn=%s"),
		*GetName(), *InPawn->GetName());
}

void AHSRPlayerController::OnUnPossess()
{
	RemoveExplorationContext();

	// Clear HUD interaction observation before Super clears pawn reference
	if (AHSRHUD* HSRHUD = Cast<AHSRHUD>(GetHUD()))
	{
		HSRHUD->ClearInteractionObserverInstance();
	}

	Super::OnUnPossess();
}

void AHSRPlayerController::SetControlMode(EHSRPlayerControlMode NewMode)
{
	FHSRInputModePolicy Policy;
	Policy.InputIntent = NewMode == EHSRPlayerControlMode::UIOnly ? EHSRUIInputIntent::UIOnly : EHSRUIInputIntent::GameOnly;
	Policy.bShowMouseCursor = NewMode == EHSRPlayerControlMode::UIOnly;
	ApplyUIInputPolicy(Policy, NewMode);
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetControlMode - Applied mode %d"),
		static_cast<uint8>(CurrentControlMode));
}

bool AHSRPlayerController::ApplyUIInputPolicy(const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
	if (!IsLocalPlayerController())
	{
		return false;
	}
	if (bControlModeApplied && CurrentControlMode == SemanticMode
		&& AppliedInputIntent == Policy.InputIntent && bShowMouseCursor == Policy.bShowMouseCursor)
	{
		return true;
	}
	if (bControlModeApplied)
	{
		RemoveExplorationContext();
	}

	switch (Policy.InputIntent)
	{
	case EHSRUIInputIntent::GameOnly:
		SetInputMode(FInputModeGameOnly());
		break;
	case EHSRUIInputIntent::UIOnly:
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		break;
	}
	case EHSRUIInputIntent::GameAndUI:
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		break;
	}
	default:
		return false;
	}

	CurrentControlMode = SemanticMode;
	AppliedInputIntent = Policy.InputIntent;
	bShowMouseCursor = Policy.bShowMouseCursor;
	if (SemanticMode == EHSRPlayerControlMode::Exploration && bInputSystemReady)
	{
		AddExplorationContext();
	}
	bControlModeApplied = true;
	return true;
}

void AHSRPlayerController::RequestOpenPauseScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenPauseScreen();
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 RequestOpen Result=%d Stack=%d HasPause=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::RequestCloseFrontendToRoot()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
			Manager->CloseFrontendToRoot();
}

void AHSRPlayerController::HandlePauseBack()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			if (Manager->GetLogicalScreenCount() <= 1) Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub);
			else Manager->RequestBack();
		}
}

void AHSRPlayerController::HandleInventory() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Inventory); }
void AHSRPlayerController::HandleParty() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Party); }
void AHSRPlayerController::HandleMap() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Map); }
void AHSRPlayerController::HandleChallenge() { if (ULocalPlayer* LP = GetLocalPlayer()) if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>()) M->OpenFrontendModule(EHSRFrontendModule::Challenge); }
void AHSRPlayerController::HandleCloseToRoot() { RequestCloseFrontendToRoot(); }

void AHSRPlayerController::AddFrontendNavigationContext()
{
	if (bFrontendNavigationContextAdded || !FrontendNavigationMappingContext) return;
	if (ULocalPlayer* LP = GetLocalPlayer())
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			FModifyContextOptions Options;
			Options.bForceImmediately = true;
			Subsystem->AddMappingContext(FrontendNavigationMappingContext, 10, Options);
			bFrontendNavigationContextAdded = true;
		}
}

void AHSRPlayerController::RequestBackScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->RequestBack();
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 RequestBack Result=%d Stack=%d HasPause=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::RequestOpenCharacterDetailScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenFrontendModule(EHSRFrontendModule::Character);
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail RequestOpen Result=%d Stack=%d HasDetail=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenCharacterDetailScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::RequestOpenInventoryScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenFrontendModule(EHSRFrontendModule::Inventory);
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory RequestOpen Result=%d Stack=%d HasInventory=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenInventoryScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

void AHSRPlayerController::AddExplorationContext()
{
	if (bExplorationContextAdded)
	{
		return;
	}

	if (!ExplorationMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - ExplorationMappingContext is not set"));
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - No LocalPlayer"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - No EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	Subsystem->AddMappingContext(ExplorationMappingContext, 0, Options);
	bExplorationContextAdded = true;
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - Added %s"),
		*ExplorationMappingContext->GetName());

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - HasContext=%s"),
		Subsystem->HasMappingContext(ExplorationMappingContext) ? TEXT("true") : TEXT("false"));

	for (const FEnhancedActionKeyMapping& Mapping : ExplorationMappingContext->GetMappings())
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - Mapping Action=%s Key=%s"),
			Mapping.Action ? *Mapping.Action->GetPathName() : TEXT("None"),
			*Mapping.Key.ToString());
	}
}

void AHSRPlayerController::RemoveExplorationContext()
{
	if (!bExplorationContextAdded)
	{
		return;
	}

	if (!ExplorationMappingContext)
	{
		bExplorationContextAdded = false;
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		bExplorationContextAdded = false;
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		bExplorationContextAdded = false;
		return;
	}

	Subsystem->RemoveMappingContext(ExplorationMappingContext);
	bExplorationContextAdded = false;
}
