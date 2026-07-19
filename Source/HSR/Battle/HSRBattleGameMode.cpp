#include "HSRBattleGameMode.h"
#include "HSRBattleCoordinator.h"
#include "HSRTurnManager.h"
#include "HSRBattleTransitionSubsystem.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "../GAS/Ability/HSRBasicAttackAbility.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"

#if WITH_EDITOR
namespace HSRBattleDevelopmentTest
{
	static void LogCase(const TCHAR* CaseName, bool bPassed)
	{
		if (bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("P5-002 TurnTest Case=%s Result=PASS"), CaseName);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P5-002 TurnTest Case=%s Result=FAIL"), CaseName);
		}
	}

	static bool SetSpeed(const FHSRBattleParticipant& Participant, float Speed)
	{
		if (!Participant.AbilitySystemComponent.IsValid())
		{
			return false;
		}
		Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Speed);
		return true;
	}

	static bool SetHealth(const FHSRBattleParticipant& Participant, float Health, float MaxHealth)
	{
		if (!Participant.AbilitySystemComponent.IsValid())
		{
			return false;
		}
		Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), MaxHealth);
		Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), Health);
		return true;
	}

	static float GetHealth(const FHSRBattleParticipant& Participant)
	{
		return Participant.AbilitySystemComponent.IsValid() ? Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) : -1.0f;
	}

	static UHSRBasicAttackAbility* FindBasicAttackAbility(const FHSRBattleParticipant& Participant)
	{
		if (!Participant.AbilitySystemComponent.IsValid())
		{
			return nullptr;
		}
		FGameplayAbilitySpec* AbilitySpec = Participant.AbilitySystemComponent->FindAbilitySpecFromClass(UHSRBasicAttackAbility::StaticClass());
		return AbilitySpec ? Cast<UHSRBasicAttackAbility>(AbilitySpec->GetPrimaryInstance()) : nullptr;
	}

	static void Run(UHSRBattleCoordinator* Coordinator)
	{
		if (!Coordinator || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Error, TEXT("P5-002 TurnTest Harness=SKIPPED reason=missing-coordinator-or-participants"));
			return;
		}

		const TArray<FHSRBattleParticipant>& Participants = Coordinator->GetParticipants();
		const bool bSpeedSetup = SetSpeed(Participants[0], 120.0f) && SetSpeed(Participants[1], 80.0f);
		UHSRTurnManager* TestManager = NewObject<UHSRTurnManager>(Coordinator);
		const bool bDifferentSpeed = bSpeedSetup && TestManager->Initialize(Participants) && TestManager->GetCurrentParticipantId() == FName(TEXT("Player"));
		LogCase(TEXT("DifferentSpeed_PlayerFirst"), bDifferentSpeed);

		const bool bNonCurrentRejected = !TestManager->ResolveAction(FName(TEXT("Enemy"))) && TestManager->GetCurrentParticipantId() == FName(TEXT("Player"));
		LogCase(TEXT("NonCurrentRejected"), bNonCurrentRejected);

		const bool bLegalResolved = TestManager->ResolveAction(FName(TEXT("Player"))) && TestManager->GetCurrentParticipantId() == FName(TEXT("Enemy"));
		LogCase(TEXT("LegalActionResolvedOnce"), bLegalResolved);

		const bool bDuplicateRejected = !TestManager->ResolveAction(FName(TEXT("Player"))) && TestManager->GetCurrentParticipantId() == FName(TEXT("Enemy"));
		LogCase(TEXT("DuplicateRejected_NoExtraAdvance"), bDuplicateRejected);

		const bool bTieSetup = SetSpeed(Participants[0], 100.0f) && SetSpeed(Participants[1], 100.0f);
		const bool bTieBreak = bTieSetup && TestManager->Initialize(Participants) && TestManager->GetCurrentParticipantId() == FName(TEXT("Enemy"));
		LogCase(TEXT("SameSpeed_StableParticipantIdTieBreak"), bTieBreak);

		SetSpeed(Participants[0], 120.0f);
		SetSpeed(Participants[1], 80.0f);
		const bool bInvalidSetup = TestManager->Initialize(Participants) && TestManager->GetCurrentParticipantId() == FName(TEXT("Player")) && TestManager->InvalidateCurrentParticipantForDevelopmentTest();
		const bool bInvalidRejectedAndAdvanced = bInvalidSetup && !TestManager->ResolveAction(FName(TEXT("Player"))) && TestManager->GetCurrentParticipantId() == FName(TEXT("Enemy"));
		LogCase(TEXT("InvalidActorRejected_Advanced"), bInvalidRejectedAndAdvanced);

		TestManager->Reset();
		const bool bResetSafe = !TestManager->ResolveAction(FName(TEXT("Player"))) && TestManager->GetState() == EHSRTurnManagerState::Waiting;
		LogCase(TEXT("ResetSafe"), bResetSafe);
		const bool bEmptySafe = !TestManager->Initialize(TArray<FHSRBattleParticipant>()) && TestManager->GetState() == EHSRTurnManagerState::Waiting;
		LogCase(TEXT("EmptyQueueSafe"), bEmptySafe);

		const bool bRestoreMainQueue = Coordinator->GetTurnManager() && Coordinator->GetTurnManager()->Initialize(Participants);
		LogCase(TEXT("RestoreBattleQueue"), bRestoreMainQueue);
		UE_LOG(LogTemp, Log, TEXT("P5-002 TurnTest Harness=COMPLETE"));

		UHSRTurnManager* MainTurnManager = Coordinator->GetTurnManager();
		const bool bAttackSetup = bRestoreMainQueue
			&& SetSpeed(Participants[0], 120.0f) && SetSpeed(Participants[1], 80.0f)
			&& SetHealth(Participants[0], 100.0f, 100.0f) && SetHealth(Participants[1], 100.0f, 100.0f)
			&& MainTurnManager->Initialize(Participants)
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"));
		int32 ActionResolvedCount = 0;
		const FDelegateHandle ActionResolvedHandle = MainTurnManager->OnActionResolved().AddLambda([&ActionResolvedCount](FName) { ++ActionResolvedCount; });
		LogCase(TEXT("P5-003_AttackSetup_PlayerCurrent"), bAttackSetup);

		const float PlayerHealthBeforeRejected = GetHealth(Participants[0]);
		const float EnemyHealthBeforeRejected = GetHealth(Participants[1]);
		const bool bAttackNonCurrentRejected = !Coordinator->RequestBasicAttack(FName(TEXT("Enemy")), FName(TEXT("Player")))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"))
			&& FMath::IsNearlyEqual(PlayerHealthBeforeRejected, GetHealth(Participants[0]))
			&& FMath::IsNearlyEqual(EnemyHealthBeforeRejected, GetHealth(Participants[1]))
			&& ActionResolvedCount == 0;
		LogCase(TEXT("P5-003_NonCurrentRejected_NoMutation"), bAttackNonCurrentRejected);

		const bool bInvalidTargetRejected = !Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Player")))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"))
			&& FMath::IsNearlyEqual(PlayerHealthBeforeRejected, GetHealth(Participants[0]))
			&& FMath::IsNearlyEqual(EnemyHealthBeforeRejected, GetHealth(Participants[1]))
			&& ActionResolvedCount == 0;
		LogCase(TEXT("P5-003_InvalidTargetRejected_NoMutation"), bInvalidTargetRejected);

		UHSRBasicAttackAbility* PlayerAttack = FindBasicAttackAbility(Participants[0]);
		const TSoftClassPtr<UGameplayEffect> OriginalEffectClass = PlayerAttack ? PlayerAttack->GetDamageEffectClassForDevelopmentTest() : nullptr;
		if (PlayerAttack)
		{
			PlayerAttack->SetDamageEffectClassForDevelopmentTest(TSoftClassPtr<UGameplayEffect>(FSoftObjectPath(TEXT("/Game/GameplayEffects/GE_P5_MissingDamageEffect.GE_P5_MissingDamageEffect_C"))));
		}
		const bool bMissingEffectRejected = PlayerAttack && !Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Enemy")))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"))
			&& FMath::IsNearlyEqual(PlayerHealthBeforeRejected, GetHealth(Participants[0]))
			&& FMath::IsNearlyEqual(EnemyHealthBeforeRejected, GetHealth(Participants[1]))
			&& ActionResolvedCount == 0;
		if (PlayerAttack)
		{
			PlayerAttack->SetDamageEffectClassForDevelopmentTest(OriginalEffectClass);
		}
		LogCase(TEXT("P5-003_MissingEffectRejected_NoMutation"), bMissingEffectRejected);

		const bool bLegalAttack = Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Enemy")))
			&& FMath::IsNearlyEqual(GetHealth(Participants[1]), 90.0f)
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Enemy"))
			&& ActionResolvedCount == 1;
		LogCase(TEXT("P5-003_LegalFixedDamageAndSingleResolve"), bLegalAttack);

		const float EnemyHealthAfterLegal = GetHealth(Participants[1]);
		const bool bAttackDuplicateRejected = !Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Enemy")))
			&& FMath::IsNearlyEqual(EnemyHealthAfterLegal, GetHealth(Participants[1]))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Enemy"))
			&& ActionResolvedCount == 1;
		LogCase(TEXT("P5-003_DuplicateRejected_NoExtraMutation"), bAttackDuplicateRejected);
		MainTurnManager->OnActionResolved().Remove(ActionResolvedHandle);
		UE_LOG(LogTemp, Log, TEXT("P5-003 AttackTest Harness=COMPLETE"));
	}
}
#endif

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

	Coordinator->OnBattleResultReady().AddUObject(this, &AHSRBattleGameMode::HandleBattleResultReady);

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

