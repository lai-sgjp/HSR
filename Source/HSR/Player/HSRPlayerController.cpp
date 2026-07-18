#include "HSRPlayerController.h"
#include "../UI/HSRHUD.h"
#include "../Character/HSRExplorationCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

AHSRPlayerController::AHSRPlayerController()
{
	// PlayerController input, including Enhanced Input action evaluation, is
	// processed through the controller's per-frame player-input tick.
	PrimaryActorTick.bCanEverTick = true;
	CurrentControlMode = EHSRPlayerControlMode::Exploration;
	bControlModeApplied = false;
	bExplorationContextAdded = false;
	bInputSystemReady = false;
}

void AHSRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	bInputSystemReady = true;

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
	Super::OnUnPossess();
}

void AHSRPlayerController::SetControlMode(EHSRPlayerControlMode NewMode)
{
	if (bControlModeApplied && CurrentControlMode == NewMode)
	{
		return;
	}

	if (bControlModeApplied)
	{
		RemoveExplorationContext();
	}

	CurrentControlMode = NewMode;

	switch (NewMode)
	{
	case EHSRPlayerControlMode::Exploration:
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		if (bInputSystemReady)
		{
			AddExplorationContext();
		}
		break;

	case EHSRPlayerControlMode::UIOnly:
		SetInputMode(FInputModeUIOnly());
		bShowMouseCursor = true;
		break;

	case EHSRPlayerControlMode::Battle:
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetControlMode - Battle mode (placeholder)"));
		break;

	default:
		break;
	}

	bControlModeApplied = true;
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetControlMode - Applied mode %d"),
		static_cast<uint8>(CurrentControlMode));
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
