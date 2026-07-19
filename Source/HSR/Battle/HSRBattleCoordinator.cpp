#include "HSRBattleCoordinator.h"
#include "HSREncounterTypes.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

FHSRBattleResult UHSRBattleCoordinator::SubmitBattleRequest(const FHSREncounterRequest& InRequest)
{
	if (CurrentState != EHSRBattleCoordinatorState::Idle)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED state=%d RequestId=%s (expected Idle)"),
			static_cast<int32>(CurrentState), *InRequest.RequestId.ToString());
		return FHSRBattleResult::MakeFailure(
			FText::Format(NSLOCTEXT("HSRBattle", "CoordinatorNotIdle", "Coordinator is not Idle (state={0})"),
				FText::AsNumber(static_cast<int32>(CurrentState))));
	}

	if (!InRequest.RequestId.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED invalid RequestId"));
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("Invalid RequestId in encounter request.")));
	}

	if (InRequest.EncounterId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED EncounterId=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("EncounterId is None.")), InRequest.RequestId);
	}

	if (InRequest.EnemyDefinitionId.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED EnemyDefinitionId=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("EnemyDefinitionId is None.")), InRequest.RequestId);
	}

	if (InRequest.BattleMapPath.IsNone())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SubmitBattleRequest - REJECTED BattleMapPath=None RequestId=%s"),
			*InRequest.RequestId.ToString());
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("BattleMapPath is None.")), InRequest.RequestId);
	}

	// Build pure-value ReturnContext from the consumed request
	FHSRBattleReturnContext RetCtx;
	RetCtx.RequestId = InRequest.RequestId;
	RetCtx.ExplorationMapPath = InRequest.ExplorationMapPath;
	RetCtx.ReturnTransform = InRequest.ReturnTransform;

	// Capture state atomically
	CurrentRequestId = InRequest.RequestId;
	CurrentEncounterId = InRequest.EncounterId;
	CurrentEnemyDefinitionId = InRequest.EnemyDefinitionId;
	ReturnContext = RetCtx;
	CurrentState = EHSRBattleCoordinatorState::Consuming;

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::SubmitBattleRequest - SUCCESS RequestId=%s EncounterId=%s EnemyDefId=%s BattleMap=%s ExplorationMap=%s"),
		*InRequest.RequestId.ToString(), *InRequest.EncounterId.ToString(),
		*InRequest.EnemyDefinitionId.ToString(), *InRequest.BattleMapPath.ToString(),
		*InRequest.ExplorationMapPath.ToString());

	return FHSRBattleResult::MakeSuccess(InRequest.RequestId);
}

FHSRBattleResult UHSRBattleCoordinator::BuildParticipants(UWorld* BattleWorld)
{
	if (CurrentState != EHSRBattleCoordinatorState::Consuming)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::BuildParticipants - REJECTED state=%d RequestId=%s (expected Consuming)"),
			static_cast<int32>(CurrentState), *CurrentRequestId.ToString());
		return FHSRBattleResult::MakeFailure(
			FText::Format(NSLOCTEXT("HSRBattle", "BuildNotConsuming", "Coordinator is not in Consuming state (state={0})"),
				FText::AsNumber(static_cast<int32>(CurrentState))),
			CurrentRequestId);
	}

	if (!BattleWorld)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED BattleWorld=null RequestId=%s EncounterId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString());
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("Battle World is null.")), CurrentRequestId);
	}

	// Clear previous participants
	Participants.Empty();

	// --- Spawn player participant ---
	AActor* PlayerActor = SpawnParticipantActor(BattleWorld, EHSRBattleParticipantTeam::Player);
	if (!PlayerActor)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED spawn PlayerActor RequestId=%s EncounterId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString());
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("Failed to spawn player participant actor.")), CurrentRequestId);
	}

	if (!InitParticipantASC(PlayerActor))
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED InitASC PlayerActor RequestId=%s EncounterId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString());
		PlayerActor->Destroy();
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("Failed to initialize ASC on player participant.")), CurrentRequestId);
	}

	FHSRBattleParticipant PlayerParticipant;
	PlayerParticipant.ParticipantId = FName(TEXT("Player"));
	PlayerParticipant.DefinitionId = CurrentEncounterId; // Player uses EncounterId as definition anchor
	PlayerParticipant.Team = EHSRBattleParticipantTeam::Player;
	PlayerParticipant.Actor = PlayerActor;
	PlayerParticipant.AbilitySystemComponent = PlayerActor->FindComponentByClass<UAbilitySystemComponent>();
	Participants.Add(PlayerParticipant);

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildParticipants - Player spawned Actor=%s ASC=%s"),
		*PlayerActor->GetName(), PlayerParticipant.AbilitySystemComponent.IsValid() ? *PlayerParticipant.AbilitySystemComponent->GetName() : TEXT("null"));

	// --- Spawn enemy participant ---
	AActor* EnemyActor = SpawnParticipantActor(BattleWorld, EHSRBattleParticipantTeam::Enemy);
	if (!EnemyActor)
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED spawn EnemyActor RequestId=%s EncounterId=%s EnemyDefId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString(), *CurrentEnemyDefinitionId.ToString());
		// Cleanup player
		PlayerActor->Destroy();
		Participants.Empty();
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("Failed to spawn enemy participant actor.")), CurrentRequestId);
	}

	if (!InitParticipantASC(EnemyActor))
	{
		UE_LOG(LogTemp, Error,
			TEXT("UHSRBattleCoordinator::BuildParticipants - FAILED InitASC EnemyActor RequestId=%s EncounterId=%s EnemyDefId=%s"),
			*CurrentRequestId.ToString(), *CurrentEncounterId.ToString(), *CurrentEnemyDefinitionId.ToString());
		EnemyActor->Destroy();
		PlayerActor->Destroy();
		Participants.Empty();
		CurrentState = EHSRBattleCoordinatorState::Failed;
		return FHSRBattleResult::MakeFailure(
			FText::FromString(TEXT("Failed to initialize ASC on enemy participant.")), CurrentRequestId);
	}

	FHSRBattleParticipant EnemyParticipant;
	EnemyParticipant.ParticipantId = FName(TEXT("Enemy"));
	EnemyParticipant.DefinitionId = CurrentEnemyDefinitionId;
	EnemyParticipant.Team = EHSRBattleParticipantTeam::Enemy;
	EnemyParticipant.Actor = EnemyActor;
	EnemyParticipant.AbilitySystemComponent = EnemyActor->FindComponentByClass<UAbilitySystemComponent>();
	Participants.Add(EnemyParticipant);

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildParticipants - Enemy spawned Actor=%s ASC=%s"),
		*EnemyActor->GetName(), EnemyParticipant.AbilitySystemComponent.IsValid() ? *EnemyParticipant.AbilitySystemComponent->GetName() : TEXT("null"));

	// Atomically transition to Spawned
	CurrentState = EHSRBattleCoordinatorState::Spawned;

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::BuildParticipants - SUCCESS RequestId=%s EncounterId=%s EnemyDefId=%s Participants=%d"),
		*CurrentRequestId.ToString(), *CurrentEncounterId.ToString(),
		*CurrentEnemyDefinitionId.ToString(), Participants.Num());

	return FHSRBattleResult::MakeSuccess(CurrentRequestId);
}