#if WITH_EDITOR
	HSRBattleDevelopmentTest::Run(Coordinator);
	RunTerminalScenarioForDevelopment();
#endif
}

void AHSRBattleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Coordinator)
	{
		Coordinator->Reset();
	}
	Coordinator = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AHSRBattleGameMode::HandleBattleResultReady(const FHSRBattleResult& Result)
{
	if (!Coordinator || !Result.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRBattleGameMode::HandleBattleResultReady - REJECTED invalid result"));
		return;
	}

	FHSRBattleResult ConsumedResult;
	if (!Coordinator->ConsumeBattleResult(ConsumedResult))
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRBattleGameMode::HandleBattleResultReady - REJECTED duplicate result RequestId=%s"), *Result.RequestId.ToString());
		return;
	}

	UHSRBattleTransitionSubsystem* Subsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("AHSRBattleGameMode::HandleBattleResultReady - FAILED missing transition subsystem"));
		return;
	}

	const FHSRExplorationReturnResult ReturnResult = Subsystem->RequestBattleReturn(ConsumedResult.ReturnContext);
	if (ReturnResult.ResultType == EHSREncounterReturnResultType::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRBattleGameMode::HandleBattleResultReady - Return request type=%d Outcome=%d RequestId=%s"),
			static_cast<int32>(ReturnResult.ResultType), static_cast<int32>(ConsumedResult.Outcome), *ConsumedResult.RequestId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRBattleGameMode::HandleBattleResultReady - Return request type=%d Outcome=%d RequestId=%s"),
			static_cast<int32>(ReturnResult.ResultType), static_cast<int32>(ConsumedResult.Outcome), *ConsumedResult.RequestId.ToString());
	}
}

