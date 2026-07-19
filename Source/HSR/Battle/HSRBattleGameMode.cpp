#include "HSRBattleGameMode.h"
#include "HSRBattleCoordinator.h"
#include "HSRBattleTransitionSubsystem.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"

AHSRBattleGameMode::AHSRBattleGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AHSRBattleGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Create the Coordinator (UObject, owned by this GameMode)
	Coordinator = NewObject<UHSRBattleCoordinator>(this);
	if (!Coordinator)
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleGameMode::BeginPlay - FAILED to create Coordinator"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRBattleGameMode::BeginPlay - Coordinator created"));

	// Access the Transition Subsystem to consume the pending encounter
	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleGameMode::BeginPlay - FAILED: Cannot access UHSRBattleTransitionSubsystem"));
		return;
	}

	// Consume the pending encounter exactly once
	FHSREncounterResult ConsumeResult = Subsystem->ConsumePendingEncounter();
	if (ConsumeResult.ResultType != EHSREncounterResultType::Success)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("AHSRBattleGameMode::BeginPlay - No pending encounter to consume (type=%d). Battle map may have been opened manually."),
			static_cast<int32>(ConsumeResult.ResultType));
		return;
	}

	// Submit the consumed request to the Coordinator (exactly-once validation)
	bool bSubmitResult = Coordinator->SubmitBattleRequest(ConsumeResult.ConsumedRequest);
	if (!bSubmitResult)
	{
		UE_LOG(LogTemp, Error,
			TEXT("AHSRBattleGameMode::BeginPlay - SubmitBattleRequest FAILED (RequestId=%s)"),
			*ConsumeResult.RequestId.ToString());
		return;
	}

	UE_LOG(LogTemp, Log,
		TEXT("AHSRBattleGameMode::BeginPlay - SubmitBattleRequest SUCCESS RequestId=%s"),
		*Coordinator->GetCurrentRequestId().ToString());

	// Build participants in the Battle World
	FHSRBattleInitResult BuildResult = Coordinator->BuildParticipants(GetWorld());
	if (!BuildResult.IsSuccess())
	{
		UE_LOG(LogTemp, Error,
			TEXT("AHSRBattleGameMode::BeginPlay - BuildParticipants FAILED: %s (Type=%d DefId=%s)"),
			*BuildResult.Message.ToString(), static_cast<int32>(BuildResult.FailureType), *BuildResult.TargetDefinitionId.ToString());
		return;
	}

	// Log final state summary
	const int32 NumParticipants = Coordinator->GetParticipants().Num();
	const FName ExplorationMap = Coordinator->GetReturnContext().ExplorationMapPath;
	UE_LOG(LogTemp, Log,
		TEXT("AHSRBattleGameMode::BeginPlay - COMPLETE CoordinatorState=%d Participants=%d ReturnMap=%s"),
		static_cast<int32>(Coordinator->GetCurrentState()),
		NumParticipants,
		*ExplorationMap.ToString());

	if (NumParticipants >= 2)
	{
		const auto& Participants = Coordinator->GetParticipants();
		for (int32 i = 0; i < Participants.Num(); ++i)
		{
			UE_LOG(LogTemp, Log,
				TEXT("  Participant[%d]: Id=%s DefId=%s Team=%d Actor=%s ASC=%s Valid=%d"),
				i,
				*Participants[i].ParticipantId.ToString(),
				*Participants[i].DefinitionId.ToString(),
				static_cast<int32>(Participants[i].Team),
				Participants[i].Actor.IsValid() ? *Participants[i].Actor->GetName() : TEXT("null"),
				Participants[i].AbilitySystemComponent.IsValid() ? *Participants[i].AbilitySystemComponent->GetName() : TEXT("null"),
				Participants[i].IsValid() ? 1 : 0);
		}
	}
}

void AHSRBattleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Coordinator = nullptr;
	Super::EndPlay(EndPlayReason);
}
