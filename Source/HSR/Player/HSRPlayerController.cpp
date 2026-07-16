#include "HSRPlayerController.h"
#include "../Character/HSRExplorationCharacter.h"

AHSRPlayerController::AHSRPlayerController()
{
	PrimaryActorTick.bCanEverTick = false;
	CurrentControlMode = EHSRPlayerControlMode::Exploration;
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
}

void AHSRPlayerController::OnUnPossess()
{
	Super::OnUnPossess();
}

void AHSRPlayerController::SetControlMode(EHSRPlayerControlMode NewMode)
{
	if (CurrentControlMode == NewMode)
	{
		return;
	}

	CurrentControlMode = NewMode;

	switch (NewMode)
	{
	case EHSRPlayerControlMode::Exploration:
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
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
}