void AHSRBattleGameMode::RunTerminalScenarioForDevelopment()
{
#if WITH_EDITOR
	if (TerminalTestScenario == EHSRP5TerminalTestScenario::None || !Coordinator || Coordinator->GetParticipants().Num() != 2)
	{
		return;
	}

	const TArray<FHSRBattleParticipant>& Participants = Coordinator->GetParticipants();
	const FHSRBattleParticipant* Player = Participants.FindByPredicate([](const FHSRBattleParticipant& Participant) { return Participant.Team == EHSRBattleParticipantTeam::Player; });
	const FHSRBattleParticipant* Enemy = Participants.FindByPredicate([](const FHSRBattleParticipant& Participant) { return Participant.Team == EHSRBattleParticipantTeam::Enemy; });
	if (!Player || !Enemy || !Player->AbilitySystemComponent.IsValid() || !Enemy->AbilitySystemComponent.IsValid() || !Coordinator->GetTurnManager())
	{
		UE_LOG(LogTemp, Error, TEXT("P5-004 DeathTest Harness=SKIPPED reason=missing-valid-participants"));
		return;
	}

	Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
	Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
	Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
	Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
	Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
	Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
	Coordinator->GetTurnManager()->Initialize(Participants);

	if (TerminalTestScenario == EHSRP5TerminalTestScenario::PlayerVictory)
	{
		Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 10.0f);
		const bool bAttackSucceeded = Coordinator->RequestBasicAttack(Player->ParticipantId, Enemy->ParticipantId);
		if (bAttackSucceeded)
		{
			UE_LOG(LogTemp, Log, TEXT("P5-004 DeathTest Case=PlayerVictory Result=PASS"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P5-004 DeathTest Case=PlayerVictory Result=FAIL"));
		}
	}
	else
	{
		Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		const bool bFinished = Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Finished;
		if (bFinished)
		{
			UE_LOG(LogTemp, Log, TEXT("P5-004 DeathTest Case=PlayerDefeat Result=PASS"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P5-004 DeathTest Case=PlayerDefeat Result=FAIL"));
		}
	}
#endif
}
