#include "HSRPlayerController.h"
#include "../Character/HSRExplorationCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"

AHSRPlayerController::AHSRPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentControlMode = EHSRPlayerControlMode::Exploration;
	bControlModeApplied = false;
	bExplorationContextAdded = false;
}

void AHSRPlayerController::BeginPlay()
{
	Super::BeginPlay();
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

	// Ensure context is added if in Exploration mode (LocalPlayer may not be ready in BeginPlay)
	if (CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}
}

void AHSRPlayerController::OnUnPossess()
{
	RemoveExplorationContext();
	Super::OnUnPossess();
}

void AHSRPlayerController::SetControlMode(EHSRPlayerControlMode NewMode)
{
	// Power-identity: first call always applies even if mode matches default
	if (bControlModeApplied && CurrentControlMode == NewMode)
	{
		return;
	}

	// Remove old mode's context before switching
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
		AddExplorationContext();
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

	Subsystem->AddMappingContext(ExplorationMappingContext, 0);
	bExplorationContextAdded = true;
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