void UHSRBattleCoordinator::Reset()
{
	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::Reset - Clearing state. Previous state=%d RequestId=%s"),
		static_cast<int32>(CurrentState), *CurrentRequestId.ToString());

	CurrentState = EHSRBattleCoordinatorState::Idle;
	CurrentRequestId = FGuid();
	CurrentEncounterId = NAME_None;
	CurrentEnemyDefinitionId = NAME_None;
	ReturnContext = FHSRBattleReturnContext();
	Participants.Empty();
}

AActor* UHSRBattleCoordinator::SpawnParticipantActor(UWorld* World, EHSRBattleParticipantTeam Team)
{
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::SpawnParticipantActor - World is null"));
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawn* Pawn = World->SpawnActor<APawn>(APawn::StaticClass(), FTransform::Identity, Params);
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::SpawnParticipantActor - FAILED to spawn APawn for team=%d"),
			static_cast<int32>(Team));
		return nullptr;
	}

	Pawn->SetActorLabel(Team == EHSRBattleParticipantTeam::Player ? TEXT("BattlePlayerPawn") : TEXT("BattleEnemyPawn"));

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::SpawnParticipantActor - SUCCESS Actor=%s Team=%d"),
		*Pawn->GetName(), static_cast<int32>(Team));

	return Pawn;
}

bool UHSRBattleCoordinator::InitParticipantASC(AActor* TargetActor)
{
	if (!TargetActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRBattleCoordinator::InitParticipantASC - TargetActor is null"));
		return false;
	}

	// Add UHSRAbilitySystemComponent at runtime (UE5.0+ AddComponentByClass)
	UHSRAbilitySystemComponent* ASC = Cast<UHSRAbilitySystemComponent>(
		TargetActor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false));

	if (!ASC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - Failed to add ASC component to Actor=%s"),
			*TargetActor->GetName());
		return false;
	}

	// Configure ASC: single-player, no tick
	ASC->SetIsReplicated(false);
	ASC->SetComponentTickEnabled(false);
	// ASC->RegisterComponent() ?? auto-registered by AddComponentByClass

	// Register CoreAttributeSet via InitStats
	const UAttributeSet* AttrSet = ASC->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr);
	if (!AttrSet)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - InitStats failed for Actor=%s"),
			*TargetActor->GetName());
		ASC->DestroyComponent(true);
		return false;
	}

	// InitAbilityActorInfo with Owner=Avatar=self
	ASC->InitAbilityActorInfo(TargetActor, TargetActor);

	if (!ASC->AbilityActorInfo.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("UHSRBattleCoordinator::InitParticipantASC - ActorInfo invalid after Init for Actor=%s"),
			*TargetActor->GetName());
		ASC->DestroyComponent(true);
		return false;
	}

	UE_LOG(LogTemp, Log,
		TEXT("UHSRBattleCoordinator::InitParticipantASC - SUCCESS Actor=%s ASC=%s ActorInfo valid, Owner=Avatar=self"),
		*TargetActor->GetName(), *ASC->GetName());

	return true;
}
