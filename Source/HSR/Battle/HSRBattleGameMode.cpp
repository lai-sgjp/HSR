#include "HSRBattleGameMode.h"
#include "HSRBattleCoordinator.h"
#include "HSRTurnManager.h"
#include "HSRBattleTransitionSubsystem.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "GameplayAbilitySpec.h"
#include "../GAS/Ability/HSRBasicAttackAbility.h"
#include "../GAS/Ability/HSRSkillAbility.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../Status/HSRStatusComponent.h"
#include "../Data/HSRBreakTypes.h"
#include "../UI/HSRBattleCommandViewModel.h"
#include "../UI/HSRBattleCommandWidget.h"
#include "Blueprint/UserWidget.h"

#if WITH_EDITOR
namespace HSRBattleDevelopmentTest
{
	struct FP8Snapshot
	{
		float Health = 0.0f;
		float Toughness = 0.0f;
		FName Turn = NAME_None;
		int32 SkillPoints = 0;
		int32 Rng = 0;
	};

	static FP8Snapshot CaptureP8Snapshot(const UHSRBattleCoordinator* Coordinator)
	{
		FP8Snapshot Snapshot;
		if (!Coordinator) return Snapshot;
		Snapshot.SkillPoints = Coordinator->GetTeamResourceState().CurrentSkillPoints;
		Snapshot.Rng = Coordinator->GetDevelopmentDamageConsumeCount();
		if (const UHSRTurnManager* TurnManager = Coordinator->GetTurnManager()) Snapshot.Turn = TurnManager->GetCurrentParticipantId();
		for (const FHSRBattleParticipant& Participant : Coordinator->GetParticipants())
		{
			if (Participant.AbilitySystemComponent.IsValid())
			{
				Snapshot.Health = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
				Snapshot.Toughness = Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetToughnessAttribute());
				break;
			}
		}
		return Snapshot;
	}

	static void RunP8ContractHarness(const UHSRBattleCoordinator* Coordinator)
	{
		const auto RunCase = [Coordinator](const TCHAR* Name, EHSRElementToughnessContractResult Actual, EHSRElementToughnessContractResult Expected)
		{
			const FP8Snapshot Before = CaptureP8Snapshot(Coordinator);
			const FP8Snapshot After = CaptureP8Snapshot(Coordinator);
			const bool bNoMutation = Before.Health == After.Health && Before.Toughness == After.Toughness && Before.Turn == After.Turn && Before.SkillPoints == After.SkillPoints && Before.Rng == After.Rng;
			const bool bPass = Actual == Expected && bNoMutation;
			if (bPass)
			{
				UE_LOG(LogTemp, Log, TEXT("P8-001 Contract Case=%s Result=PASS Reason=%d HPBefore=%f HPAfter=%f ToughnessBefore=%f ToughnessAfter=%f TurnBefore=%s TurnAfter=%s ResourceBefore=%d ResourceAfter=%d RNGBefore=%d RNGAfter=%d NoMutation=%d"), Name, static_cast<int32>(Actual), Before.Health, After.Health, Before.Toughness, After.Toughness, *Before.Turn.ToString(), *After.Turn.ToString(), Before.SkillPoints, After.SkillPoints, Before.Rng, After.Rng, bNoMutation ? 1 : 0);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("P8-001 Contract Case=%s Result=FAIL Reason=%d HPBefore=%f HPAfter=%f ToughnessBefore=%f ToughnessAfter=%f TurnBefore=%s TurnAfter=%s ResourceBefore=%d ResourceAfter=%d RNGBefore=%d RNGAfter=%d NoMutation=%d"), Name, static_cast<int32>(Actual), Before.Health, After.Health, Before.Toughness, After.Toughness, *Before.Turn.ToString(), *After.Turn.ToString(), Before.SkillPoints, After.SkillPoints, Before.Rng, After.Rng, bNoMutation ? 1 : 0);
			}
		};
		const FGameplayTag ElementRoot = FGameplayTag::RequestGameplayTag(TEXT("Element"), false);
		const FGameplayTag WeaknessRoot = FGameplayTag::RequestGameplayTag(TEXT("Weakness"), false);
		FGameplayTagContainer ValidWeaknesses; ValidWeaknesses.AddTag(WeaknessRoot);
		FGameplayTagContainer InvalidWeaknesses; InvalidWeaknesses.AddTag(ElementRoot);
		const EHSRElementToughnessContractResult ValidResult = FHSRToughnessConfiguration::ValidateElement(ElementRoot) == EHSRElementToughnessContractResult::Valid && FHSRToughnessConfiguration::ValidateWeaknesses(ValidWeaknesses) == EHSRElementToughnessContractResult::Valid && FHSRToughnessConfiguration::ValidateToughnessDamage(1.0f) == EHSRElementToughnessContractResult::Valid && FHSRToughnessConfiguration::ValidateInitialToughness(1.0f, 1.0f) == EHSRElementToughnessContractResult::Valid ? EHSRElementToughnessContractResult::Valid : EHSRElementToughnessContractResult::InvalidElementTag;
		RunCase(TEXT("ValidContract"), ValidResult, EHSRElementToughnessContractResult::Valid);
		RunCase(TEXT("MissingElement"), FHSRToughnessConfiguration::ValidateElement(FGameplayTag()), EHSRElementToughnessContractResult::MissingElement);
		RunCase(TEXT("EmptyWeakness"), FHSRToughnessConfiguration::ValidateWeaknesses(FGameplayTagContainer()), EHSRElementToughnessContractResult::EmptyWeaknesses);
		RunCase(TEXT("InvalidWeaknessTag"), FHSRToughnessConfiguration::ValidateWeaknesses(InvalidWeaknesses), EHSRElementToughnessContractResult::InvalidWeaknessTag);
		RunCase(TEXT("InvalidToughnessDamage"), FHSRToughnessConfiguration::ValidateToughnessDamage(0.0f), EHSRElementToughnessContractResult::InvalidToughnessDamage);
		RunCase(TEXT("InvalidInitialToughness"), FHSRToughnessConfiguration::ValidateInitialToughness(0.0f, 1.0f), EHSRElementToughnessContractResult::InvalidInitialToughness);
		UE_LOG(LogTemp, Log, TEXT("P8-001 Contract Harness=COMPLETE"));
	}

	struct FP9TurnEventRecorder
	{
		TArray<FHSRTurnLifecycleEvent> Events;

		void Record(const FHSRTurnLifecycleEvent& Event)
		{
			Events.Add(Event);
			UE_LOG(LogTemp, Log, TEXT("P9-000 TurnLifecycle EventType=%d BattleEpoch=%llu ParticipantId=%s TurnSequence=%llu"),
				static_cast<int32>(Event.EventType), Event.BattleEpoch, *Event.ParticipantId.ToString(), Event.TurnSequence);
		}
	};

	static void LogP9Case(const TCHAR* CaseName, bool bPassed, const FP9TurnEventRecorder& Recorder)
	{
		const TCHAR* Result = bPassed ? TEXT("PASS") : TEXT("FAIL");
		if (bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("P9-000 TurnLifecycle Case=%s Result=%s Events=%d"), CaseName, Result, Recorder.Events.Num());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P9-000 TurnLifecycle Case=%s Result=%s Events=%d"), CaseName, Result, Recorder.Events.Num());
		}
	}

	static void BindP9Recorder(UHSRTurnManager* Manager, FP9TurnEventRecorder& Recorder)
	{
		Manager->OnTurnStarted().AddRaw(&Recorder, &FP9TurnEventRecorder::Record);
		Manager->OnTurnEnded().AddRaw(&Recorder, &FP9TurnEventRecorder::Record);
	}

	static bool IsP9Event(const FHSRTurnLifecycleEvent& Event, EHSRTurnLifecycleEventType Type, FName ParticipantId, uint64 Epoch, uint64 Sequence)
	{
		return Event.EventType == Type && Event.ParticipantId == ParticipantId && Event.BattleEpoch == Epoch && Event.TurnSequence == Sequence;
	}

	static void RunP9TurnLifecycleHarness(UHSRBattleCoordinator* Coordinator)
	{
		if (!Coordinator || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Error, TEXT("P9-000 TurnLifecycle Harness=SKIPPED Reason=MissingCoordinatorOrParticipants"));
			return;
		}

		TArray<FHSRBattleParticipant> Participants = Coordinator->GetParticipants();
		if (!Participants[0].AbilitySystemComponent.IsValid() || !Participants[1].AbilitySystemComponent.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("P9-000 TurnLifecycle Harness=SKIPPED Reason=InvalidParticipantASC"));
			return;
		}
		Participants[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
		Participants[1].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
		const FName FirstId = Participants[0].ParticipantId;
		const FName SecondId = Participants[1].ParticipantId;

		UHSRTurnManager* Manager = NewObject<UHSRTurnManager>(Coordinator);
		FP9TurnEventRecorder Recorder;
		BindP9Recorder(Manager, Recorder);
		const bool bInitialized = Manager->Initialize(Participants);
		const uint64 EpochOne = Manager->GetBattleEpoch();
		LogP9Case(TEXT("Initialize_FirstStartedOnly"), bInitialized && EpochOne != 0 && Recorder.Events.Num() == 1
			&& IsP9Event(Recorder.Events[0], EHSRTurnLifecycleEventType::TurnStarted, FirstId, EpochOne, 1), Recorder);

		const int32 BeforeNonCurrent = Recorder.Events.Num();
		LogP9Case(TEXT("NonCurrentResolve_NoEvent"), !Manager->ResolveAction(SecondId) && Recorder.Events.Num() == BeforeNonCurrent, Recorder);
		const bool bLegalResolve = Manager->ResolveAction(FirstId);
		LogP9Case(TEXT("LegalResolve_EndedThenStarted"), bLegalResolve && Recorder.Events.Num() == BeforeNonCurrent + 2
			&& IsP9Event(Recorder.Events[BeforeNonCurrent], EHSRTurnLifecycleEventType::TurnEnded, FirstId, EpochOne, 1)
			&& IsP9Event(Recorder.Events[BeforeNonCurrent + 1], EHSRTurnLifecycleEventType::TurnStarted, SecondId, EpochOne, 2), Recorder);
		const int32 BeforeDuplicate = Recorder.Events.Num();
		LogP9Case(TEXT("DuplicateResolve_NoEvent"), !Manager->ResolveAction(FirstId) && Recorder.Events.Num() == BeforeDuplicate, Recorder);

		const int32 BeforeReset = Recorder.Events.Num();
		Manager->Reset();
		LogP9Case(TEXT("Reset_NoEvent"), Recorder.Events.Num() == BeforeReset && Manager->GetBattleEpoch() == 0
			&& Manager->GetTurnSequence() == 0, Recorder);
		const int32 BeforeResetInitialize = Recorder.Events.Num();
		const bool bSecondInitialize = Manager->Initialize(Participants);
		const uint64 EpochTwo = Manager->GetBattleEpoch();
		LogP9Case(TEXT("Reset_NewEpochSequenceRestart"), bSecondInitialize && EpochTwo != 0 && EpochTwo != EpochOne
			&& Recorder.Events.Num() == BeforeResetInitialize + 1
			&& IsP9Event(Recorder.Events.Last(), EHSRTurnLifecycleEventType::TurnStarted, FirstId, EpochTwo, 1), Recorder);

		Manager->Reset();
		const int32 BeforeInvalid = Recorder.Events.Num();
		Manager->Initialize(Participants);
		const uint64 InvalidEpoch = Manager->GetBattleEpoch();
		const FName InvalidId = Manager->GetCurrentParticipantId();
		const int32 BeforeInvalidResolve = Recorder.Events.Num();
		const bool bInvalidated = Manager->InvalidateCurrentParticipantForDevelopmentTest();
		const bool bInvalidRejected = !Manager->ResolveAction(InvalidId);
		LogP9Case(TEXT("InvalidCurrent_SkippedWithoutEnded"), bInvalidated && bInvalidRejected
			&& Recorder.Events.Num() == BeforeInvalidResolve + 1
			&& IsP9Event(Recorder.Events.Last(), EHSRTurnLifecycleEventType::TurnStarted, SecondId, InvalidEpoch, 2), Recorder);

		const float FirstHealth = Participants[0].AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
		const float FirstMaxHealth = Participants[0].AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
		Manager->Reset();
		Manager->Initialize(Participants);
		const uint64 DefeatedEpoch = Manager->GetBattleEpoch();
		const int32 BeforeDefeatedResolve = Recorder.Events.Num();
		Participants[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		const bool bDefeatedRejected = !Manager->ResolveAction(FirstId);
		const bool bDefeatedCasePassed = bDefeatedRejected && Recorder.Events.Num() == BeforeDefeatedResolve + 1
			&& IsP9Event(Recorder.Events.Last(), EHSRTurnLifecycleEventType::TurnStarted, SecondId, DefeatedEpoch, 2);
		Participants[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), FirstMaxHealth);
		Participants[0].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), FirstHealth);
		LogP9Case(TEXT("DefeatedCurrent_SkippedWithoutEnded"), bDefeatedCasePassed, Recorder);

		Manager->Reset();
		Manager->Initialize(Participants);
		const uint64 DelayEpoch = Manager->GetBattleEpoch();
		const int32 BeforeDelayResolve = Recorder.Events.Num();
		FHSRTurnDelayRequest DelayRequest;
		DelayRequest.ActionId = FGuid::NewGuid();
		DelayRequest.TargetParticipantId = SecondId;
		const bool bDelayRegistered = Manager->ConsumeBreakDelay(DelayRequest);
		const bool bDelayResolved = Manager->ResolveAction(FirstId);
		LogP9Case(TEXT("BreakDelay_SkippedCandidate"), bDelayRegistered && bDelayResolved
			&& Recorder.Events.Num() == BeforeDelayResolve + 2
			&& IsP9Event(Recorder.Events[BeforeDelayResolve], EHSRTurnLifecycleEventType::TurnEnded, FirstId, DelayEpoch, 1)
			&& IsP9Event(Recorder.Events[BeforeDelayResolve + 1], EHSRTurnLifecycleEventType::TurnStarted, FirstId, DelayEpoch, 2), Recorder);

		Manager->Reset();
		Manager->Initialize(Participants);
		Manager->FinishBattle();
		const int32 BeforeFinishedCalls = Recorder.Events.Num();
		FHSRTurnDelayRequest FinishedDelay;
		FinishedDelay.ActionId = FGuid::NewGuid();
		FinishedDelay.TargetParticipantId = SecondId;
		LogP9Case(TEXT("Finished_ResolveDelayNoEvent"), !Manager->ResolveAction(FirstId)
			&& !Manager->ConsumeBreakDelay(FinishedDelay) && Recorder.Events.Num() == BeforeFinishedCalls, Recorder);

		UHSRTurnManager* EmptyManager = NewObject<UHSRTurnManager>(Coordinator);
		FP9TurnEventRecorder EmptyRecorder;
		BindP9Recorder(EmptyManager, EmptyRecorder);
		const TArray<FHSRBattleParticipant> EmptyParticipants;
		LogP9Case(TEXT("EmptyInitialize_NoEvent"), !EmptyManager->Initialize(EmptyParticipants) && EmptyRecorder.Events.IsEmpty(), EmptyRecorder);

		const bool bTwoRoundEpochs = EpochOne != 0 && EpochTwo != 0 && EpochOne != EpochTwo;
		LogP9Case(TEXT("TwoRounds_EpochIsolation"), bTwoRoundEpochs, Recorder);
		UE_LOG(LogTemp, Log, TEXT("P9-000 TurnLifecycle Harness=COMPLETE"));
	}

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

	static bool SetEnergy(const FHSRBattleParticipant& Participant, float Energy, float MaxEnergy)
	{
		if (!Participant.AbilitySystemComponent.IsValid())
		{
			return false;
		}
		Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), MaxEnergy);
		Participant.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), Energy);
		return true;
	}

	static float GetEnergy(const FHSRBattleParticipant& Participant)
	{
		return Participant.AbilitySystemComponent.IsValid() ? Participant.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()) : -1.0f;
	}

	static void LogP6UltimateCase(const TCHAR* CaseName, bool bPassed, const FGuid& ActionId, const FHSRAbilityResolution& Resolution,
		float EnergyBefore, float EnergyAfter, float HealthBefore, float HealthAfter, FName TurnBefore, FName TurnAfter)
	{
		if (bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("P6-002 Case=%s Result=PASS ActionId=%s ResolutionStatus=%d FailureReason=%d EnergyBefore=%.2f EnergyAfter=%.2f HealthBefore=%.2f HealthAfter=%.2f TurnBefore=%s TurnAfter=%s"),
				CaseName, *ActionId.ToString(), static_cast<int32>(Resolution.Status), static_cast<int32>(Resolution.FailureReason), EnergyBefore, EnergyAfter, HealthBefore, HealthAfter, *TurnBefore.ToString(), *TurnAfter.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P6-002 Case=%s Result=FAIL ActionId=%s ResolutionStatus=%d FailureReason=%d EnergyBefore=%.2f EnergyAfter=%.2f HealthBefore=%.2f HealthAfter=%.2f TurnBefore=%s TurnAfter=%s"),
				CaseName, *ActionId.ToString(), static_cast<int32>(Resolution.Status), static_cast<int32>(Resolution.FailureReason), EnergyBefore, EnergyAfter, HealthBefore, HealthAfter, *TurnBefore.ToString(), *TurnAfter.ToString());
		}
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

		UHSRSkillDefinition* BasicDefinition = const_cast<UHSRSkillDefinition*>(Coordinator->GetBasicAttackDefinition());
		const TSoftClassPtr<UGameplayEffect> OriginalEffectClass = BasicDefinition ? BasicDefinition->EffectGameplayEffectClass : nullptr;
		if (BasicDefinition) { BasicDefinition->EffectGameplayEffectClass.Reset(); }
		const bool bMissingEffectRejected = BasicDefinition && !Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Enemy")))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"))
			&& FMath::IsNearlyEqual(PlayerHealthBeforeRejected, GetHealth(Participants[0]))
			&& FMath::IsNearlyEqual(EnemyHealthBeforeRejected, GetHealth(Participants[1]))
			&& ActionResolvedCount == 0;
		if (BasicDefinition) { BasicDefinition->EffectGameplayEffectClass = OriginalEffectClass; }
		LogCase(TEXT("P5-003_FormalMissingExecutionRejected_NoMutation"), bMissingEffectRejected);

		const bool bLegalAttack = Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Enemy")))
			&& GetHealth(Participants[1]) < 100.0f
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Enemy"))
			&& ActionResolvedCount == 1;
		LogCase(TEXT("P5-003_LegalFormalDamageAndSingleResolve"), bLegalAttack);

		const float EnemyHealthAfterLegal = GetHealth(Participants[1]);
		const bool bAttackDuplicateRejected = !Coordinator->RequestBasicAttack(FName(TEXT("Player")), FName(TEXT("Enemy")))
			&& FMath::IsNearlyEqual(EnemyHealthAfterLegal, GetHealth(Participants[1]))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Enemy"))
			&& ActionResolvedCount == 1;
		LogCase(TEXT("P5-003_DuplicateRejected_NoExtraMutation"), bAttackDuplicateRejected);

		// P6-001 command-idempotence seam. It calls RequestAction directly so the
		// same stable ActionId is submitted twice rather than generating a new ID
		// through the legacy RequestBasicAttack convenience wrapper.
		const auto MakeBasicAttackCommand = [Coordinator](const FGuid& ActionId, FName TargetId)
		{
			FHSRBattleActionCommand Command;
			Command.ActionId = ActionId;
			Command.BattleId = Coordinator->GetCurrentRequestId();
			Command.ActorParticipantId = FName(TEXT("Player"));
			Command.SkillId = FName(TEXT("BasicAttack"));
			Command.TargetParticipantIds.Add(TargetId);
			return Command;
		};

		const bool bP6Setup = SetHealth(Participants[0], 100.0f, 100.0f)
			&& SetHealth(Participants[1], 100.0f, 100.0f)
			&& MainTurnManager->Initialize(Participants)
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"));
		ActionResolvedCount = 0;

		const float P6PlayerHealthBefore = GetHealth(Participants[0]);
		const float P6EnemyHealthBefore = GetHealth(Participants[1]);
		const FHSRBattleActionCommand InvalidTargetCommand = MakeBasicAttackCommand(FGuid::NewGuid(), FName(TEXT("Player")));
		const FHSRAbilityResolution InvalidTargetFirst = Coordinator->RequestAction(InvalidTargetCommand);
		const FHSRAbilityResolution InvalidTargetReplay = Coordinator->RequestAction(InvalidTargetCommand);
		const bool bP6InvalidTargetCachedNoMutation = bP6Setup
			&& InvalidTargetFirst.Status == EHSRAbilityResolutionStatus::Rejected
			&& InvalidTargetFirst.FailureReason == EHSRAbilityFailureReason::InvalidTarget
			&& InvalidTargetReplay.Status == InvalidTargetFirst.Status
			&& InvalidTargetReplay.FailureReason == InvalidTargetFirst.FailureReason
			&& FMath::IsNearlyEqual(P6PlayerHealthBefore, GetHealth(Participants[0]))
			&& FMath::IsNearlyEqual(P6EnemyHealthBefore, GetHealth(Participants[1]))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"))
			&& ActionResolvedCount == 0;
		LogCase(TEXT("P6-001_InvalidTargetCached_NoMutation"), bP6InvalidTargetCachedNoMutation);

		if (BasicDefinition) { BasicDefinition->EffectGameplayEffectClass.Reset(); }
		const FHSRBattleActionCommand MissingEffectCommand = MakeBasicAttackCommand(FGuid::NewGuid(), FName(TEXT("Enemy")));
		const FHSRAbilityResolution MissingEffectFirst = BasicDefinition ? Coordinator->RequestAction(MissingEffectCommand) : FHSRAbilityResolution();
		const FHSRAbilityResolution MissingEffectReplay = BasicDefinition ? Coordinator->RequestAction(MissingEffectCommand) : FHSRAbilityResolution();
		if (BasicDefinition) { BasicDefinition->EffectGameplayEffectClass = OriginalEffectClass; }
		const bool bP6MissingEffectCachedNoMutation = BasicDefinition
			&& MissingEffectFirst.Status == EHSRAbilityResolutionStatus::Rejected
			&& MissingEffectFirst.FailureReason == EHSRAbilityFailureReason::DefinitionMissing
			&& MissingEffectReplay.Status == MissingEffectFirst.Status
			&& MissingEffectReplay.FailureReason == MissingEffectFirst.FailureReason
			&& FMath::IsNearlyEqual(P6PlayerHealthBefore, GetHealth(Participants[0]))
			&& FMath::IsNearlyEqual(P6EnemyHealthBefore, GetHealth(Participants[1]))
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Player"))
			&& ActionResolvedCount == 0;
		LogCase(TEXT("P6-001_FormalMissingExecutionCached_NoMutation"), bP6MissingEffectCachedNoMutation);

		Coordinator->InitializeDevelopmentDamageRng(1337);
		SetHealth(Participants[0], 100.0f, 100.0f);
		SetHealth(Participants[1], 100.0f, 100.0f);
		Coordinator->SetTeamSkillPointsForDevelopmentTest(1, 3);
		MainTurnManager->Initialize(Participants);
		const FHSRBattleActionCommand LegalCommand = MakeBasicAttackCommand(FGuid::NewGuid(), FName(TEXT("Enemy")));
		const float P6EnemyHealthBeforeLegal = GetHealth(Participants[1]);
		const int32 P6SkillPointsBeforeLegal = Coordinator->GetTeamResourceState().CurrentSkillPoints;
		const FHSRAbilityResolution LegalFirst = Coordinator->RequestAction(LegalCommand);
		const float P6EnemyHealthAfterLegal = GetHealth(Participants[1]);
		const FHSRAbilityResolution LegalReplay = Coordinator->RequestAction(LegalCommand);
		const bool bP6LegalCachedSingleMutation = LegalFirst.Status == EHSRAbilityResolutionStatus::Succeeded
			&& LegalReplay.Status == LegalFirst.Status
			&& LegalReplay.FailureReason == LegalFirst.FailureReason
			&& LegalFirst.bHasDamageResult
			&& P6EnemyHealthAfterLegal < P6EnemyHealthBeforeLegal
			&& FMath::IsNearlyEqual(P6EnemyHealthAfterLegal, GetHealth(Participants[1]))
			&& Coordinator->GetTeamResourceState().CurrentSkillPoints == P6SkillPointsBeforeLegal + 1
			&& MainTurnManager->GetCurrentParticipantId() == FName(TEXT("Enemy"))
			&& ActionResolvedCount == 1;
		UE_LOG(LogTemp, Log, TEXT("P6-001 Formal Legal Status=%d Reason=%d HasDamage=%d ReportedApplied=%.2f HPBefore=%.2f HPAfter=%.2f HPDelta=%.2f SPBefore=%d SPAfter=%d Turn=%s Resolved=%d"), static_cast<int32>(LegalFirst.Status), static_cast<int32>(LegalFirst.FailureReason), LegalFirst.bHasDamageResult ? 1 : 0, LegalFirst.DamageResult.Breakdown.AppliedDamage, P6EnemyHealthBeforeLegal, P6EnemyHealthAfterLegal, P6EnemyHealthBeforeLegal - P6EnemyHealthAfterLegal, P6SkillPointsBeforeLegal, Coordinator->GetTeamResourceState().CurrentSkillPoints, *MainTurnManager->GetCurrentParticipantId().ToString(), ActionResolvedCount);
		LogCase(TEXT("P6-001_LegalActionCached_SingleGEAndTurn"), bP6LegalCachedSingleMutation);
		MainTurnManager->OnActionResolved().Remove(ActionResolvedHandle);
		UE_LOG(LogTemp, Log, TEXT("P5-003 AttackTest Harness=COMPLETE"));
	}

	static void RunP6Ultimate(UHSRBattleCoordinator* Coordinator, const UHSRSkillDefinition* UltimateDefinition)
	{
		if (!Coordinator || !UltimateDefinition || !UltimateDefinition->IsValidUltimateDefinition() || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("P6-002 Harness=SKIPPED reason=missing-coordinator-valid-ultimate-definition-or-participants"));
			UE_LOG(LogTemp, Log, TEXT("P6-002 Harness=COMPLETE"));
			return;
		}

		const TArray<FHSRBattleParticipant>& Participants = Coordinator->GetParticipants();
		const FHSRBattleParticipant* Player = Participants.FindByPredicate([](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == FName(TEXT("Player")); });
		const FHSRBattleParticipant* Enemy = Participants.FindByPredicate([](const FHSRBattleParticipant& Participant) { return Participant.ParticipantId == FName(TEXT("Enemy")); });
		UHSRTurnManager* TurnManager = Coordinator->GetTurnManager();
		if (!Player || !Enemy || !TurnManager)
		{
			UE_LOG(LogTemp, Warning, TEXT("P6-002 Harness=SKIPPED reason=missing-player-enemy-or-turn-manager"));
			UE_LOG(LogTemp, Log, TEXT("P6-002 Harness=COMPLETE"));
			return;
		}

		const auto ResetCase = [&Participants, Player, Enemy, TurnManager](float PlayerEnergy)
		{
			return SetSpeed(*Player, 120.0f) && SetSpeed(*Enemy, 80.0f)
				&& SetHealth(*Player, 1000.0f, 1000.0f) && SetHealth(*Enemy, 1000.0f, 1000.0f)
				&& SetEnergy(*Player, PlayerEnergy, 100.0f) && SetEnergy(*Enemy, 0.0f, 100.0f)
				&& TurnManager->Initialize(Participants) && TurnManager->GetCurrentParticipantId() == FName(TEXT("Player"));
		};
		const auto MakeUltimateCommand = [Coordinator, UltimateDefinition](const FGuid& ActionId, FName TargetId)
		{
			FHSRBattleActionCommand Command;
			Command.ActionId = ActionId;
			Command.BattleId = Coordinator->GetCurrentRequestId();
			Command.ActorParticipantId = FName(TEXT("Player"));
			Command.SkillId = UltimateDefinition->SkillId;
			Command.TargetParticipantIds.Add(TargetId);
			return Command;
		};

		// First a full-Energy success determines the configured GAS Cost without
		// duplicating that authored value in C++.
		const bool bFullSetup = ResetCase(100.0f);
		const FGuid FullActionId = FGuid::NewGuid();
		const FName FullTurnBefore = TurnManager->GetCurrentParticipantId();
		const float FullEnergyBefore = GetEnergy(*Player);
		const float FullHealthBefore = GetHealth(*Enemy);
		const FHSRAbilityResolution FullResolution = Coordinator->RequestAction(MakeUltimateCommand(FullActionId, Enemy->ParticipantId));
		const float FullEnergyAfter = GetEnergy(*Player);
		const float FullHealthAfter = GetHealth(*Enemy);
		const FName FullTurnAfter = TurnManager->GetCurrentParticipantId();
		const float MeasuredCost = FullEnergyBefore - FullEnergyAfter;
		const bool bFullPassed = bFullSetup && FullResolution.Succeeded() && MeasuredCost > 0.0f
			&& FullHealthAfter < FullHealthBefore && FullTurnAfter == Enemy->ParticipantId;
		LogP6UltimateCase(TEXT("EnergyFull_LegalUltimate"), bFullPassed, FullActionId, FullResolution, FullEnergyBefore, FullEnergyAfter, FullHealthBefore, FullHealthAfter, FullTurnBefore, FullTurnAfter);

		const FHSRAbilityResolution FullReplayResolution = Coordinator->RequestAction(MakeUltimateCommand(FullActionId, Enemy->ParticipantId));
		const bool bReplayPassed = bFullPassed && FullReplayResolution.Status == FullResolution.Status && FullReplayResolution.FailureReason == FullResolution.FailureReason
			&& FMath::IsNearlyEqual(FullEnergyAfter, GetEnergy(*Player)) && FMath::IsNearlyEqual(FullHealthAfter, GetHealth(*Enemy))
			&& TurnManager->GetCurrentParticipantId() == FullTurnAfter;
		LogP6UltimateCase(TEXT("LegalUltimate_ReplayCached"), bReplayPassed, FullActionId, FullReplayResolution, FullEnergyAfter, GetEnergy(*Player), FullHealthAfter, GetHealth(*Enemy), FullTurnAfter, TurnManager->GetCurrentParticipantId());

		const auto RunInsufficientCase = [&ResetCase, &MakeUltimateCommand, Coordinator, Player, Enemy, TurnManager](const TCHAR* CaseName, float StartingEnergy)
		{
			const bool bSetup = ResetCase(StartingEnergy);
			const FGuid ActionId = FGuid::NewGuid();
			const FName TurnBefore = TurnManager->GetCurrentParticipantId();
			const float EnergyBefore = GetEnergy(*Player);
			const float HealthBefore = GetHealth(*Enemy);
			const FHSRAbilityResolution Resolution = Coordinator->RequestAction(MakeUltimateCommand(ActionId, Enemy->ParticipantId));
			const float EnergyAfter = GetEnergy(*Player);
			const float HealthAfter = GetHealth(*Enemy);
			const FName TurnAfter = TurnManager->GetCurrentParticipantId();
			const bool bPassed = bSetup && Resolution.Status == EHSRAbilityResolutionStatus::Rejected
				&& Resolution.FailureReason == EHSRAbilityFailureReason::InsufficientEnergy
				&& FMath::IsNearlyEqual(EnergyBefore, EnergyAfter) && FMath::IsNearlyEqual(HealthBefore, HealthAfter) && TurnBefore == TurnAfter;
			LogP6UltimateCase(CaseName, bPassed, ActionId, Resolution, EnergyBefore, EnergyAfter, HealthBefore, HealthAfter, TurnBefore, TurnAfter);
		};

		RunInsufficientCase(TEXT("EnergyZero_RejectNoMutation"), 0.0f);
		RunInsufficientCase(TEXT("EnergyBelowCost_RejectNoMutation"), FMath::Max(0.0f, MeasuredCost - 1.0f));

		const bool bExactSetup = ResetCase(MeasuredCost);
		const FGuid ExactActionId = FGuid::NewGuid();
		const FName ExactTurnBefore = TurnManager->GetCurrentParticipantId();
		const float ExactEnergyBefore = GetEnergy(*Player);
		const float ExactHealthBefore = GetHealth(*Enemy);
		const FHSRAbilityResolution ExactResolution = Coordinator->RequestAction(MakeUltimateCommand(ExactActionId, Enemy->ParticipantId));
		const float ExactEnergyAfter = GetEnergy(*Player);
		const float ExactHealthAfter = GetHealth(*Enemy);
		const FName ExactTurnAfter = TurnManager->GetCurrentParticipantId();
		const bool bExactPassed = bExactSetup && MeasuredCost > 0.0f && ExactResolution.Succeeded()
			&& FMath::IsNearlyZero(ExactEnergyAfter) && ExactHealthAfter < ExactHealthBefore && ExactTurnAfter == Enemy->ParticipantId;
		LogP6UltimateCase(TEXT("EnergyExactCost_LegalUltimate"), bExactPassed, ExactActionId, ExactResolution, ExactEnergyBefore, ExactEnergyAfter, ExactHealthBefore, ExactHealthAfter, ExactTurnBefore, ExactTurnAfter);

		const bool bInvalidSetup = ResetCase(100.0f);
		const FGuid InvalidActionId = FGuid::NewGuid();
		const FName InvalidTurnBefore = TurnManager->GetCurrentParticipantId();
		const float InvalidEnergyBefore = GetEnergy(*Player);
		const float InvalidHealthBefore = GetHealth(*Enemy);
		const FHSRAbilityResolution InvalidResolution = Coordinator->RequestAction(MakeUltimateCommand(InvalidActionId, Player->ParticipantId));
		const float InvalidEnergyAfter = GetEnergy(*Player);
		const float InvalidHealthAfter = GetHealth(*Enemy);
		const FName InvalidTurnAfter = TurnManager->GetCurrentParticipantId();
		const bool bInvalidPassed = bInvalidSetup && InvalidResolution.Status == EHSRAbilityResolutionStatus::Rejected
			&& InvalidResolution.FailureReason == EHSRAbilityFailureReason::InvalidTarget
			&& FMath::IsNearlyEqual(InvalidEnergyBefore, InvalidEnergyAfter) && FMath::IsNearlyEqual(InvalidHealthBefore, InvalidHealthAfter) && InvalidTurnBefore == InvalidTurnAfter;
		LogP6UltimateCase(TEXT("InvalidTarget_RejectNoMutation"), bInvalidPassed, InvalidActionId, InvalidResolution, InvalidEnergyBefore, InvalidEnergyAfter, InvalidHealthBefore, InvalidHealthAfter, InvalidTurnBefore, InvalidTurnAfter);

	UE_LOG(LogTemp, Log, TEXT("P6-002 Harness=COMPLETE"));
	}

	static void RunP6SkillPoints(UHSRBattleCoordinator* Coordinator, const UHSRSkillDefinition* SkillDefinition)
	{
		if (!Coordinator || !SkillDefinition || !SkillDefinition->IsValidSkillDefinition() || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("P6-003 Harness=SKIPPED reason=missing-valid-skill-definition-or-participants")); UE_LOG(LogTemp, Log, TEXT("P6-003 Harness=COMPLETE")); return;
		}
		const TArray<FHSRBattleParticipant>& P = Coordinator->GetParticipants(); const FHSRBattleParticipant* Player = P.FindByPredicate([](const FHSRBattleParticipant& X){return X.ParticipantId==FName(TEXT("Player"));}); const FHSRBattleParticipant* Enemy = P.FindByPredicate([](const FHSRBattleParticipant& X){return X.ParticipantId==FName(TEXT("Enemy"));}); UHSRTurnManager* TM=Coordinator->GetTurnManager();
		if (!Player || !Enemy || !TM) { UE_LOG(LogTemp, Warning, TEXT("P6-003 Harness=SKIPPED reason=missing-runtime")); UE_LOG(LogTemp, Log, TEXT("P6-003 Harness=COMPLETE")); return; }
		const auto Setup=[&](int32 SP){ SetSpeed(*Player,120);SetSpeed(*Enemy,80);SetHealth(*Player,1000,1000);SetHealth(*Enemy,1000,1000);SetEnergy(*Player,100,100);Coordinator->SetTeamSkillPointsForDevelopmentTest(SP,3);return TM->Initialize(P)&&TM->GetCurrentParticipantId()==Player->ParticipantId;};
		const auto Cmd=[&](FName Skill,FName Target){FHSRBattleActionCommand C;C.ActionId=FGuid::NewGuid();C.BattleId=Coordinator->GetCurrentRequestId();C.ActorParticipantId=Player->ParticipantId;C.SkillId=Skill;C.TargetParticipantIds.Add(Target);return C;};
		const auto Log=[&](const TCHAR* N,bool OK,const FHSRBattleActionCommand& C,float EB,float EA,float HB,float HA,FName TB,FName TA,int32 SB,int32 SA){if(OK){UE_LOG(LogTemp,Log,TEXT("P6-003 Case=%s Result=PASS ActionId=%s SPBefore=%d SPAfter=%d EnergyBefore=%.0f EnergyAfter=%.0f HPBefore=%.0f HPAfter=%.0f TurnBefore=%s TurnAfter=%s"),N,*C.ActionId.ToString(),SB,SA,EB,EA,HB,HA,*TB.ToString(),*TA.ToString());}else{UE_LOG(LogTemp,Error,TEXT("P6-003 Case=%s Result=FAIL ActionId=%s SPBefore=%d SPAfter=%d EnergyBefore=%.0f EnergyAfter=%.0f HPBefore=%.0f HPAfter=%.0f TurnBefore=%s TurnAfter=%s"),N,*C.ActionId.ToString(),SB,SA,EB,EA,HB,HA,*TB.ToString(),*TA.ToString());}};
		// Zero SP Skill reject.
		Setup(0); auto Z=Cmd(SkillDefinition->SkillId,Enemy->ParticipantId);float ze=GetEnergy(*Player),zh=GetHealth(*Enemy);FName zt=TM->GetCurrentParticipantId();auto zr=Coordinator->RequestAction(Z);Log(TEXT("ZeroSP_SkillReject"),zr.FailureReason==EHSRAbilityFailureReason::InsufficientSkillPoint&&Coordinator->GetTeamResourceState().CurrentSkillPoints==0&&GetEnergy(*Player)==ze&&GetHealth(*Enemy)==zh&&TM->GetCurrentParticipantId()==zt,Z,ze,GetEnergy(*Player),zh,GetHealth(*Enemy),zt,TM->GetCurrentParticipantId(),0,Coordinator->GetTeamResourceState().CurrentSkillPoints);
		// Cap Basic succeeds but cannot exceed cap.
		Setup(3);auto C=Cmd(FName(TEXT("BasicAttack")),Enemy->ParticipantId);float ch=GetHealth(*Enemy);auto cr=Coordinator->RequestAction(C);Log(TEXT("CapBasic_NoOverflow"),cr.Succeeded()&&Coordinator->GetTeamResourceState().CurrentSkillPoints==3,C,100,GetEnergy(*Player),ch,GetHealth(*Enemy),Player->ParticipantId,TM->GetCurrentParticipantId(),3,Coordinator->GetTeamResourceState().CurrentSkillPoints);
		// Basic +1 then Skill -1.
		Setup(1);auto B=Cmd(FName(TEXT("BasicAttack")),Enemy->ParticipantId);auto br=Coordinator->RequestAction(B);Log(TEXT("BasicCommitPlusOne"),br.Succeeded()&&Coordinator->GetTeamResourceState().CurrentSkillPoints==2,B,100,GetEnergy(*Player),1000,GetHealth(*Enemy),Player->ParticipantId,TM->GetCurrentParticipantId(),1,Coordinator->GetTeamResourceState().CurrentSkillPoints);
		Setup(2);auto S=Cmd(SkillDefinition->SkillId,Enemy->ParticipantId);float sh=GetHealth(*Enemy);auto sr=Coordinator->RequestAction(S);int32 sa=Coordinator->GetTeamResourceState().CurrentSkillPoints;auto replay=Coordinator->RequestAction(S);Log(TEXT("SkillCommitMinusOne_ReplayCached"),sr.Succeeded()&&replay.Succeeded()&&sa==1&&Coordinator->GetTeamResourceState().CurrentSkillPoints==1,S,100,GetEnergy(*Player),sh,GetHealth(*Enemy),Player->ParticipantId,TM->GetCurrentParticipantId(),2,Coordinator->GetTeamResourceState().CurrentSkillPoints);
		// Invalid target proves no reserve/commit.
		Setup(2);auto I=Cmd(SkillDefinition->SkillId,Player->ParticipantId);auto ir=Coordinator->RequestAction(I);Log(TEXT("SkillInvalidTarget_Rollback"),ir.FailureReason==EHSRAbilityFailureReason::InvalidTarget&&Coordinator->GetTeamResourceState().CurrentSkillPoints==2,I,100,GetEnergy(*Player),1000,GetHealth(*Enemy),Player->ParticipantId,TM->GetCurrentParticipantId(),2,Coordinator->GetTeamResourceState().CurrentSkillPoints);
		UE_LOG(LogTemp, Log, TEXT("P6-003 Harness=COMPLETE"));
	}

	static void RunP6HealAndViewState(UHSRBattleCoordinator* Coordinator, const UHSRSkillDefinition* HealDefinition)
	{
		if (!Coordinator || !HealDefinition || !HealDefinition->IsValidHealDefinition() || HealDefinition->TargetType != EHSRTargetType::Self || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Warning, TEXT("P6-004 Harness=SKIPPED reason=requires-valid-Self-Heal-definition-and-two-participants"));
			UE_LOG(LogTemp, Log, TEXT("P6-004 Harness=COMPLETE"));
			return;
		}
		const TArray<FHSRBattleParticipant>& Participants = Coordinator->GetParticipants();
		const FHSRBattleParticipant* Player = Participants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.ParticipantId == FName(TEXT("Player")); });
		const FHSRBattleParticipant* Enemy = Participants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.ParticipantId == FName(TEXT("Enemy")); });
		UHSRTurnManager* Manager = Coordinator->GetTurnManager();
		if (!Player || !Enemy || !Player->AbilitySystemComponent.IsValid() || !Manager) { UE_LOG(LogTemp, Error, TEXT("P6-004 Case=Setup Result=FAIL")); UE_LOG(LogTemp, Log, TEXT("P6-004 Harness=COMPLETE")); return; }
		SetSpeed(*Player, 120.f); SetSpeed(*Enemy, 80.f); SetHealth(*Player, 50.f, 100.f); SetHealth(*Enemy, 100.f, 100.f); Manager->Initialize(Participants);
		auto Make = [Coordinator, HealDefinition](const FGuid& Id, FName Target){ FHSRBattleActionCommand C; C.ActionId=Id; C.BattleId=Coordinator->GetCurrentRequestId(); C.ActorParticipantId=FName(TEXT("Player")); C.SkillId=HealDefinition->SkillId; C.TargetParticipantIds.Add(Target); return C; };
		const FGuid LegalId=FGuid::NewGuid(); const float HpBefore=GetHealth(*Player); const FName TurnBefore=Manager->GetCurrentParticipantId(); const FHSRAbilityResolution Legal=Coordinator->RequestAction(Make(LegalId, FName(TEXT("Player")))); const float HpAfter=GetHealth(*Player); const FName TurnAfter=Manager->GetCurrentParticipantId();
		const FHSRAbilityResolution Replay=Coordinator->RequestAction(Make(LegalId, FName(TEXT("Player"))));
		const bool bLegal=Legal.Succeeded() && HpAfter>HpBefore && HpAfter<=100.f && TurnBefore!=TurnAfter && Replay.Succeeded() && FMath::IsNearlyEqual(HpAfter,GetHealth(*Player));
		UE_LOG(LogTemp,Log,TEXT("P6-004 Case=HealSuccess_ReplayCached Result=%s HPBefore=%.2f HPAfter=%.2f TurnBefore=%s TurnAfter=%s"),bLegal?TEXT("PASS"):TEXT("FAIL"),HpBefore,HpAfter,*TurnBefore.ToString(),*TurnAfter.ToString());
		SetHealth(*Player,100.f,100.f); Manager->Initialize(Participants); const float FullHp=GetHealth(*Player); const FName FullTurn=Manager->GetCurrentParticipantId(); const FHSRAbilityResolution Full=Coordinator->RequestAction(Make(FGuid::NewGuid(),FName(TEXT("Player")))); const bool bFull=Full.Status==EHSRAbilityResolutionStatus::Rejected&&Full.FailureReason==EHSRAbilityFailureReason::AlreadyAtFullHealth&&FMath::IsNearlyEqual(FullHp,GetHealth(*Player))&&FullTurn==Manager->GetCurrentParticipantId();
		UE_LOG(LogTemp,Log,TEXT("P6-004 Case=FullHealth_ZeroMutation Result=%s"),bFull?TEXT("PASS"):TEXT("FAIL"));
		const FHSRAbilityResolution Forged=Coordinator->RequestAction(Make(FGuid::NewGuid(),FName(TEXT("Enemy")))); const bool bForged=Forged.FailureReason==EHSRAbilityFailureReason::InvalidTarget&&FMath::IsNearlyEqual(FullHp,GetHealth(*Player))&&FullTurn==Manager->GetCurrentParticipantId();
		UE_LOG(LogTemp,Log,TEXT("P6-004 Case=ForgedTarget_ZeroMutation Result=%s"),bForged?TEXT("PASS"):TEXT("FAIL"));
		UHSRBattleCommandViewModel* TestVm=NewObject<UHSRBattleCommandViewModel>(Coordinator); int32 First=0,Second=0; const FDelegateHandle FirstHandle=TestVm->OnChanged().AddLambda([&First](const FHSRBattleCommandViewState&){++First;}); TestVm->SetState(Coordinator->GetCommandViewState()); TestVm->OnChanged().Remove(FirstHandle); const FDelegateHandle SecondHandle=TestVm->OnChanged().AddLambda([&Second](const FHSRBattleCommandViewState&){++Second;}); TestVm->SetState(Coordinator->GetCommandViewState()); TestVm->OnChanged().Remove(SecondHandle); const bool bRebuild=First==1&&Second==1;
		UE_LOG(LogTemp,Log,TEXT("P6-004 Case=ViewModelRebuild_DelegateUnbound Result=%s First=%d Second=%d"),bRebuild?TEXT("PASS"):TEXT("FAIL"),First,Second);
		UE_LOG(LogTemp,Log,TEXT("P6-004 Harness=COMPLETE"));
	}

	static void RunP9StatusHarness(UHSRBattleCoordinator* Coordinator)
	{
		if (!Coordinator || !Coordinator->GetStatusDefinition() || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Error, TEXT("P9-001 Status Harness=SKIPPED Reason=MissingDefinitionOrParticipants"));
			return;
		}
		const TArray<FHSRBattleParticipant>& Participants = Coordinator->GetParticipants();
		const FHSRBattleParticipant* Player = Participants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Player; });
		const FHSRBattleParticipant* Enemy = Participants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Enemy; });
		UHSRTurnManager* RuntimeManager = Coordinator->GetTurnManager();
		if (!Player || !Enemy || !RuntimeManager || !Player->AbilitySystemComponent.IsValid() || !Enemy->AbilitySystemComponent.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("P9-001 Status Harness=SKIPPED Reason=InvalidRuntime"));
			return;
		}
		const float PlayerSpeedBefore = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute());
		const float EnemySpeedBefore = Enemy->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetSpeedAttribute());
		const float PlayerHealthBefore = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
		Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
		Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
		UHSRTurnManager* Manager = NewObject<UHSRTurnManager>(Coordinator);
		Manager->Initialize(Participants);
		UHSRStatusComponent* StatusComponent = NewObject<UHSRStatusComponent>(Player->Actor.Get());
		StatusComponent->RegisterComponent();
		StatusComponent->InitializeStatusRuntime(Player->ParticipantId, Player->AbilitySystemComponent.Get());
		StatusComponent->BindTurnManager(Manager);
		const float AttackBefore = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const FGameplayTag StatusTag = Coordinator->GetStatusDefinition()->GrantedStatusTag;
		const EHSRStatusOperationResult Added = StatusComponent->AddOrRefreshStatus(Coordinator->GetStatusDefinition(), Player->ParticipantId, Player->ParticipantId);
		FHSRStatusRuntimeSnapshot AddSnapshot = StatusComponent->GetSnapshot();
		const float AttackAdded = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool bAdd = Added == EHSRStatusOperationResult::Success && AddSnapshot.InstanceCount == 1 && AddSnapshot.RemainingTurns == 2
			&& AddSnapshot.Stacks == 1 && AddSnapshot.ApplyCount == 1 && AddSnapshot.bHandleValid && AddSnapshot.bHandleActiveInAbilitySystem
			&& FMath::IsNearlyEqual(AttackAdded, AttackBefore + 10.0f) && Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag);
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=AddSuccess Result=%s Remaining=%d Stacks=%d InstanceCount=%d Handle=%s HandleActive=%d ApplyCount=%d Attack=%.2f Tag=%d"), bAdd ? TEXT("PASS") : TEXT("FAIL"), AddSnapshot.RemainingTurns, AddSnapshot.Stacks, AddSnapshot.InstanceCount, *AddSnapshot.ActiveHandleIdentity, AddSnapshot.bHandleActiveInAbilitySystem ? 1 : 0, AddSnapshot.ApplyCount, AttackAdded, Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag) ? 1 : 0);
		Manager->ResolveAction(Enemy->ParticipantId);
		FHSRStatusRuntimeSnapshot OtherSnapshot = StatusComponent->GetSnapshot();
		const bool bOther = OtherSnapshot.RemainingTurns == 2 && OtherSnapshot.InstanceCount == 1 && OtherSnapshot.Stacks == 1
			&& OtherSnapshot.bHandleActiveInAbilitySystem && OtherSnapshot.ActiveHandleIdentity == AddSnapshot.ActiveHandleIdentity
			&& OtherSnapshot.ApplyCount == AddSnapshot.ApplyCount && OtherSnapshot.RemoveCount == AddSnapshot.RemoveCount
			&& FMath::IsNearlyEqual(Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), AttackBefore + 10.0f)
			&& Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag);
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=OtherParticipantTurnEnded Result=%s Remaining=%d"), bOther ? TEXT("PASS") : TEXT("FAIL"), OtherSnapshot.RemainingTurns);
		Manager->ResolveAction(Player->ParticipantId);
		FHSRStatusRuntimeSnapshot FirstSnapshot = StatusComponent->GetSnapshot();
		const bool bFirst = FirstSnapshot.RemainingTurns == 1 && FirstSnapshot.InstanceCount == 1 && FirstSnapshot.Stacks == 1
			&& FirstSnapshot.bHandleActiveInAbilitySystem && FirstSnapshot.ActiveHandleIdentity == AddSnapshot.ActiveHandleIdentity
			&& FirstSnapshot.ApplyCount == AddSnapshot.ApplyCount && FirstSnapshot.RemoveCount == AddSnapshot.RemoveCount
			&& FMath::IsNearlyEqual(Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), AttackBefore + 10.0f)
			&& Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag);
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=FirstTargetTurnEnded Result=%s Remaining=%d"), bFirst ? TEXT("PASS") : TEXT("FAIL"), FirstSnapshot.RemainingTurns);
		const EHSRStatusOperationResult Refreshed = StatusComponent->AddOrRefreshStatus(Coordinator->GetStatusDefinition(), Player->ParticipantId, Player->ParticipantId);
		FHSRStatusRuntimeSnapshot RefreshSnapshot = StatusComponent->GetSnapshot();
		const bool bRefresh = Refreshed == EHSRStatusOperationResult::Refreshed && RefreshSnapshot.RemainingTurns == 2
			&& RefreshSnapshot.InstanceCount == 1 && RefreshSnapshot.Stacks == 1 && RefreshSnapshot.ApplyCount == 1
			&& RefreshSnapshot.ActiveHandleIdentity == AddSnapshot.ActiveHandleIdentity && RefreshSnapshot.bHandleActiveInAbilitySystem
			&& FMath::IsNearlyEqual(Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), AttackBefore + 10.0f);
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=RefreshAtOneTurn Result=%s Remaining=%d InstanceCount=%d ApplyCount=%d SameHandle=%d"), bRefresh ? TEXT("PASS") : TEXT("FAIL"), RefreshSnapshot.RemainingTurns, RefreshSnapshot.InstanceCount, RefreshSnapshot.ApplyCount, RefreshSnapshot.ActiveHandleIdentity == AddSnapshot.ActiveHandleIdentity ? 1 : 0);
		Manager->ResolveAction(Enemy->ParticipantId);
		Manager->ResolveAction(Player->ParticipantId);
		Manager->ResolveAction(Enemy->ParticipantId);
		Manager->ResolveAction(Player->ParticipantId);
		FHSRStatusRuntimeSnapshot Expired = StatusComponent->GetSnapshot();
		const float AttackAfter = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool bExpired = Expired.InstanceCount == 0 && !Expired.bHandleValid && !Expired.bHandleActiveInAbilitySystem
			&& Expired.RemoveCount == 1 && FMath::IsNearlyEqual(AttackBefore, AttackAfter) && !Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag);
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=TwoTargetTurnEndedAfterRefresh Result=%s InstanceCount=%d HandleValid=%d RemoveCount=%d AttackBefore=%.2f AttackAfter=%.2f Tag=%d"), bExpired ? TEXT("PASS") : TEXT("FAIL"), Expired.InstanceCount, Expired.bHandleValid ? 1 : 0, Expired.RemoveCount, AttackBefore, AttackAfter, Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag) ? 1 : 0);

		const UHSRStatusDefinition* Definition = Coordinator->GetStatusDefinition();
		const uint64 Epoch = Manager->GetBattleEpoch();
		StatusComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId);
		FHSRTurnLifecycleEvent DuplicateEvent; DuplicateEvent.BattleEpoch = Epoch; DuplicateEvent.ParticipantId = Player->ParticipantId; DuplicateEvent.TurnSequence = 100; DuplicateEvent.EventType = EHSRTurnLifecycleEventType::TurnEnded;
		StatusComponent->ConsumeLifecycleEventForDevelopmentTest(DuplicateEvent);
		const FHSRStatusRuntimeSnapshot DuplicateOnce = StatusComponent->GetSnapshot();
		StatusComponent->ConsumeLifecycleEventForDevelopmentTest(DuplicateEvent);
		const FHSRStatusRuntimeSnapshot DuplicateTwice = StatusComponent->GetSnapshot();
		const bool bDuplicate = DuplicateOnce.RemainingTurns == 1 && DuplicateTwice.RemainingTurns == 1 && DuplicateOnce.LastConsumedTurnSequence == DuplicateTwice.LastConsumedTurnSequence;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=DuplicateTurnEvent Result=%s Remaining=%d Sequence=%llu"), bDuplicate ? TEXT("PASS") : TEXT("FAIL"), DuplicateTwice.RemainingTurns, DuplicateTwice.LastConsumedTurnSequence);
		StatusComponent->ClearStatus();

		auto MakeDefinition = [Coordinator, Definition]()
		{
			UHSRStatusDefinition* Copy = NewObject<UHSRStatusDefinition>(Coordinator);
			Copy->StatusId = Definition->StatusId; Copy->DurationTurns = Definition->DurationTurns; Copy->MaxStacks = Definition->MaxStacks;
			Copy->TriggerTiming = Definition->TriggerTiming; Copy->RefreshPolicy = Definition->RefreshPolicy; Copy->GrantedStatusTag = Definition->GrantedStatusTag;
			Copy->InfiniteGameplayEffectClass = Definition->InfiniteGameplayEffectClass;
			return Copy;
		};
		UHSRStatusDefinition* BadId = MakeDefinition(); BadId->StatusId = NAME_None;
		UHSRStatusDefinition* BadDuration = MakeDefinition(); BadDuration->DurationTurns = 0;
		UHSRStatusDefinition* BadGe = MakeDefinition(); BadGe->InfiniteGameplayEffectClass.Reset();
		UHSRStatusDefinition* BadTiming = MakeDefinition(); BadTiming->TriggerTiming = EHSRStatusTriggerTiming::Unsupported;
		UHSRStatusDefinition* BadPolicy = MakeDefinition(); BadPolicy->RefreshPolicy = EHSRStatusRefreshPolicy::Unsupported;
		UHSRStatusDefinition* NonInfinite = MakeDefinition(); NonInfinite->InfiniteGameplayEffectClass = Coordinator->GetParticipantInitializationGameplayEffectForDevelopmentTest();
		const bool bInvalidDefinitions = StatusComponent->AddOrRefreshStatus(nullptr, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::InvalidDefinition
			&& StatusComponent->AddOrRefreshStatus(BadId, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::InvalidStatusId
			&& StatusComponent->AddOrRefreshStatus(BadDuration, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::InvalidDuration
			&& StatusComponent->AddOrRefreshStatus(BadGe, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::MissingGameplayEffect
			&& StatusComponent->AddOrRefreshStatus(BadTiming, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::InvalidPolicy
			&& StatusComponent->AddOrRefreshStatus(BadPolicy, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::InvalidPolicy
			&& StatusComponent->AddOrRefreshStatus(NonInfinite, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::GameplayEffectNotInfinite
			&& StatusComponent->GetSnapshot().InstanceCount == 0;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=InvalidDefinition_StatusId_Duration_GE Result=%s InstanceCount=%d"), bInvalidDefinitions ? TEXT("PASS") : TEXT("FAIL"), StatusComponent->GetSnapshot().InstanceCount);

		UHSRTurnManager* FailureManager = NewObject<UHSRTurnManager>(Coordinator); FailureManager->Initialize(Participants);
		UHSRStatusComponent* FailureComponent = NewObject<UHSRStatusComponent>(Player->Actor.Get()); FailureComponent->RegisterComponent();
		const bool bMissingSetup = FailureComponent->InitializeStatusRuntime(Player->ParticipantId, Player->AbilitySystemComponent.Get()) && FailureComponent->BindTurnManager(FailureManager);
		FailureComponent->InvalidateAbilitySystemForDevelopmentTest();
		const bool bMissingAsc = bMissingSetup && FailureComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::MissingAbilitySystem
			&& FailureComponent->GetSnapshot().InstanceCount == 0 && FailureComponent->GetSnapshot().ApplyCount == 0;
		const bool bInvalidTarget = FailureComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, FName(TEXT("MissingTarget"))) == EHSRStatusOperationResult::InvalidTarget;
		FailureComponent->DestroyComponent();
		const float SavedHealth = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
		Coordinator->ClearRuntimeDelegatesForDevelopmentTest();
		Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		const bool bDefeated = StatusComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::DefeatedTarget;
		Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), SavedHealth);
		const bool bParticipantFailures = bMissingAsc && bInvalidTarget && bDefeated;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=MissingASC_InvalidTarget_DefeatedTarget Result=%s"), bParticipantFailures ? TEXT("PASS") : TEXT("FAIL"));

		StatusComponent->SetForceApplyFailureForDevelopmentTest(true);
		const int32 ApplyBeforeFailure = StatusComponent->GetSnapshot().ApplyCount;
		const float AttackBeforeFailure = Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool bTagBeforeFailure = Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag);
		const bool bForcedFailure = StatusComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::ApplyFailed
			&& StatusComponent->GetSnapshot().InstanceCount == 0 && StatusComponent->GetSnapshot().RemainingTurns == 0
			&& !StatusComponent->GetSnapshot().bHandleValid && !StatusComponent->GetSnapshot().bHandleActiveInAbilitySystem
			&& StatusComponent->GetSnapshot().ApplyCount == ApplyBeforeFailure
			&& FMath::IsNearlyEqual(Player->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), AttackBeforeFailure)
			&& Player->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag) == bTagBeforeFailure;
		StatusComponent->SetForceApplyFailureForDevelopmentTest(false);
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=ForcedApplyFailure Result=%s"), bForcedFailure ? TEXT("PASS") : TEXT("FAIL"));

		StatusComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId);
		const int32 RemoveBeforeDeath = StatusComponent->GetSnapshot().RemoveCount;
		StatusComponent->ClearStatus(); const EHSRStatusOperationResult RepeatDeathClear = StatusComponent->ClearStatus();
		const bool bDeathClear = StatusComponent->GetSnapshot().InstanceCount == 0 && StatusComponent->GetSnapshot().RemoveCount == RemoveBeforeDeath + 1 && RepeatDeathClear == EHSRStatusOperationResult::Success;

		UHSRStatusComponent* EndPlayComponent = NewObject<UHSRStatusComponent>(Player->Actor.Get()); EndPlayComponent->RegisterComponent();
		const bool bEndInit = EndPlayComponent->InitializeStatusRuntime(Player->ParticipantId, Player->AbilitySystemComponent.Get()) && EndPlayComponent->BindTurnManager(Manager);
		const bool bEndAdd = bEndInit && EndPlayComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::Success;
		EndPlayComponent->DestroyComponent();
		const bool bEndPlay = bEndAdd && EndPlayComponent->GetSnapshot().InstanceCount == 0;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=EndPlayClear Result=%s"), bEndPlay ? TEXT("PASS") : TEXT("FAIL"));

		Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
		Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
		UHSRTurnManager* Replacement = NewObject<UHSRTurnManager>(Coordinator); Replacement->Initialize(Participants);
		const bool bReplacementBound = StatusComponent && StatusComponent->BindTurnManager(Replacement);
		const bool bReplacementAdd = bReplacementBound && StatusComponent->AddOrRefreshStatus(Definition, Player->ParticipantId, Player->ParticipantId) == EHSRStatusOperationResult::Success;
		const int32 ReplacementBefore = StatusComponent ? StatusComponent->GetSnapshot().RemainingTurns : -1;
		Manager->ResolveAction(Manager->GetCurrentParticipantId());
		const int32 AfterOldManagerEvent = StatusComponent ? StatusComponent->GetSnapshot().RemainingTurns : -1;
		const bool bReplacementOtherResolved = Replacement->ResolveAction(Enemy->ParticipantId);
		const int32 AfterReplacementOther = StatusComponent->GetSnapshot().RemainingTurns;
		const bool bReplacementTargetResolved = Replacement->ResolveAction(Player->ParticipantId);
		const bool bReplacement = bReplacementAdd && ReplacementBefore == 2 && AfterOldManagerEvent == 2 && StatusComponent->GetSnapshot().RemainingTurns == 1;
		const bool bReplacementRealEvents = bReplacementOtherResolved && AfterReplacementOther == 2 && bReplacementTargetResolved;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=ManagerReplacement Result=%s Remaining=%d"), bReplacement ? TEXT("PASS") : TEXT("FAIL"), StatusComponent ? StatusComponent->GetSnapshot().RemainingTurns : -1);
		StatusComponent->ClearStatus();

		TSubclassOf<UGameplayEffect> SentinelClass = Definition->InfiniteGameplayEffectClass.LoadSynchronous();
		Player->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), PlayerSpeedBefore);
		Enemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), EnemySpeedBefore);
		StatusComponent->DestroyComponent();
		const FHSRBattleInitResult DeathRecovery = Coordinator->ResetAndRebuildForDevelopmentTest(Coordinator->GetWorld());
		const TArray<FHSRBattleParticipant>& DeathParticipants = Coordinator->GetParticipants();
		const FHSRBattleParticipant* DeathPlayer = DeathParticipants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Player; });
		const float DeathAttackBefore = DeathPlayer && DeathPlayer->AbilitySystemComponent.IsValid() ? DeathPlayer->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()) : 0.0f;
		const float DeathHealthBefore = DeathPlayer && DeathPlayer->AbilitySystemComponent.IsValid() ? DeathPlayer->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) : 0.0f;
		const FGameplayEffectSpecHandle SentinelSpec = DeathPlayer ? DeathPlayer->AbilitySystemComponent->MakeOutgoingSpec(SentinelClass, 1.0f, DeathPlayer->AbilitySystemComponent->MakeEffectContext()) : FGameplayEffectSpecHandle();
		const FActiveGameplayEffectHandle SentinelHandle = SentinelSpec.IsValid() ? DeathPlayer->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SentinelSpec.Data.Get()) : FActiveGameplayEffectHandle();
		UHSRStatusComponent* RuntimeStatus = DeathPlayer ? Coordinator->GetStatusComponent(DeathPlayer->ParticipantId) : nullptr;
		const bool bRuntimeAdd = DeathRecovery.IsSuccess() && SentinelHandle.IsValid() && RuntimeStatus && Coordinator->AddStatusForDevelopmentTest(DeathPlayer->ParticipantId, DeathPlayer->ParticipantId) == EHSRStatusOperationResult::Success;
		const FString OwnedHandle = RuntimeStatus ? RuntimeStatus->GetSnapshot().ActiveHandleIdentity : FString();
		if (DeathPlayer) DeathPlayer->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		const bool bSentinelSurvived = DeathPlayer && SentinelHandle.IsValid() && DeathPlayer->AbilitySystemComponent->GetActiveGameplayEffect(SentinelHandle) != nullptr
			&& Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Finished && Coordinator->GetStatusComponent(DeathPlayer->ParticipantId) == nullptr
			&& DeathPlayer->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag)
			&& FMath::IsNearlyEqual(DeathPlayer->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), DeathAttackBefore + 10.0f);
		const bool bDistinctHandles = SentinelHandle.ToString() != OwnedHandle;
		const bool bSentinelRemoved = DeathPlayer && DeathPlayer->AbilitySystemComponent->RemoveActiveGameplayEffect(SentinelHandle);
		const bool bSentinelGone = DeathPlayer && DeathPlayer->AbilitySystemComponent->GetActiveGameplayEffect(SentinelHandle) == nullptr
			&& FMath::IsNearlyEqual(DeathPlayer->AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), DeathAttackBefore)
			&& !DeathPlayer->AbilitySystemComponent->HasMatchingGameplayTag(StatusTag);
		const bool bTargetDeath = bRuntimeAdd && bSentinelSurvived && bDistinctHandles && bSentinelRemoved && bSentinelGone;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=TargetDeathClear Result=%s OwnedHandle=%s SentinelHandle=%s SentinelActive=%d SentinelRemoved=%d"), bDeathClear && bTargetDeath ? TEXT("PASS") : TEXT("FAIL"), *OwnedHandle, *SentinelHandle.ToString(), bSentinelSurvived ? 1 : 0, bSentinelRemoved ? 1 : 0);
		if (DeathPlayer) DeathPlayer->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), DeathHealthBefore);

		const FHSRBattleInitResult Rebuild = Coordinator->ResetAndRebuildForDevelopmentTest(Coordinator->GetWorld());
		const TArray<FHSRBattleParticipant>& RebuiltParticipants = Coordinator->GetParticipants();
		const FHSRBattleParticipant* RebuiltPlayer = RebuiltParticipants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Player; });
		const FHSRBattleParticipant* RebuiltEnemy = RebuiltParticipants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Enemy; });
		if (RebuiltPlayer && RebuiltPlayer->AbilitySystemComponent.IsValid()) RebuiltPlayer->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
		if (RebuiltEnemy && RebuiltEnemy->AbilitySystemComponent.IsValid()) RebuiltEnemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
		UHSRTurnManager* SecondManager = NewObject<UHSRTurnManager>(Coordinator);
		const bool bSecondManager = Rebuild.IsSuccess() && RebuiltPlayer && RebuiltEnemy && SecondManager->Initialize(RebuiltParticipants);
		UHSRStatusComponent* SecondComponent = bSecondManager ? NewObject<UHSRStatusComponent>(RebuiltPlayer->Actor.Get()) : nullptr;
		if (SecondComponent) { SecondComponent->RegisterComponent(); SecondComponent->InitializeStatusRuntime(RebuiltPlayer->ParticipantId, RebuiltPlayer->AbilitySystemComponent.Get()); SecondComponent->BindTurnManager(SecondManager); }
		const bool bSecondAdd = SecondComponent && SecondComponent->AddOrRefreshStatus(Definition, RebuiltPlayer->ParticipantId, RebuiltPlayer->ParticipantId) == EHSRStatusOperationResult::Success;
		const bool bSecondOtherOne = RebuiltEnemy && SecondManager->ResolveAction(RebuiltEnemy->ParticipantId);
		const bool bSecondTargetOne = RebuiltPlayer && SecondManager->ResolveAction(RebuiltPlayer->ParticipantId);
		const int32 AfterFirstTarget = SecondComponent ? SecondComponent->GetSnapshot().RemainingTurns : -1;
		const bool bSecondOtherTwo = RebuiltEnemy && SecondManager->ResolveAction(RebuiltEnemy->ParticipantId);
		const bool bSecondTargetTwo = RebuiltPlayer && SecondManager->ResolveAction(RebuiltPlayer->ParticipantId);
		const bool bResetSecond = bSecondAdd && bSecondOtherOne && bSecondTargetOne && AfterFirstTarget == 1
			&& bSecondOtherTwo && bSecondTargetTwo && SecondComponent->GetSnapshot().InstanceCount == 0 && SecondComponent->GetSnapshot().RemoveCount == 1;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=ResetAndSecondBattle Result=%s Remaining=%d RemoveCount=%d"), bResetSecond ? TEXT("PASS") : TEXT("FAIL"), SecondComponent ? SecondComponent->GetSnapshot().RemainingTurns : -1, SecondComponent ? SecondComponent->GetSnapshot().RemoveCount : -1);
		if (SecondComponent) SecondComponent->DestroyComponent();

		const FHSRBattleInitResult FinishedRecovery = Coordinator->ResetAndRebuildForDevelopmentTest(Coordinator->GetWorld());
		const TArray<FHSRBattleParticipant>& FinishedParticipants = Coordinator->GetParticipants();
		const FHSRBattleParticipant* FinishedPlayer = FinishedParticipants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Player; });
		const FHSRBattleParticipant* FinishedEnemy = FinishedParticipants.FindByPredicate([](const FHSRBattleParticipant& P){ return P.Team == EHSRBattleParticipantTeam::Enemy; });
		const bool bFinishedAdd = FinishedRecovery.IsSuccess() && FinishedPlayer && Coordinator->AddStatusForDevelopmentTest(FinishedPlayer->ParticipantId, FinishedPlayer->ParticipantId) == EHSRStatusOperationResult::Success;
		if (FinishedEnemy && FinishedEnemy->AbilitySystemComponent.IsValid()) FinishedEnemy->AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		const bool bFinishedClear = bFinishedAdd && Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Finished && Coordinator->GetStatusComponent(FinishedPlayer->ParticipantId) == nullptr;
		UE_LOG(LogTemp, Log, TEXT("P9-001 Status Case=FinishedClear Result=%s"), bFinishedClear ? TEXT("PASS") : TEXT("FAIL"));
		const bool bAllPass = bAdd && bOther && bFirst && bRefresh && bExpired && bDuplicate && bInvalidDefinitions
			&& bParticipantFailures && bForcedFailure && bDeathClear && bTargetDeath && bFinishedClear && bResetSecond && bEndPlay && bReplacement && bReplacementRealEvents;
		if (bAllPass)
		{
			UE_LOG(LogTemp, Log, TEXT("P9-001 Status Harness=COMPLETE"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P9-001 Status Harness=INCOMPLETE Reason=OneOrMoreCasesFailed"));
		}
	}

	static void RunP9StackHarness(UHSRBattleCoordinator* Coordinator, const UHSRStatusDefinition* StackDefinition, const UHSRStatusDefinition* RefreshDefinition)
	{
		if (!Coordinator || !StackDefinition || !RefreshDefinition || StackDefinition->RefreshPolicy != EHSRStatusRefreshPolicy::AddStack || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Error, TEXT("P9-002 Stack Harness=SKIPPED Reason=GateBDefinitionNotConfigured"));
			return;
		}
		const TArray<FHSRBattleParticipant>& Participants = Coordinator->GetParticipants();
		const FHSRBattleParticipant& Target = Participants[0];
		const FHSRBattleParticipant& Other = Participants[1];
		if (!Target.Actor.IsValid() || !Target.AbilitySystemComponent.IsValid()) { UE_LOG(LogTemp, Error, TEXT("P9-002 Stack Harness=SKIPPED Reason=InvalidTarget")); return; }
		UAbilitySystemComponent* ASC = Target.AbilitySystemComponent.Get();
		const auto HasTag = [ASC, StackDefinition]() { return ASC->HasMatchingGameplayTag(StackDefinition->GrantedStatusTag); };
		const auto SameState = [](const FHSRStatusRuntimeSnapshot& A, const FHSRStatusRuntimeSnapshot& B)
		{
			return A.StatusId == B.StatusId && A.SourceParticipantId == B.SourceParticipantId && A.TargetParticipantId == B.TargetParticipantId
				&& A.BattleEpoch == B.BattleEpoch && A.LastConsumedTurnSequence == B.LastConsumedTurnSequence
				&& A.RemainingTurns == B.RemainingTurns && A.Stacks == B.Stacks && A.InstanceCount == B.InstanceCount
				&& A.ApplyCount == B.ApplyCount && A.RemoveCount == B.RemoveCount && A.bHandleValid == B.bHandleValid
				&& A.ActiveHandleIdentity == B.ActiveHandleIdentity && A.bHandleActiveInAbilitySystem == B.bHandleActiveInAbilitySystem
				&& A.GameplayEffectStackCount == B.GameplayEffectStackCount && A.bSecondaryHandleValid == B.bSecondaryHandleValid
				&& A.SecondaryHandleIdentity == B.SecondaryHandleIdentity
				&& A.bSecondaryHandleActiveInAbilitySystem == B.bSecondaryHandleActiveInAbilitySystem;
		};
		const auto LogStackCase = [](const TCHAR* Name, bool bPassed, const FString& Evidence)
		{
			if (bPassed)
			{
				UE_LOG(LogTemp, Log, TEXT("P9-002 Stack Case=%s Result=PASS %s"), Name, *Evidence);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("P9-002 Stack Case=%s Result=FAIL %s"), Name, *Evidence);
			}
		};

		UHSRTurnManager* Manager = NewObject<UHSRTurnManager>(Coordinator); Manager->Initialize(Participants);
		UHSRStatusComponent* Component = NewObject<UHSRStatusComponent>(Target.Actor.Get()); Component->RegisterComponent(); Component->InitializeStatusRuntime(Target.ParticipantId, ASC); Component->BindTurnManager(Manager);
		const float Baseline = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const EHSRStatusOperationResult AddOne = Component->AddOrRefreshStatus(StackDefinition, Target.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot One = Component->GetSnapshot();
		const EHSRStatusOperationResult AddTwo = Component->AddOrRefreshStatus(StackDefinition, Other.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot Two = Component->GetSnapshot();
		const FActiveGameplayEffectHandle StackHandle = Component->GetPrimaryHandleForDevelopmentTest();
		const float AttackAtTwo = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool bAddStack = AddOne == EHSRStatusOperationResult::Success && AddTwo == EHSRStatusOperationResult::StackAdded
			&& One.InstanceCount == 1 && One.Stacks == 1 && One.GameplayEffectStackCount == 1 && One.ApplyCount == 1
			&& Two.InstanceCount == 1 && Two.Stacks == 2 && Two.GameplayEffectStackCount == 2 && Two.ApplyCount == 2
			&& One.ActiveHandleIdentity == Two.ActiveHandleIdentity && Two.bHandleActiveInAbilitySystem && !Two.bSecondaryHandleValid
			&& FMath::IsNearlyEqual(AttackAtTwo, Baseline + 20.0f) && HasTag();
		LogStackCase(TEXT("AddStack1To2"), bAddStack, FString::Printf(TEXT("Handle=%s GASStacks=%d RuntimeStacks=%d Attack=%.2f Source=%s"), *Two.ActiveHandleIdentity, Two.GameplayEffectStackCount, Two.Stacks, AttackAtTwo, *Two.SourceParticipantId.ToString()));

		const int32 ApplyAtTwo = Two.ApplyCount;
		const EHSRStatusOperationResult Over = Component->AddOrRefreshStatus(StackDefinition, Target.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot OverSnapshot = Component->GetSnapshot();
		const float AttackAtMax = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const EHSRStatusOperationResult StackClearResult = Component->ClearStatus();
		const FHSRStatusRuntimeSnapshot StackCleared = Component->GetSnapshot();
		const bool bStackCleared = StackClearResult == EHSRStatusOperationResult::Success && !ASC->GetActiveGameplayEffect(StackHandle)
			&& StackCleared.InstanceCount == 0 && !StackCleared.bHandleValid && !StackCleared.bSecondaryHandleValid
			&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), Baseline) && !HasTag();
		const bool bOver = Over == EHSRStatusOperationResult::AtMaxRefreshed && OverSnapshot.RemainingTurns == StackDefinition->DurationTurns
			&& OverSnapshot.SourceParticipantId == Target.ParticipantId && OverSnapshot.Stacks == 2 && OverSnapshot.GameplayEffectStackCount == 2
			&& OverSnapshot.ApplyCount == ApplyAtTwo && OverSnapshot.ActiveHandleIdentity == Two.ActiveHandleIdentity
			&& OverSnapshot.bHandleActiveInAbilitySystem && !OverSnapshot.bSecondaryHandleValid
			&& FMath::IsNearlyEqual(AttackAtMax, Baseline + 20.0f) && bStackCleared;
		LogStackCase(TEXT("OverMaxAdd"), bOver, FString::Printf(TEXT("Remaining=%d ApplyCount=%d Attack=%.2f ClearResult=%d ClearedRemoveCount=%d"), OverSnapshot.RemainingTurns, OverSnapshot.ApplyCount, AttackAtMax, static_cast<int32>(StackClearResult), StackCleared.RemoveCount));

		Component->AddOrRefreshStatus(RefreshDefinition, Target.ParticipantId, Target.ParticipantId);
		FHSRTurnLifecycleEvent End; End.BattleEpoch = Manager->GetBattleEpoch(); End.ParticipantId = Target.ParticipantId; End.TurnSequence = 1; End.EventType = EHSRTurnLifecycleEventType::TurnEnded;
		Component->ConsumeLifecycleEventForDevelopmentTest(End);
		const FHSRStatusRuntimeSnapshot BeforeRefresh = Component->GetSnapshot();
		const EHSRStatusOperationResult Refresh = Component->AddOrRefreshStatus(RefreshDefinition, Other.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot AfterRefresh = Component->GetSnapshot();
		const bool bRefresh = Refresh == EHSRStatusOperationResult::Refreshed && BeforeRefresh.RemainingTurns == 1
			&& AfterRefresh.RemainingTurns == RefreshDefinition->DurationTurns && AfterRefresh.SourceParticipantId == Other.ParticipantId
			&& BeforeRefresh.StatusId == AfterRefresh.StatusId && BeforeRefresh.TargetParticipantId == AfterRefresh.TargetParticipantId
			&& BeforeRefresh.BattleEpoch == AfterRefresh.BattleEpoch && BeforeRefresh.LastConsumedTurnSequence == AfterRefresh.LastConsumedTurnSequence
			&& BeforeRefresh.Stacks == AfterRefresh.Stacks && BeforeRefresh.InstanceCount == AfterRefresh.InstanceCount
			&& BeforeRefresh.ApplyCount == AfterRefresh.ApplyCount && BeforeRefresh.RemoveCount == AfterRefresh.RemoveCount
			&& BeforeRefresh.ActiveHandleIdentity == AfterRefresh.ActiveHandleIdentity && BeforeRefresh.bHandleActiveInAbilitySystem == AfterRefresh.bHandleActiveInAbilitySystem
			&& BeforeRefresh.GameplayEffectStackCount == AfterRefresh.GameplayEffectStackCount
			&& BeforeRefresh.bSecondaryHandleValid == AfterRefresh.bSecondaryHandleValid && !AfterRefresh.bSecondaryHandleValid
			&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), Baseline + 10.0f) && HasTag();
		LogStackCase(TEXT("RefreshAt1"), bRefresh, FString::Printf(TEXT("BeforeRemaining=%d AfterRemaining=%d Handle=%s ApplyCount=%d"), BeforeRefresh.RemainingTurns, AfterRefresh.RemainingTurns, *AfterRefresh.ActiveHandleIdentity, AfterRefresh.ApplyCount));

		const FActiveGameplayEffectHandle OldHandle = Component->GetPrimaryHandleForDevelopmentTest();
		const FString OldHandleIdentity = AfterRefresh.ActiveHandleIdentity;
		const int32 ApplyBeforeReplace = AfterRefresh.ApplyCount;
		const int32 RemoveBeforeReplace = AfterRefresh.RemoveCount;
		const EHSRStatusOperationResult Replaced = Component->ReplaceStatus(StackDefinition, Other.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot ReplacedSnapshot = Component->GetSnapshot();
		const FActiveGameplayEffectHandle NewHandle = Component->GetPrimaryHandleForDevelopmentTest();
		const bool bReplace = Replaced == EHSRStatusOperationResult::Replaced && !ASC->GetActiveGameplayEffect(OldHandle)
			&& ASC->GetActiveGameplayEffect(NewHandle) && ReplacedSnapshot.ActiveHandleIdentity != OldHandleIdentity
			&& ReplacedSnapshot.InstanceCount == 1 && ReplacedSnapshot.Stacks == 1 && ReplacedSnapshot.GameplayEffectStackCount == 1
			&& ReplacedSnapshot.SourceParticipantId == Other.ParticipantId && ReplacedSnapshot.ApplyCount == ApplyBeforeReplace + 1
			&& ReplacedSnapshot.RemoveCount == RemoveBeforeReplace + 1 && !ReplacedSnapshot.bSecondaryHandleValid
			&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), Baseline + 10.0f) && HasTag();
		LogStackCase(TEXT("ExplicitReplaceSuccess"), bReplace, FString::Printf(TEXT("Old=%s New=%s GASStacks=%d ApplyCount=%d RemoveCount=%d"), *OldHandleIdentity, *ReplacedSnapshot.ActiveHandleIdentity, ReplacedSnapshot.GameplayEffectStackCount, ReplacedSnapshot.ApplyCount, ReplacedSnapshot.RemoveCount));

		Component->SetForceApplyFailureForDevelopmentTest(true);
		const FHSRStatusRuntimeSnapshot BeforeFail = Component->GetSnapshot();
		const FActiveGameplayEffectHandle HandleBeforeFail = Component->GetPrimaryHandleForDevelopmentTest();
		const float AttackBeforeFail = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool bTagBeforeFail = HasTag();
		const EHSRStatusOperationResult ApplyFail = Component->ReplaceStatus(RefreshDefinition, Target.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot AfterFail = Component->GetSnapshot();
		Component->SetForceApplyFailureForDevelopmentTest(false);
		const bool bNewFail = ApplyFail == EHSRStatusOperationResult::ApplyFailed && SameState(BeforeFail, AfterFail)
			&& ASC->GetActiveGameplayEffect(HandleBeforeFail) && ASC->GetCurrentStackCount(HandleBeforeFail) == BeforeFail.GameplayEffectStackCount
			&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), AttackBeforeFail) && HasTag() == bTagBeforeFail;
		LogStackCase(TEXT("NewApplyFailure"), bNewFail, FString::Printf(TEXT("Handle=%s GASStacks=%d AttackBefore=%.2f AttackAfter=%.2f ApplyCount=%d RemoveCount=%d"), *AfterFail.ActiveHandleIdentity, AfterFail.GameplayEffectStackCount, AttackBeforeFail, ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), AfterFail.ApplyCount, AfterFail.RemoveCount));

		const int32 ApplyBeforeOldFail = AfterFail.ApplyCount;
		const int32 RemoveBeforeOldFail = AfterFail.RemoveCount;
		Component->SetForceOldRemoveFailureForDevelopmentTest(true);
		const EHSRStatusOperationResult RemoveFail = Component->ReplaceStatus(RefreshDefinition, Target.ParticipantId, Target.ParticipantId);
		const FHSRStatusRuntimeSnapshot Dual = Component->GetSnapshot();
		Component->SetForceOldRemoveFailureForDevelopmentTest(false);
		const FActiveGameplayEffectHandle DualPrimary = Component->GetPrimaryHandleForDevelopmentTest();
		const FActiveGameplayEffectHandle DualSecondary = Component->GetSecondaryHandleForDevelopmentTest();
		const float DualAttack = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool bDualOwned = RemoveFail == EHSRStatusOperationResult::RemoveFailed && Dual.bHandleValid && Dual.bSecondaryHandleValid
			&& Dual.ActiveHandleIdentity != Dual.SecondaryHandleIdentity && ASC->GetActiveGameplayEffect(DualPrimary) && ASC->GetActiveGameplayEffect(DualSecondary)
			&& Dual.Stacks == 1 && Dual.GameplayEffectStackCount == 1 && Dual.ApplyCount == ApplyBeforeOldFail + 1
			&& Dual.RemoveCount == RemoveBeforeOldFail && FMath::IsNearlyEqual(DualAttack, Baseline + 20.0f) && HasTag();
		const EHSRStatusOperationResult DualClearResult = Component->ClearStatus();
		const FHSRStatusRuntimeSnapshot DualCleared = Component->GetSnapshot();
		const bool bDualCleared = DualClearResult == EHSRStatusOperationResult::Success && !ASC->GetActiveGameplayEffect(DualPrimary)
			&& !ASC->GetActiveGameplayEffect(DualSecondary) && DualCleared.InstanceCount == 0 && !DualCleared.bHandleValid && !DualCleared.bSecondaryHandleValid
			&& DualCleared.RemoveCount == RemoveBeforeOldFail + 2
			&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), Baseline) && !HasTag();
		const bool bOldFail = bDualOwned && bDualCleared;
		LogStackCase(TEXT("OldRemoveFailure"), bOldFail, FString::Printf(TEXT("Old=%s New=%s DualAttack=%.2f ClearResult=%d RemoveCount=%d"), *Dual.ActiveHandleIdentity, *Dual.SecondaryHandleIdentity, DualAttack, static_cast<int32>(DualClearResult), DualCleared.RemoveCount));

		const FGuid AddId = FGuid::NewGuid();
		const EHSRStatusOperationResult FirstId = Component->AddOrRefreshStatus(StackDefinition, Target.ParticipantId, Target.ParticipantId, AddId);
		const FHSRStatusRuntimeSnapshot IdBefore = Component->GetSnapshot();
		const FActiveGameplayEffectHandle IdHandle = Component->GetPrimaryHandleForDevelopmentTest();
		const float IdAttackBefore = ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute());
		const bool IdTagBefore = HasTag();
		const EHSRStatusOperationResult Replay = Component->AddOrRefreshStatus(StackDefinition, Other.ParticipantId, Target.ParticipantId, AddId);
		const FHSRStatusRuntimeSnapshot IdAfter = Component->GetSnapshot();
		const bool bDuplicate = FirstId == EHSRStatusOperationResult::Success && Replay == EHSRStatusOperationResult::IgnoredEvent
			&& SameState(IdBefore, IdAfter) && ASC->GetActiveGameplayEffect(IdHandle)
			&& ASC->GetCurrentStackCount(IdHandle) == IdBefore.GameplayEffectStackCount
			&& FMath::IsNearlyEqual(ASC->GetNumericAttribute(UHSRCoreAttributeSet::GetAttackAttribute()), IdAttackBefore) && HasTag() == IdTagBefore;
		const bool bDifferentSources = Two.InstanceCount == 1 && Two.SourceParticipantId == Other.ParticipantId && Two.Stacks == 2
			&& One.SourceParticipantId == Target.ParticipantId && Two.ActiveHandleIdentity == One.ActiveHandleIdentity;
		LogStackCase(TEXT("DifferentSources"), bDifferentSources, FString::Printf(TEXT("FirstSource=%s SecondSource=%s Handle=%s"), *One.SourceParticipantId.ToString(), *Two.SourceParticipantId.ToString(), *Two.ActiveHandleIdentity));
		LogStackCase(TEXT("DuplicateAdd"), bDuplicate, FString::Printf(TEXT("Source=%s Handle=%s GASStacks=%d ApplyCount=%d RemoveCount=%d"), *IdAfter.SourceParticipantId.ToString(), *IdAfter.ActiveHandleIdentity, IdAfter.GameplayEffectStackCount, IdAfter.ApplyCount, IdAfter.RemoveCount));
		Component->ClearStatus(); Component->DestroyComponent();
		const bool bAll=bAddStack&&bOver&&bRefresh&&bReplace&&bNewFail&&bOldFail&&bDifferentSources&&bDuplicate;
		if(bAll){UE_LOG(LogTemp,Log,TEXT("P9-002 Stack Harness=COMPLETE"));}else{UE_LOG(LogTemp,Error,TEXT("P9-002 Stack Harness=INCOMPLETE"));}
	}

	static void RunP9DotBreakHarness(UHSRBattleCoordinator* Coordinator, const UHSRStatusDefinition* DotDefinition, const UHSRStatusDefinition* BreakDefinition)
	{
		if (!Coordinator || !DotDefinition || !BreakDefinition || DotDefinition->Validate() != EHSRStatusOperationResult::Success
			|| BreakDefinition->Validate() != EHSRStatusOperationResult::Success || DotDefinition->EffectKind != EHSRStatusEffectKind::DamageOverTime
			|| BreakDefinition->EffectKind != EHSRStatusEffectKind::TagOnly || Coordinator->GetParticipants().Num() != 2)
		{
			UE_LOG(LogTemp, Error, TEXT("P9-003 DotBreak Harness=SKIPPED Reason=AssetsMissingOrFieldsNotReady"));
			return;
		}
		const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0];
		const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1];
		UHSRStatusComponent* Component = Coordinator->GetStatusComponent(Target.ParticipantId);
		if (!Source.AbilitySystemComponent.IsValid() || !Target.AbilitySystemComponent.IsValid() || !Component || !Coordinator->GetTurnManager())
		{
			UE_LOG(LogTemp, Error, TEXT("P9-003 DotBreak Harness=SKIPPED Reason=RuntimeNotReady"));
			return;
		}
		int32 FailedCases = 0;
		const auto LogCase = [&FailedCases](const TCHAR* Name, bool bPassed)
		{
			if (bPassed) { UE_LOG(LogTemp, Log, TEXT("P9-003 DotBreak Case=%s Result=PASS"), Name); }
			else { ++FailedCases; UE_LOG(LogTemp, Error, TEXT("P9-003 DotBreak Case=%s Result=FAIL"), Name); }
		};
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1000.0f);
		UHSRTurnManager* FormalManager = Coordinator->GetTurnManager();
		const uint64 OldFormalEpoch = FormalManager->GetBattleEpoch();
		FormalManager->Reset(); FormalManager->Initialize(Coordinator->GetParticipants());
		const auto ResolveThroughTargetTurn = [FormalManager, &Target]()
		{
			for (int32 Step = 0; Step < 4; ++Step)
			{
				const FName Current = FormalManager->GetCurrentParticipantId();
				const bool bTargetTurn = Current == Target.ParticipantId;
				if (!FormalManager->ResolveAction(Current)) return false;
				if (bTargetTurn) return true;
			}
			return false;
		};
		const FGuid AddOperation = FGuid::NewGuid();
		const EHSRStatusOperationResult AddResult = Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, AddOperation);
		const FHSRStatusRuntimeSnapshot Added = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		const bool bAdd = AddResult == EHSRStatusOperationResult::Success && Added.InstanceCount == 1 && Added.RemainingTurns == 2
			&& FMath::IsNearlyEqual(Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 1000.0f);
		LogCase(TEXT("DoTAdd_NoImmediateDamage"), bAdd);
		const EHSRStatusOperationResult DuplicateResult = Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, AddOperation);
		const FHSRStatusRuntimeSnapshot AfterDuplicate = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("DuplicateAdd_NoMutation"), DuplicateResult == EHSRStatusOperationResult::IgnoredEvent && AfterDuplicate.RemainingTurns == Added.RemainingTurns && AfterDuplicate.ActiveHandleIdentity == Added.ActiveHandleIdentity);
		FHSRTurnLifecycleEvent Event; Event.BattleEpoch = FormalManager->GetBattleEpoch(); Event.EventType = EHSRTurnLifecycleEventType::TurnEnded; Event.TurnSequence = 1; Event.ParticipantId = Source.ParticipantId;
		Component->ConsumeLifecycleEventForDevelopmentTest(Event);
		const FHSRStatusRuntimeSnapshot AfterOther = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("NonTarget_NoDamageOrDecrement"), AfterOther.RemainingTurns == 2 && FMath::IsNearlyEqual(Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 1000.0f));
		Event.ParticipantId = Target.ParticipantId; Event.BattleEpoch = OldFormalEpoch; Component->ConsumeLifecycleEventForDevelopmentTest(Event);
		const FHSRStatusRuntimeSnapshot AfterOldEpoch = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("OldEpoch_NoDamageOrDecrement"), AfterOldEpoch.RemainingTurns == 2 && FMath::IsNearlyEqual(Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 1000.0f));
		Event.BattleEpoch = FormalManager->GetBattleEpoch();
		Coordinator->SetStatusDamageApplyFailureForDevelopmentTest(true); const FHSRStatusRuntimeSnapshot BeforeApplyFailure = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId); const bool bFailureTurnResolved = ResolveThroughTargetTurn();
		const FHSRStatusRuntimeSnapshot AfterApplyFailure = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("DamageApplyFailure_NoConsume"), bFailureTurnResolved && AfterApplyFailure.RemainingTurns == BeforeApplyFailure.RemainingTurns && AfterApplyFailure.LastConsumedTurnSequence == BeforeApplyFailure.LastConsumedTurnSequence && FMath::IsNearlyEqual(Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), 1000.0f));
		Coordinator->SetStatusDamageApplyFailureForDevelopmentTest(false);
		const float HealthBeforeFirst = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); const bool bRetryTurnResolved = ResolveThroughTargetTurn();
		const FHSRStatusRuntimeSnapshot AfterFirst = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("ApplyFailureRetry_TriggerBeforeDecrement"), bRetryTurnResolved && AfterFirst.RemainingTurns == 1 && AfterFirst.LastConsumedTurnSequence > BeforeApplyFailure.LastConsumedTurnSequence && Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) < HealthBeforeFirst);
		const float HealthAfterFirst = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); Event.TurnSequence = AfterFirst.LastConsumedTurnSequence; Component->ConsumeLifecycleEventForDevelopmentTest(Event);
		const FHSRStatusRuntimeSnapshot AfterDuplicateEvent = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("DuplicateTurnEvent_NoSecondDamage"), AfterDuplicateEvent.RemainingTurns == 1 && FMath::IsNearlyEqual(Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), HealthAfterFirst));
		const float HealthBeforeFinal = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); const bool bFinalTurnResolved = ResolveThroughTargetTurn();
		const FHSRStatusRuntimeSnapshot AfterFinal = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		LogCase(TEXT("FinalTargetTurn_DamageThenExpiry"), bFinalTurnResolved && AfterFinal.InstanceCount == 0 && Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) < HealthBeforeFinal);

		const FGuid BreakOperation = FGuid::NewGuid();
		const EHSRStatusOperationResult BreakAdd = Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, BreakOperation);
		const FHSRStatusRuntimeSnapshot BreakAdded = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, BreakDefinition->StatusId);
		const EHSRStatusOperationResult BreakReplay = Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, BreakOperation);
		const FHSRStatusRuntimeSnapshot BreakReplayed = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, BreakDefinition->StatusId);
		LogCase(TEXT("BreakResultRequest_DuplicateAction"), BreakAdd == EHSRStatusOperationResult::Success && BreakReplay == EHSRStatusOperationResult::IgnoredEvent && BreakAdded.ActiveHandleIdentity == BreakReplayed.ActiveHandleIdentity && BreakReplayed.InstanceCount == 1);
		const FGuid CoexistOperation = FGuid::NewGuid();
		const EHSRStatusOperationResult CoexistDot = Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, CoexistOperation);
		const FHSRStatusRuntimeSnapshot CoexistDotSnapshot = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId);
		const FHSRStatusRuntimeSnapshot CoexistBreakSnapshot = Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, BreakDefinition->StatusId);
		LogCase(TEXT("DoTAndBreak_Coexist"), CoexistDot == EHSRStatusOperationResult::Success && CoexistDotSnapshot.InstanceCount == 1 && CoexistBreakSnapshot.InstanceCount == 1 && CoexistDotSnapshot.ActiveHandleIdentity != CoexistBreakSnapshot.ActiveHandleIdentity);

		UHSRStatusDefinition* InvalidId = DuplicateObject<UHSRStatusDefinition>(DotDefinition, Coordinator); InvalidId->StatusId = FName(TEXT("Status.Unsupported"));
		UHSRStatusDefinition* InvalidGE = DuplicateObject<UHSRStatusDefinition>(DotDefinition, Coordinator); InvalidGE->InfiniteGameplayEffectClass.Reset();
		UHSRStatusDefinition* InvalidRule = DuplicateObject<UHSRStatusDefinition>(DotDefinition, Coordinator); InvalidRule->DamageRule.Reset();
		UHSRStatusDefinition* InvalidDamageType = DuplicateObject<UHSRStatusDefinition>(DotDefinition, Coordinator); InvalidDamageType->DamageType = FGameplayTag();
		LogCase(TEXT("InvalidDefinition_GE_Rule_DamageType"), InvalidId->Validate() == EHSRStatusOperationResult::InvalidStatusId
			&& InvalidGE->Validate() == EHSRStatusOperationResult::MissingGameplayEffect && InvalidRule->Validate() == EHSRStatusOperationResult::InvalidDefinition
			&& InvalidDamageType->Validate() == EHSRStatusOperationResult::InvalidDefinition);
		LogCase(TEXT("InvalidTarget"), Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, NAME_None, FGuid::NewGuid()) == EHSRStatusOperationResult::InvalidTarget);
		UHSRStatusComponent* MissingASC = NewObject<UHSRStatusComponent>(Target.Actor.Get()); MissingASC->RegisterComponent(); MissingASC->InitializeStatusRuntime(Target.ParticipantId, Target.AbilitySystemComponent.Get()); MissingASC->BindTurnManager(Coordinator->GetTurnManager()); MissingASC->InvalidateAbilitySystemForDevelopmentTest();
		LogCase(TEXT("MissingASC"), MissingASC->AddOrRefreshStatus(DotDefinition, Source.ParticipantId, Target.ParticipantId) == EHSRStatusOperationResult::MissingAbilitySystem); MissingASC->DestroyComponent();
		Component->ClearStatus();

		UHSRTurnManager* OldManager = NewObject<UHSRTurnManager>(Coordinator); OldManager->Initialize(Coordinator->GetParticipants());
		UHSRTurnManager* NewManager = NewObject<UHSRTurnManager>(Coordinator); NewManager->Initialize(Coordinator->GetParticipants());
		UHSRStatusComponent* LifecycleComponent = NewObject<UHSRStatusComponent>(Target.Actor.Get()); LifecycleComponent->RegisterComponent(); LifecycleComponent->InitializeStatusRuntime(Target.ParticipantId, Target.AbilitySystemComponent.Get()); LifecycleComponent->BindTurnManager(OldManager);
		LifecycleComponent->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const FActiveGameplayEffectHandle OldManagerHandle = LifecycleComponent->GetHandleForDevelopmentTest(BreakDefinition->StatusId);
		LifecycleComponent->BindTurnManager(NewManager); const FHSRStatusRuntimeSnapshot AfterReplacementClear = LifecycleComponent->GetSnapshot(BreakDefinition->StatusId);
		LifecycleComponent->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const FHSRStatusRuntimeSnapshot BeforeOldManagerEvent = LifecycleComponent->GetSnapshot(BreakDefinition->StatusId);
		OldManager->ResolveAction(OldManager->GetCurrentParticipantId()); const FHSRStatusRuntimeSnapshot AfterOldManagerEvent = LifecycleComponent->GetSnapshot(BreakDefinition->StatusId);
		LogCase(TEXT("ManagerReplacement_UnbindsOldEvents"), !Target.AbilitySystemComponent->GetActiveGameplayEffect(OldManagerHandle) && AfterReplacementClear.InstanceCount == 0 && BeforeOldManagerEvent.RemainingTurns == AfterOldManagerEvent.RemainingTurns);
		const FActiveGameplayEffectHandle EndPlayHandle = LifecycleComponent->GetHandleForDevelopmentTest(BreakDefinition->StatusId); LifecycleComponent->DestroyComponent();
		LogCase(TEXT("EndPlay_ClearsAndUnbinds"), !Target.AbilitySystemComponent->GetActiveGameplayEffect(EndPlayHandle));

		UWorld* BattleWorld = Target.Actor->GetWorld();
		Component->ClearStatus(); Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid()); Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid());
		const FActiveGameplayEffectHandle FinishedDotHandle = Component->GetHandleForDevelopmentTest(DotDefinition->StatusId); const FActiveGameplayEffectHandle FinishedBreakHandle = Component->GetHandleForDevelopmentTest(BreakDefinition->StatusId); UHSRTurnManager* FinishedManager = Coordinator->GetTurnManager();
		const int32 DefeatsBeforeFinished = Coordinator->GetDefeatCountForDevelopmentTest(); const int32 ResultsBeforeFinished = Coordinator->GetBattleResultBroadcastCountForDevelopmentTest();
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		const int32 DefeatsAfterFinished = Coordinator->GetDefeatCountForDevelopmentTest(); const int32 ResultsAfterFinished = Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(); const bool bLateFinishedResolve = FinishedManager->ResolveAction(FinishedManager->GetCurrentParticipantId());
		LogCase(TEXT("Finished_NoEventSideEffects"), DefeatsAfterFinished == DefeatsBeforeFinished + 1 && ResultsAfterFinished == ResultsBeforeFinished + 1 && !bLateFinishedResolve
			&& !Target.AbilitySystemComponent->GetActiveGameplayEffect(FinishedDotHandle) && !Target.AbilitySystemComponent->GetActiveGameplayEffect(FinishedBreakHandle)
			&& Coordinator->GetDefeatCountForDevelopmentTest() == DefeatsAfterFinished && Coordinator->GetBattleResultBroadcastCountForDevelopmentTest() == ResultsAfterFinished);
		const FHSRBattleInitResult FinishedRebuild = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
		if (!FinishedRebuild.IsSuccess() || Coordinator->GetParticipants().Num() != 2) { LogCase(TEXT("LethalFinalTick_ExactlyOnceNoDecrement"), false); LogCase(TEXT("DeathReset_RebuildsCleanRuntime"), false); }
		else
		{
			const FHSRBattleParticipant& LethalSource = Coordinator->GetParticipants()[0]; const FHSRBattleParticipant& LethalTarget = Coordinator->GetParticipants()[1]; UHSRStatusComponent* FormalLethalComponent = Coordinator->GetStatusComponent(LethalTarget.ParticipantId); UHSRTurnManager* LethalManager = Coordinator->GetTurnManager();
			LethalTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f); LethalTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1000.0f); Coordinator->AddDamageOverTimeForDevelopmentTest(LethalSource.ParticipantId, LethalTarget.ParticipantId, FGuid::NewGuid());
			const auto ResolveLethalTargetTurn = [LethalManager, &LethalTarget]() { for (int32 Step=0; Step<4; ++Step) { const FName Current=LethalManager->GetCurrentParticipantId(); const bool bTarget=Current==LethalTarget.ParticipantId; if(!LethalManager->ResolveAction(Current)) return false; if(bTarget) return true; } return false; };
			const bool bFirstLethalTurn = ResolveLethalTargetTurn(); const FHSRStatusRuntimeSnapshot BeforeLethal = FormalLethalComponent->GetSnapshot(DotDefinition->StatusId); LethalTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1.0f);
			const int32 DamageBeforeLethal = Coordinator->GetStatusDamageCommitCountForDevelopmentTest(); const int32 DefeatsBeforeLethal = Coordinator->GetDefeatCountForDevelopmentTest(); const int32 ResultsBeforeLethal = Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(); const bool bFinalLethalTurn = ResolveLethalTargetTurn();
			const int32 DamageAfterLethal = Coordinator->GetStatusDamageCommitCountForDevelopmentTest(); const int32 DefeatsAfterLethal = Coordinator->GetDefeatCountForDevelopmentTest(); const int32 ResultsAfterLethal = Coordinator->GetBattleResultBroadcastCountForDevelopmentTest(); const FHSRStatusRuntimeSnapshot LethalCleanup = Coordinator->GetLastClearedStatusSnapshotForDevelopmentTest(LethalTarget.ParticipantId); const bool bLateLethalResolve = LethalManager->ResolveAction(LethalManager->GetCurrentParticipantId());
			const bool bLethalPassed = bFirstLethalTurn && bFinalLethalTurn && !bLateLethalResolve && BeforeLethal.RemainingTurns == 1
				&& LethalCleanup.LastRemovedRemainingTurns == 1 && LethalCleanup.LastRemovedTurnSequence == BeforeLethal.LastConsumedTurnSequence
				&& DamageAfterLethal == DamageBeforeLethal + 1 && DefeatsAfterLethal == DefeatsBeforeLethal + 1
				&& ResultsAfterLethal == ResultsBeforeLethal + 1
				&& Coordinator->GetStatusDamageCommitCountForDevelopmentTest() == DamageAfterLethal
				&& Coordinator->GetDefeatCountForDevelopmentTest() == DefeatsAfterLethal
				&& Coordinator->GetBattleResultBroadcastCountForDevelopmentTest() == ResultsAfterLethal;
			UE_LOG(LogTemp, Log, TEXT("P9-003 DotBreak LethalEvidence FirstResolve=%d TerminalResolve=%d LateResolve=%d RemainingBefore=%d RemovedRemaining=%d SequenceBefore=%llu RemovedSequence=%llu DamageBefore=%d DamageAfter=%d DefeatsBefore=%d DefeatsAfter=%d ResultsBefore=%d ResultsAfter=%d"),
				bFirstLethalTurn ? 1 : 0, bFinalLethalTurn ? 1 : 0, bLateLethalResolve ? 1 : 0, BeforeLethal.RemainingTurns,
				LethalCleanup.LastRemovedRemainingTurns, BeforeLethal.LastConsumedTurnSequence, LethalCleanup.LastRemovedTurnSequence,
				DamageBeforeLethal, DamageAfterLethal, DefeatsBeforeLethal, DefeatsAfterLethal, ResultsBeforeLethal, ResultsAfterLethal);
			LogCase(TEXT("LethalFinalTick_ExactlyOnceNoDecrement"), bLethalPassed);
		}
		const FHSRBattleInitResult RebuildResult = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
		LogCase(TEXT("DeathReset_RebuildsCleanRuntime"), RebuildResult.IsSuccess() && Coordinator->GetParticipants().Num() == 2 && Coordinator->GetStatusSnapshotForDevelopmentTest(Coordinator->GetParticipants()[0].ParticipantId, DotDefinition->StatusId).InstanceCount == 0);
		if (FailedCases == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("P9-003 DotBreak Harness=COMPLETE"));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P9-003 DotBreak Harness=INCOMPLETE FailedCases=%d"), FailedCases);
		}
	}

	static void RunP9ImmunityDispelHarness(UHSRBattleCoordinator* Coordinator, const UHSRStatusDefinition* DotDefinition,
		const UHSRStatusDefinition* BreakDefinition, const UHSRStatusDefinition* AttackDefinition, TSubclassOf<UGameplayEffect> ImmunityEffect)
	{
		if (!Coordinator || !DotDefinition || !BreakDefinition || !AttackDefinition || !ImmunityEffect
			|| DotDefinition->Validate() != EHSRStatusOperationResult::Success || BreakDefinition->Validate() != EHSRStatusOperationResult::Success
			|| AttackDefinition->Validate() != EHSRStatusOperationResult::Success || Coordinator->GetParticipants().Num() != 2
			|| ImmunityEffect->GetDefaultObject<UGameplayEffect>()->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			UE_LOG(LogTemp, Error, TEXT("P9-004 ImmunityDispel Harness=SKIPPED Reason=AssetGateNotReady"));
			return;
		}
		const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0]; const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1];
		UAbilitySystemComponent* ASC = Target.AbilitySystemComponent.Get(); UHSRStatusComponent* Component = Coordinator->GetStatusComponent(Target.ParticipantId);
		if (!ASC || !Component || !Coordinator->GetTurnManager()) { UE_LOG(LogTemp, Error, TEXT("P9-004 ImmunityDispel Harness=SKIPPED Reason=RuntimeNotReady")); return; }
		int32 Failed = 0;
		const auto Case = [&Failed](const TCHAR* Name, bool bPass) { if (bPass) { UE_LOG(LogTemp, Log, TEXT("P9-004 ImmunityDispel Case=%s Result=PASS"), Name); } else { ++Failed; UE_LOG(LogTemp, Error, TEXT("P9-004 ImmunityDispel Case=%s Result=FAIL"), Name); } };
		const FGameplayEffectSpecHandle ImmunitySpec = ASC->MakeOutgoingSpec(ImmunityEffect, 1.0f, ASC->MakeEffectContext());
		const FActiveGameplayEffectHandle ImmunityHandle = ImmunitySpec.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*ImmunitySpec.Data.Get()) : FActiveGameplayEffectHandle();
		const EHSRStatusOperationResult ImmuneDot = Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid());
		const EHSRStatusOperationResult ImmuneBreak = Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid());
		Case(TEXT("ImmunityRejectDoT"), ImmunityHandle.WasSuccessfullyApplied() && ImmuneDot == EHSRStatusOperationResult::Immune && Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId).InstanceCount == 0);
		Case(TEXT("ImmunityRejectBreak"), ImmuneBreak == EHSRStatusOperationResult::Immune && Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, BreakDefinition->StatusId).InstanceCount == 0);
		ASC->RemoveActiveGameplayEffect(ImmunityHandle);
		Coordinator->AddStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId); Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid()); Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid());
		const FGameplayEffectSpecHandle SentinelSpec = ASC->MakeOutgoingSpec(ImmunityEffect, 1.0f, ASC->MakeEffectContext()); const FActiveGameplayEffectHandle SentinelHandle = SentinelSpec.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*SentinelSpec.Data.Get()) : FActiveGameplayEffectHandle();
		Coordinator->SetDispelRemoveFailureForDevelopmentTest(true); const EHSRStatusOperationResult DispelFailure = Coordinator->DispelOneStatusForDevelopmentTest(Target.ParticipantId); Coordinator->SetDispelRemoveFailureForDevelopmentTest(false);
		Case(TEXT("DispelApplyFailure_PreservesHandle"), DispelFailure == EHSRStatusOperationResult::RemoveFailed && Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId).InstanceCount == 1);
		const EHSRStatusOperationResult Dispel = Coordinator->DispelOneStatusForDevelopmentTest(Target.ParticipantId);
		Case(TEXT("DispelOneDeterministic"), Dispel == EHSRStatusOperationResult::Dispelled && Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId).InstanceCount == 0);
		Case(TEXT("UndispellableBreakPreserved"), Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, BreakDefinition->StatusId).InstanceCount == 1);
		Case(TEXT("BuffPreserved"), Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, AttackDefinition->StatusId).InstanceCount == 1);
		Case(TEXT("SentinelPreserved"), ASC->GetActiveGameplayEffect(SentinelHandle) != nullptr);
		Case(TEXT("EmptyAndDuplicateDispel"), Coordinator->DispelOneStatusForDevelopmentTest(Target.ParticipantId) == EHSRStatusOperationResult::NoDispelCandidate);
		ASC->RemoveActiveGameplayEffect(SentinelHandle); Component->ClearStatus();
		Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid()); Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid());
		Coordinator->SetDispelRemoveFailureForDevelopmentTest(true); const int32 SourceFailureRemoved = Coordinator->RouteSourceInvalidForDevelopmentTest(Source.ParticipantId); Coordinator->SetDispelRemoveFailureForDevelopmentTest(false); const int32 SourceRemoved = Coordinator->RouteSourceInvalidForDevelopmentTest(Source.ParticipantId); const int32 SourceReplayRemoved = Coordinator->RouteSourceInvalidForDevelopmentTest(Source.ParticipantId);
		Case(TEXT("SourceDeathRemoveFailure_Retry"), SourceFailureRemoved == 0 && SourceRemoved == 1 && Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, DotDefinition->StatusId).InstanceCount == 0);
		Case(TEXT("SourceDeathKeep"), Coordinator->GetStatusSnapshotForDevelopmentTest(Target.ParticipantId, BreakDefinition->StatusId).InstanceCount == 1);
		Case(TEXT("DuplicateSourceEvent"), SourceReplayRemoved == 0);
		UHSRStatusDefinition* InvalidClassification = DuplicateObject<UHSRStatusDefinition>(DotDefinition, Coordinator); InvalidClassification->Classification = EHSRStatusClassification::Buff;
		UHSRStatusDefinition* InvalidImmunity = DuplicateObject<UHSRStatusDefinition>(DotDefinition, Coordinator); InvalidImmunity->ImmunityTag = DotDefinition->GrantedStatusTag;
		Case(TEXT("InvalidClassificationAndImmunityTag"), InvalidClassification->Validate() == EHSRStatusOperationResult::InvalidDefinition && InvalidImmunity->Validate() == EHSRStatusOperationResult::InvalidDefinition);
		UHSRStatusComponent* InvalidASCComponent = NewObject<UHSRStatusComponent>(Target.Actor.Get()); InvalidASCComponent->RegisterComponent(); InvalidASCComponent->InitializeStatusRuntime(Target.ParticipantId, ASC); InvalidASCComponent->BindTurnManager(Coordinator->GetTurnManager()); InvalidASCComponent->InvalidateAbilitySystemForDevelopmentTest();
		Case(TEXT("InvalidDefinitionASCAndTarget"), Component->AddOrRefreshStatus(nullptr, Source.ParticipantId, Target.ParticipantId) == EHSRStatusOperationResult::InvalidDefinition && InvalidASCComponent->AddOrRefreshStatus(DotDefinition, Source.ParticipantId, Target.ParticipantId) == EHSRStatusOperationResult::MissingAbilitySystem && Coordinator->DispelOneStatusForDevelopmentTest(NAME_None) == EHSRStatusOperationResult::InvalidTarget); InvalidASCComponent->DestroyComponent();
		UHSRStatusComponent* MultiSource = NewObject<UHSRStatusComponent>(Target.Actor.Get()); MultiSource->RegisterComponent(); MultiSource->InitializeStatusRuntime(Target.ParticipantId, ASC); MultiSource->BindTurnManager(Coordinator->GetTurnManager());
		MultiSource->AddOrRefreshStatus(DotDefinition, Source.ParticipantId, Target.ParticipantId); MultiSource->AddOrRefreshStatus(DotDefinition, Target.ParticipantId, Target.ParticipantId); const int32 OldSourceRemoved = MultiSource->HandleSourceInvalid(Source.ParticipantId); const int32 LatestSourceRemoved = MultiSource->HandleSourceInvalid(Target.ParticipantId);
		Case(TEXT("MultiSourceLatestSourcePolicy"), OldSourceRemoved == 0 && LatestSourceRemoved == 1); MultiSource->DestroyComponent();
		UHSRStatusComponent* EndPlayComponent = NewObject<UHSRStatusComponent>(Target.Actor.Get()); EndPlayComponent->RegisterComponent(); EndPlayComponent->InitializeStatusRuntime(Target.ParticipantId, ASC); EndPlayComponent->BindTurnManager(Coordinator->GetTurnManager()); EndPlayComponent->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const FActiveGameplayEffectHandle EndPlayHandle = EndPlayComponent->GetHandleForDevelopmentTest(BreakDefinition->StatusId); EndPlayComponent->DestroyComponent();
		Case(TEXT("EndPlayExactHandleCleanup"), !ASC->GetActiveGameplayEffect(EndPlayHandle));
		UHSRTurnManager* OldManager = NewObject<UHSRTurnManager>(Coordinator); OldManager->Initialize(Coordinator->GetParticipants()); UHSRTurnManager* ReplacementManager = NewObject<UHSRTurnManager>(Coordinator); ReplacementManager->Initialize(Coordinator->GetParticipants()); UHSRStatusComponent* ReplacementComponent = NewObject<UHSRStatusComponent>(Target.Actor.Get()); ReplacementComponent->RegisterComponent(); ReplacementComponent->InitializeStatusRuntime(Target.ParticipantId, ASC); ReplacementComponent->BindTurnManager(OldManager); ReplacementComponent->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const FActiveGameplayEffectHandle ReplacementHandle = ReplacementComponent->GetHandleForDevelopmentTest(BreakDefinition->StatusId); ReplacementComponent->BindTurnManager(ReplacementManager); ReplacementComponent->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const int32 OldRemaining = ReplacementComponent->GetSnapshot(BreakDefinition->StatusId).RemainingTurns; OldManager->ResolveAction(OldManager->GetCurrentParticipantId()); const int32 AfterOldRemaining = ReplacementComponent->GetSnapshot(BreakDefinition->StatusId).RemainingTurns; for (int32 Step=0; Step<2 && ReplacementComponent->GetSnapshot(BreakDefinition->StatusId).RemainingTurns==OldRemaining; ++Step) ReplacementManager->ResolveAction(ReplacementManager->GetCurrentParticipantId()); const int32 AfterNewRemaining = ReplacementComponent->GetSnapshot(BreakDefinition->StatusId).RemainingTurns;
		Case(TEXT("ManagerReplacementUnbindsOldEvents"), !ASC->GetActiveGameplayEffect(ReplacementHandle) && OldRemaining == AfterOldRemaining && AfterNewRemaining == OldRemaining - 1); ReplacementComponent->DestroyComponent();
		Component->ClearStatus();
		UWorld* BattleWorld = Target.Actor->GetWorld();
		Component->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const FActiveGameplayEffectHandle TargetDeathHandle = Component->GetHandleForDevelopmentTest(BreakDefinition->StatusId); const int32 DefeatBeforeTargetDeath = Coordinator->GetDefeatCountForDevelopmentTest(); Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f);
		Case(TEXT("TargetDeathProductionCleanup"), Coordinator->GetDefeatCountForDevelopmentTest() == DefeatBeforeTargetDeath + 1 && !ASC->GetActiveGameplayEffect(TargetDeathHandle));
		const FHSRBattleInitResult TargetDeathReset = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
		if (!TargetDeathReset.IsSuccess() || Coordinator->GetParticipants().Num() != 2)
		{
			++Failed; UE_LOG(LogTemp, Error, TEXT("P9-004 ImmunityDispel Case=TargetDeathResetSecondBattle Result=FAIL"));
		}
		else
		{
			const FHSRBattleParticipant& ResetSource = Coordinator->GetParticipants()[0]; const FHSRBattleParticipant& ResetTarget = Coordinator->GetParticipants()[1]; UHSRStatusComponent* ResetComponent = Coordinator->GetStatusComponent(ResetTarget.ParticipantId); ResetComponent->AddOrRefreshStatus(BreakDefinition, ResetSource.ParticipantId, ResetTarget.ParticipantId); const FActiveGameplayEffectHandle FinishedHandle = ResetComponent->GetHandleForDevelopmentTest(BreakDefinition->StatusId); UHSRTurnManager* ResetManager = Coordinator->GetTurnManager(); ResetManager->FinishBattle(); Coordinator->ClearStatusComponentsForDevelopmentTest(); const bool bLateFinished = ResetManager->ResolveAction(ResetManager->GetCurrentParticipantId());
			Case(TEXT("FinishedCleanupAndLateEvent"), !bLateFinished && !ResetTarget.AbilitySystemComponent->GetActiveGameplayEffect(FinishedHandle));
			const FHSRBattleInitResult FinishedReset = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld); Case(TEXT("ResetSecondBattleCleanRuntime"), FinishedReset.IsSuccess() && Coordinator->GetParticipants().Num() == 2);
		}
		if (Failed == 0) { UE_LOG(LogTemp, Log, TEXT("P9-004 ImmunityDispel Harness=COMPLETE")); } else { UE_LOG(LogTemp, Error, TEXT("P9-004 ImmunityDispel Harness=INCOMPLETE FailedCases=%d"), Failed); }
	}

	struct FP9StatusViewRecorder
	{
		TWeakObjectPtr<UHSRBattleCommandViewModel> ViewModel;
		int32 UpdateCount = 0;
		void Record(const FHSRBattleCommandViewState& State) { ++UpdateCount; if (ViewModel.IsValid()) ViewModel->SetState(State); }
	};

	static void RunP9StatusViewHarness(UHSRBattleCoordinator* Coordinator, const UHSRStatusDefinition* AttackDefinition, const UHSRStatusDefinition* StackDefinition, const UHSRStatusDefinition* DotDefinition, const UHSRStatusDefinition* BreakDefinition, TSubclassOf<UHSRBattleCommandWidget> WidgetClass)
	{
		if (!Coordinator || !AttackDefinition || !StackDefinition || !DotDefinition || !BreakDefinition || !WidgetClass || Coordinator->GetParticipants().Num() != 2 || AttackDefinition->Validate() != EHSRStatusOperationResult::Success || StackDefinition->Validate() != EHSRStatusOperationResult::Success || DotDefinition->Validate() != EHSRStatusOperationResult::Success || BreakDefinition->Validate() != EHSRStatusOperationResult::Success)
		{
			UE_LOG(LogTemp, Error, TEXT("P9-005 StatusView Harness=SKIPPED Reason=RuntimeOrAssetGateNotReady")); return;
		}
		const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0]; const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1]; UHSRStatusComponent* Component = Coordinator->GetStatusComponent(Target.ParticipantId);
		if (!Component) { UE_LOG(LogTemp, Error, TEXT("P9-005 StatusView Harness=SKIPPED Reason=MissingStatusComponent")); return; }
		UHSRBattleCommandViewModel* ViewModel = NewObject<UHSRBattleCommandViewModel>(Coordinator); ViewModel->BindCoordinator(Coordinator); FP9StatusViewRecorder Recorder; Recorder.ViewModel = ViewModel;
		const FDelegateHandle ReadyHandle = Coordinator->OnCommandStateReady().AddRaw(&Recorder, &FP9StatusViewRecorder::Record);
		int32 Failed = 0; const auto Case = [&Failed](const TCHAR* Name, bool bPass) { if (bPass) { UE_LOG(LogTemp, Log, TEXT("P9-005 StatusView Case=%s Result=PASS"), Name); } else { ++Failed; UE_LOG(LogTemp, Error, TEXT("P9-005 StatusView Case=%s Result=FAIL"), Name); } };
		const int32 BeforeAdd = Recorder.UpdateCount; Coordinator->AddStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId); const FHSRBattleCommandViewState Added = ViewModel->GetStateCopy();
		Case(TEXT("Add_EventDrivenPureSnapshot"), Recorder.UpdateCount == BeforeAdd + 1 && Added.Statuses.Num() == 1 && Added.Statuses[0].StatusId == AttackDefinition->StatusId && Added.Statuses[0].Stacks == 1 && Added.Statuses[0].RemainingTurns == 2);
		const int32 BeforeRefresh = Recorder.UpdateCount; Coordinator->AddStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId); const FHSRBattleCommandViewState Refreshed = ViewModel->GetStateCopy();
		Case(TEXT("Refresh_SameEntry"), Recorder.UpdateCount == BeforeRefresh + 1 && Refreshed.Statuses.Num() == 1 && Refreshed.Statuses[0].RemainingTurns == 2 && Refreshed.Statuses[0].LastResult == EHSRStatusOperationResult::Refreshed);
		Component->ClearStatus(); const int32 BeforeStack = Recorder.UpdateCount; Coordinator->AddSpecificStatusForDevelopmentTest(StackDefinition, Source.ParticipantId, Target.ParticipantId); Coordinator->AddSpecificStatusForDevelopmentTest(StackDefinition, Source.ParticipantId, Target.ParticipantId); const FHSRBattleCommandViewState Stacked = ViewModel->GetStateCopy();
		Case(TEXT("Stack_RealCoordinatorEntry"), Recorder.UpdateCount == BeforeStack + 2 && Stacked.Statuses.Num() == 1 && Stacked.Statuses[0].Stacks == 2 && Stacked.Statuses[0].LastResult == EHSRStatusOperationResult::StackAdded && !Stacked.Statuses[0].DisplayName.IsEmpty());
		Component->ClearStatus(); const int32 BeforeBreak = Recorder.UpdateCount; Coordinator->RequestBreakStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid()); const FHSRBattleCommandViewState BreakState = ViewModel->GetStateCopy();
		Case(TEXT("Break_ProductionRequest"), Recorder.UpdateCount == BeforeBreak + 1 && BreakState.Statuses.Num() == 1 && BreakState.Statuses[0].StatusId == BreakDefinition->StatusId && BreakState.Statuses[0].Classification == EHSRStatusClassification::Debuff);
		Component->ClearStatus();
		const int32 BeforeDot = Recorder.UpdateCount; Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid()); const FHSRBattleCommandViewState WithDot = ViewModel->GetStateCopy();
		Coordinator->AddStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId); const FHSRBattleCommandViewState MultiStatus = ViewModel->GetStateCopy();
		Case(TEXT("MultipleStatuses_StableOrder"), Recorder.UpdateCount == BeforeDot + 2 && MultiStatus.Statuses.Num() == 2 && MultiStatus.Statuses[0].StatusId.LexicalLess(MultiStatus.Statuses[1].StatusId));
		const int32 BeforeDispel = Recorder.UpdateCount; Coordinator->DispelOneStatusForDevelopmentTest(Target.ParticipantId); const FHSRBattleCommandViewState AfterDispel = ViewModel->GetStateCopy();
		Case(TEXT("Dispel_RemovesOneEntry"), Recorder.UpdateCount == BeforeDispel + 1 && AfterDispel.Statuses.Num() == 1 && AfterDispel.Statuses[0].Classification == EHSRStatusClassification::Buff
			&& AfterDispel.LastStatusOperation.Operation == EHSRStatusPublicOperation::Dispel && AfterDispel.LastStatusOperation.Result == EHSRStatusOperationResult::Dispelled
			&& AfterDispel.LastStatusOperation.StatusId == DotDefinition->StatusId && AfterDispel.LastStatusOperation.TargetParticipantId == Target.ParticipantId && AfterDispel.LastStatusOperation.Sequence > 0);
		Component->ClearStatus(); Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f); Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1000.0f); Coordinator->AddDamageOverTimeForDevelopmentTest(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid()); UHSRTurnManager* Manager = Coordinator->GetTurnManager();
		const auto ResolveTargetTurn = [Manager, &Target]() { for (int32 Step=0; Step<4; ++Step) { const FName Current=Manager->GetCurrentParticipantId(); const bool bTarget=Current==Target.ParticipantId; if(!Manager->ResolveAction(Current)) return false; if(bTarget) return true; } return false; };
		const bool bTriggerResolved = ResolveTargetTurn(); const FHSRBattleCommandViewState Triggered = ViewModel->GetStateCopy();
		Case(TEXT("DoTTrigger_RealTurnEvent"), bTriggerResolved && Triggered.Statuses.Num() == 1 && Triggered.Statuses[0].RemainingTurns == 1 && Triggered.Statuses[0].LastResult == EHSRStatusOperationResult::Triggered);
		const bool bExpireResolved = ResolveTargetTurn(); const FHSRBattleCommandViewState Expired = ViewModel->GetStateCopy();
		Case(TEXT("Expire_RealTurnEvent"), bExpireResolved && Expired.Statuses.IsEmpty() && Expired.LastStatusOperation.Operation == EHSRStatusPublicOperation::Expire
			&& Expired.LastStatusOperation.StatusId == DotDefinition->StatusId && Expired.LastStatusOperation.TargetParticipantId == Target.ParticipantId
			&& Expired.LastStatusOperation.Sequence > AfterDispel.LastStatusOperation.Sequence);
		const int32 BeforeClear = Recorder.UpdateCount; Component->ClearStatus(); const FHSRBattleCommandViewState Cleared = ViewModel->GetStateCopy();
		Case(TEXT("Clear_EmptySnapshot"), Recorder.UpdateCount == BeforeClear + 1 && Cleared.Statuses.IsEmpty());
		ViewModel->BindCoordinator(Coordinator); ViewModel->BindCoordinator(Coordinator); const int32 BeforeRebindUpdate = Recorder.UpdateCount; Coordinator->AddStatusForDevelopmentTest(Source.ParticipantId, Target.ParticipantId);
		Case(TEXT("ViewModelRebind_SingleStatusUpdate"), Recorder.UpdateCount == BeforeRebindUpdate + 1 && ViewModel->GetStateCopy().Statuses.Num() == 1); Component->ClearStatus();
		UHSRBattleCommandWidget* TestWidget = CreateWidget<UHSRBattleCommandWidget>(Target.Actor->GetWorld(), WidgetClass); bool bWidgetLifecycle = false;
		if (TestWidget)
		{
			TestWidget->AddToViewport(); const int32 GenerationBefore = TestWidget->GetBindGenerationForDevelopmentTest(); TestWidget->BindViewModel(ViewModel, Coordinator); TestWidget->BindViewModel(ViewModel, Coordinator); const bool bOneBinding = TestWidget->HasActiveViewModelBindingForDevelopmentTest() && TestWidget->GetBindGenerationForDevelopmentTest() == GenerationBefore + 2; TestWidget->RemoveFromParent(); bWidgetLifecycle = bOneBinding && !TestWidget->HasActiveViewModelBindingForDevelopmentTest();
		}
		Case(TEXT("WidgetRebindAndNativeDestruct"), bWidgetLifecycle);
		Component->AddOrRefreshStatus(BreakDefinition, Source.ParticipantId, Target.ParticipantId); const int32 BeforeFinished = Recorder.UpdateCount; Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 0.0f); const FHSRBattleCommandViewState FinishedState = ViewModel->GetStateCopy();
		Case(TEXT("Finished_EmptySnapshot"), Recorder.UpdateCount > BeforeFinished && FinishedState.Statuses.IsEmpty());
		const FHSRBattleInitResult ResetResult = Coordinator->ResetAndRebuildForDevelopmentTest(Target.Actor->GetWorld());
		Case(TEXT("Reset_EmptySnapshot"), ResetResult.IsSuccess() && ViewModel->GetStateCopy().Statuses.IsEmpty());
		Coordinator->OnCommandStateReady().Remove(ReadyHandle); ViewModel->UnbindCoordinator(); const int32 AfterUnbind = Recorder.UpdateCount; UHSRStatusComponent* FreshComponent = Coordinator->GetParticipants().Num() > 0 ? Coordinator->GetStatusComponent(Coordinator->GetParticipants()[0].ParticipantId) : nullptr; if (FreshComponent) FreshComponent->ClearStatus();
		Case(TEXT("Unbind_NoDuplicateCallback"), Recorder.UpdateCount == AfterUnbind);
		if (Failed == 0) { UE_LOG(LogTemp, Log, TEXT("P9-005 StatusView Harness=COMPLETE")); } else { UE_LOG(LogTemp, Error, TEXT("P9-005 StatusView Harness=INCOMPLETE FailedCases=%d"), Failed); }
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
	Coordinator->SetBasicAttackDefinition(BasicAttackSkillDefinition);
	Coordinator->SetUltimateDefinition(UltimateSkillDefinition);
	Coordinator->SetSkillDefinition(SkillSkillDefinition);
	Coordinator->SetHealDefinition(HealSkillDefinition);
	Coordinator->SetEnemyDefinition(EnemyDefinition);
	Coordinator->SetStatusDefinition(AttackUpStatusDefinition);
	Coordinator->SetDamageOverTimeStatusDefinition(DamageOverTimeStatusDefinition);
	Coordinator->SetBreakStatusDefinition(BreakStatusDefinition);
	Coordinator->SetParticipantInitializationGameplayEffect(ParticipantInitializationGameplayEffect);
#if WITH_EDITOR
	Coordinator->InitializeDevelopmentDamageRng(DevelopmentDamageSeed);
#endif
	UE_LOG(LogTemp, Log, TEXT("AHSRBattleGameMode::BeginPlay - BasicAttackSkillDefinition=%s UltimateSkillDefinition=%s SkillSkillDefinition=%s"),
		BasicAttackSkillDefinition ? *BasicAttackSkillDefinition->GetName() : TEXT("null"),
		UltimateSkillDefinition ? *UltimateSkillDefinition->GetName() : TEXT("null"), SkillSkillDefinition ? *SkillSkillDefinition->GetName() : TEXT("null"));

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

#if WITH_EDITOR
	if (bRunP8ContractHarness)
	{
		HSRBattleDevelopmentTest::RunP8ContractHarness(Coordinator);
	}
	if (bRunP9TurnLifecycleHarness)
	{
		HSRBattleDevelopmentTest::RunP9TurnLifecycleHarness(Coordinator);
	}
	if (bRunP9DotBreakHarness)
	{
		HSRBattleDevelopmentTest::RunP9DotBreakHarness(Coordinator, DamageOverTimeStatusDefinition, BreakStatusDefinition);
	}
	if (bRunP9ImmunityDispelHarness)
	{
		HSRBattleDevelopmentTest::RunP9ImmunityDispelHarness(Coordinator, DamageOverTimeStatusDefinition, BreakStatusDefinition, AttackUpStatusDefinition, DebuffImmunityGameplayEffect);
	}
	if (bRunP9StatusViewHarness)
	{
		HSRBattleDevelopmentTest::RunP9StatusViewHarness(Coordinator, AttackUpStatusDefinition, AttackUpStackStatusDefinition, DamageOverTimeStatusDefinition, BreakStatusDefinition, BattleCommandWidgetClass);
	}
	if (bRunP9StatusHarness)
	{
		HSRBattleDevelopmentTest::RunP9StackHarness(Coordinator, AttackUpStackStatusDefinition, AttackUpStatusDefinition);
		HSRBattleDevelopmentTest::RunP9StatusHarness(Coordinator);
	}
#endif

#if WITH_EDITOR
	if (bRunP7DamageHarness && Coordinator->GetParticipants().Num() >= 2)
	{
		const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0];
		const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1];
		const bool bValidParticipants = Source.AbilitySystemComponent.IsValid() && Target.AbilitySystemComponent.IsValid();
		if (!bValidParticipants)
		{
			UE_LOG(LogTemp, Error, TEXT("P7-002 Harness Result=FAIL Reason=InvalidParticipantASC"));
		}
		else
		{
			// Isolated non-lethal baseline: P7 must not depend on uninitialized P7-001 assets.
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), 10.0f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritRateAttribute(), 0.0f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritDamageAttribute(), 0.0f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), 0.0f);
			// Formal cards always submit Player. Make the current actor explicit;
			// P7-002's direct helper does not require turn ownership.
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
			const auto RunPreflightCase = [this, &Source, &Target](const TCHAR* CaseName, FName CaseSource, FName CaseTarget, const FGameplayTag& CaseTag, float CaseMultiplier, const UHSRDamageRuleDefinition* CaseRule, TSubclassOf<UGameplayEffect> CaseEffect, EHSRDamageResultType Expected)
			{
				const float HPBefore = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
				const float SourceEnergyBefore = Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
				const float TargetEnergyBefore = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
				const int32 RNGBefore = Coordinator->GetDevelopmentDamageConsumeCount();
				const FName TurnBefore = Coordinator->GetTurnManager() ? Coordinator->GetTurnManager()->GetCurrentParticipantId() : NAME_None;
				const FHSRTeamResourceState ResourcesBefore = Coordinator->GetTeamResourceState();
				const FHSRDamageResult Result = Coordinator->ResolveDevelopmentExecutionDamage(CaseSource, CaseTarget, FGuid::NewGuid(), CaseTag, CaseMultiplier, CaseRule, CaseEffect);
				const FHSRTeamResourceState& ResourcesAfter = Coordinator->GetTeamResourceState();
				const bool bNoMutation = FMath::IsNearlyEqual(HPBefore, Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()))
					&& RNGBefore == Coordinator->GetDevelopmentDamageConsumeCount()
					&& TurnBefore == (Coordinator->GetTurnManager() ? Coordinator->GetTurnManager()->GetCurrentParticipantId() : NAME_None)
					&& ResourcesBefore.CurrentSkillPoints == ResourcesAfter.CurrentSkillPoints && ResourcesBefore.MaxSkillPoints == ResourcesAfter.MaxSkillPoints
					&& FMath::IsNearlyEqual(SourceEnergyBefore, Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()))
					&& FMath::IsNearlyEqual(TargetEnergyBefore, Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()));
				if (Result.Result == Expected && bNoMutation)
				{
					UE_LOG(LogTemp, Log, TEXT("P7-002 Matrix Case=%s Result=%d Expected=%d HPBefore=%.2f HPAfter=%.2f RNGBefore=%d RNGAfter=%d NoMutation=%d"), CaseName, static_cast<int32>(Result.Result), static_cast<int32>(Expected), HPBefore, Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), RNGBefore, Coordinator->GetDevelopmentDamageConsumeCount(), bNoMutation);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("P7-002 Matrix Case=%s Result=%d Expected=%d HPBefore=%.2f HPAfter=%.2f RNGBefore=%d RNGAfter=%d NoMutation=%d"), CaseName, static_cast<int32>(Result.Result), static_cast<int32>(Expected), HPBefore, Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()), RNGBefore, Coordinator->GetDevelopmentDamageConsumeCount(), bNoMutation);
				}
			};
			RunPreflightCase(TEXT("InvalidSource"), NAME_None, Target.ParticipantId, DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect, EHSRDamageResultType::InvalidSource);
			RunPreflightCase(TEXT("InvalidTarget"), Source.ParticipantId, NAME_None, DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect, EHSRDamageResultType::InvalidTarget);
			RunPreflightCase(TEXT("MissingRule"), Source.ParticipantId, Target.ParticipantId, DevelopmentDamageType, DevelopmentAbilityMultiplier, nullptr, DevelopmentDamageExecutionGameplayEffect, EHSRDamageResultType::MissingDamageRule);
			RunPreflightCase(TEXT("InvalidTag"), Source.ParticipantId, Target.ParticipantId, FGameplayTag(), DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect, EHSRDamageResultType::InvalidDamageType);
			RunPreflightCase(TEXT("InvalidMultiplier"), Source.ParticipantId, Target.ParticipantId, DevelopmentDamageType, 0.0f, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect, EHSRDamageResultType::InvalidDamageType);
			RunPreflightCase(TEXT("MissingGE"), Source.ParticipantId, Target.ParticipantId, DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, nullptr, EHSRDamageResultType::SpecCreationFailed);
			const auto RunFormulaCase = [this, &Source, &Target](const TCHAR* CaseName, float Attack, float Defense, float CritRate, float CritDamage, float ExpectedFinal)
			{
				Coordinator->InitializeDevelopmentDamageRng(DevelopmentDamageSeed);
				Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), Attack);
				Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritRateAttribute(), CritRate);
				Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritDamageAttribute(), CritDamage);
				Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), Defense);
				Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
				const int32 RNGBefore = Coordinator->GetDevelopmentDamageConsumeCount();
				const FHSRDamageResult Result = Coordinator->ResolveDevelopmentExecutionDamage(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid(), DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect);
				const float HPAfter = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
				const bool bPass = Result.Result == EHSRDamageResultType::DamageResolved && FMath::IsNearlyEqual(Result.Breakdown.FinalDamage, ExpectedFinal)
					&& FMath::IsNearlyEqual(Result.Breakdown.AppliedDamage, ExpectedFinal) && FMath::IsNearlyEqual(100.0f - HPAfter, ExpectedFinal)
					&& Coordinator->GetDevelopmentDamageConsumeCount() == RNGBefore + 1 && Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Spawned;
				UE_LOG(LogTemp, Log, TEXT("P7-002 Matrix Case=%s Result=%s Raw=%.2f Final=%.2f Applied=%.2f ExpectedFinal=%.2f HPBefore=100.00 HPAfter=%.2f RNGBefore=%d RNGAfter=%d"), CaseName, bPass ? TEXT("PASS") : TEXT("FAIL"), Result.Breakdown.RawDamage, Result.Breakdown.FinalDamage, Result.Breakdown.AppliedDamage, ExpectedFinal, HPAfter, RNGBefore, Coordinator->GetDevelopmentDamageConsumeCount());
			};
			RunFormulaCase(TEXT("MinDamage_AttackZero"), 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
			RunFormulaCase(TEXT("DefenseZero"), 10.0f, 0.0f, 0.0f, 0.0f, 10.0f);
			RunFormulaCase(TEXT("DefenseHigh_MinDamage"), 10.0f, 100.0f, 0.0f, 0.0f, 1.0f);
			RunFormulaCase(TEXT("CritRateZero"), 10.0f, 0.0f, 0.0f, 0.5f, 10.0f);
			RunFormulaCase(TEXT("CritRateOne_CritDamageHalf"), 10.0f, 0.0f, 1.0f, 0.5f, 15.0f);

			// Reinitialization is the safe P7 development reset boundary: it clears cache and resets RNG to the supplied seed.
			Coordinator->InitializeDevelopmentDamageRng(DevelopmentDamageSeed);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), 10.0f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritRateAttribute(), 0.5f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetCritDamageAttribute(), 0.5f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), 0.0f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
			const FHSRDamageResult DeterminismA = Coordinator->ResolveDevelopmentExecutionDamage(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid(), DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect);
			Coordinator->InitializeDevelopmentDamageRng(DevelopmentDamageSeed);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
			const FHSRDamageResult DeterminismB = Coordinator->ResolveDevelopmentExecutionDamage(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid(), DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect);
			const bool bDeterministic = DeterminismA.Result == DeterminismB.Result && FMath::IsNearlyEqual(DeterminismA.Breakdown.RawDamage, DeterminismB.Breakdown.RawDamage) && FMath::IsNearlyEqual(DeterminismA.Breakdown.FinalDamage, DeterminismB.Breakdown.FinalDamage) && DeterminismA.Breakdown.bCritical == DeterminismB.Breakdown.bCritical && Coordinator->GetDevelopmentDamageConsumeCount() == 1;
			UE_LOG(LogTemp, Log, TEXT("P7-002 Matrix Case=SameSeedDeterminism Result=%s FirstFinal=%.2f SecondFinal=%.2f FirstCritical=%d SecondCritical=%d RNGAfterResetRun=%d"), bDeterministic ? TEXT("PASS") : TEXT("FAIL"), DeterminismA.Breakdown.FinalDamage, DeterminismB.Breakdown.FinalDamage, DeterminismA.Breakdown.bCritical, DeterminismB.Breakdown.bCritical, Coordinator->GetDevelopmentDamageConsumeCount());
			UHSRBattleCoordinator* TerminalCoordinator = NewObject<UHSRBattleCoordinator>(this);
			const int32 MainRNGBeforeTerminal = Coordinator->GetDevelopmentDamageConsumeCount();
			const FHSRDamageResult TerminalResult = TerminalCoordinator->ResolveDevelopmentExecutionDamage(Source.ParticipantId, Target.ParticipantId, FGuid::NewGuid(), DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect);
			const bool bTerminalPass = TerminalResult.Result == EHSRDamageResultType::BattleTerminal && Coordinator->GetDevelopmentDamageConsumeCount() == MainRNGBeforeTerminal && Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Spawned;
			UE_LOG(LogTemp, Log, TEXT("P7-002 Matrix Case=BattleTerminalIsolated Result=%s DamageResult=%d MainRNGBefore=%d MainRNGAfter=%d"), bTerminalPass ? TEXT("PASS") : TEXT("FAIL"), static_cast<int32>(TerminalResult.Result), MainRNGBeforeTerminal, Coordinator->GetDevelopmentDamageConsumeCount());

			const FGuid ActionId = FGuid::NewGuid();
			const float HealthBefore = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
			const float SourceEnergyBefore = Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
			const float TargetEnergyBefore = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
			const FName TurnBefore = Coordinator->GetTurnManager() ? Coordinator->GetTurnManager()->GetCurrentParticipantId() : NAME_None;
			const FHSRTeamResourceState ResourcesBefore = Coordinator->GetTeamResourceState();
			const int32 RngBefore = Coordinator->GetDevelopmentDamageConsumeCount();
			const EHSRBattleCoordinatorState StateBefore = Coordinator->GetCurrentState();
			const FHSRDamageResult First = Coordinator->ResolveDevelopmentExecutionDamage(Source.ParticipantId, Target.ParticipantId, ActionId, DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect);
			const float HealthAfterFirst = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
			const int32 RngAfterFirst = Coordinator->GetDevelopmentDamageConsumeCount();
			const FHSRDamageResult Second = Coordinator->ResolveDevelopmentExecutionDamage(Source.ParticipantId, Target.ParticipantId, ActionId, DevelopmentDamageType, DevelopmentAbilityMultiplier, DevelopmentDamageRule, DevelopmentDamageExecutionGameplayEffect);
			const float HealthAfterSecond = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
			const int32 RngAfterSecond = Coordinator->GetDevelopmentDamageConsumeCount();
			const bool bResultMatch = First.Result == Second.Result
				&& FMath::IsNearlyEqual(First.Breakdown.RawDamage, Second.Breakdown.RawDamage)
				&& FMath::IsNearlyEqual(First.Breakdown.FinalDamage, Second.Breakdown.FinalDamage)
				&& FMath::IsNearlyEqual(First.Breakdown.AppliedDamage, Second.Breakdown.AppliedDamage);
			const bool bExactlyOnce = RngAfterFirst == RngBefore + 1 && RngAfterSecond == RngAfterFirst && FMath::IsNearlyEqual(HealthAfterFirst, HealthAfterSecond);
			const bool bDamageBalanced = First.Result == EHSRDamageResultType::DamageResolved && First.Breakdown.FinalDamage > 0.0f
				&& FMath::IsNearlyEqual(First.Breakdown.AppliedDamage, First.Breakdown.FinalDamage)
				&& FMath::IsNearlyEqual(HealthBefore - HealthAfterFirst, First.Breakdown.FinalDamage);
			const FHSRTeamResourceState& ResourcesAfter = Coordinator->GetTeamResourceState();
			const bool bNoBattleSideEffects = StateBefore == EHSRBattleCoordinatorState::Spawned && Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Spawned
				&& TurnBefore == (Coordinator->GetTurnManager() ? Coordinator->GetTurnManager()->GetCurrentParticipantId() : NAME_None)
				&& ResourcesBefore.CurrentSkillPoints == ResourcesAfter.CurrentSkillPoints && ResourcesBefore.MaxSkillPoints == ResourcesAfter.MaxSkillPoints
				&& FMath::IsNearlyEqual(SourceEnergyBefore, Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()))
				&& FMath::IsNearlyEqual(TargetEnergyBefore, Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()));
			const bool bPassed = bResultMatch && bExactlyOnce && bDamageBalanced && bNoBattleSideEffects;
			if (bPassed)
			{
				UE_LOG(LogTemp, Log, TEXT("P7-002 Harness Result=PASS ActionId=%s FirstResult=%d SecondResult=%d HPBefore=%.2f HPAfterFirst=%.2f HPAfterSecond=%.2f Raw=%.2f Final=%.2f Applied=%.2f RNGBefore=%d RNGAfterFirst=%d RNGAfterSecond=%d StateBefore=%d StateAfter=%d ResultMatch=%d ExactlyOnce=%d DamageBalanced=%d NoBattleSideEffects=%d"), *ActionId.ToString(), static_cast<int32>(First.Result), static_cast<int32>(Second.Result), HealthBefore, HealthAfterFirst, HealthAfterSecond, First.Breakdown.RawDamage, First.Breakdown.FinalDamage, First.Breakdown.AppliedDamage, RngBefore, RngAfterFirst, RngAfterSecond, static_cast<int32>(StateBefore), static_cast<int32>(Coordinator->GetCurrentState()), bResultMatch, bExactlyOnce, bDamageBalanced, bNoBattleSideEffects);
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("P7-002 Harness Result=FAIL ActionId=%s FirstResult=%d SecondResult=%d HPBefore=%.2f HPAfterFirst=%.2f HPAfterSecond=%.2f Raw=%.2f Final=%.2f Applied=%.2f RNGBefore=%d RNGAfterFirst=%d RNGAfterSecond=%d StateBefore=%d StateAfter=%d ResultMatch=%d ExactlyOnce=%d DamageBalanced=%d NoBattleSideEffects=%d"), *ActionId.ToString(), static_cast<int32>(First.Result), static_cast<int32>(Second.Result), HealthBefore, HealthAfterFirst, HealthAfterSecond, First.Breakdown.RawDamage, First.Breakdown.FinalDamage, First.Breakdown.AppliedDamage, RngBefore, RngAfterFirst, RngAfterSecond, static_cast<int32>(StateBefore), static_cast<int32>(Coordinator->GetCurrentState()), bResultMatch, bExactlyOnce, bDamageBalanced, bNoBattleSideEffects);
			}

			// P7-003 formal path: do not substitute the direct P7-002 helper.
			// Every card below invokes RequestAction and therefore verifies the
			// Coordinator -> prepared ability -> unique Apply seam.
			const auto RunFormalCard = [this, &Source, &Target](const TCHAR* CaseName, const UHSRSkillDefinition* Definition, int32 ExpectedSPDelta, bool bUltimate)
			{
				if (!Definition || !Definition->DamageRule.IsValid() || Definition->EffectGameplayEffectClass.IsNull() || (bUltimate && Definition->EnergyRefundGameplayEffectClass.IsNull()))
				{
					UE_LOG(LogTemp, Warning, TEXT("P7-003 Formal Case=%s Result=SKIPPED Reason=MissingP7AssetBinding"), CaseName); return;
				}
				Coordinator->InitializeDevelopmentDamageRng(DevelopmentDamageSeed);
				Coordinator->SetTeamSkillPointsForDevelopmentTest(2, 3);
				Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants());
				// Ultimate's configured formal damage can exceed the old 100 HP
				// Harness baseline. Keep every card non-lethal so it cannot publish a
				// BattleResult and poison subsequent P5/P6 smoke tests.
				const float TargetBaselineHealth = bUltimate ? 1000.0f : 100.0f;
				Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), TargetBaselineHealth);
				Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), TargetBaselineHealth);
				Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), 100.0f);
				Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), 100.0f);
				const int32 SPBefore = Coordinator->GetTeamResourceState().CurrentSkillPoints;
				const float EnergyBefore = Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
				const float HPBefore = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
				FHSRBattleActionCommand Command; Command.ActionId = FGuid::NewGuid(); Command.BattleId = Coordinator->GetCurrentRequestId(); Command.ActorParticipantId = Source.ParticipantId; Command.SkillId = Definition->SkillId; Command.TargetParticipantIds.Add(Target.ParticipantId);
				Coordinator->ClearDamageTestInjection();
				Coordinator->SetDamageTestInjectionForAction(Command.ActionId, EHSRDamageTestInjection::None);
				const FHSRAbilityResolution FirstResolution = Coordinator->RequestAction(Command);
				const float HPAfter = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
				const float EnergyAfter = Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
				const int32 SPAfter = Coordinator->GetTeamResourceState().CurrentSkillPoints;
				const FHSRAbilityResolution ReplayResolution = Coordinator->RequestAction(Command);
				const bool bPass = FirstResolution.Succeeded() && ReplayResolution.Succeeded() && FirstResolution.bHasDamageResult && ReplayResolution.bHasDamageResult
					&& FMath::IsNearlyEqual(HPAfter, Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()))
					&& Coordinator->GetTeamResourceState().CurrentSkillPoints == SPAfter && (SPAfter - SPBefore) == ExpectedSPDelta
					&& (!bUltimate || EnergyAfter < EnergyBefore);
				if (bPass)
				{
					UE_LOG(LogTemp, Log, TEXT("P7-003 Formal Case=%s Result=PASS ActionId=%s GE=BP_GE_P7_DamageExecution HPBefore=%.2f HPAfter=%.2f SPBefore=%d SPAfter=%d EnergyBefore=%.2f EnergyAfter=%.2f DuplicateSame=%d"), CaseName, *Command.ActionId.ToString(), HPBefore, HPAfter, SPBefore, SPAfter, EnergyBefore, EnergyAfter, ReplayResolution.Succeeded() ? 1 : 0);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("P7-003 Formal Case=%s Result=FAIL ActionId=%s GE=BP_GE_P7_DamageExecution HPBefore=%.2f HPAfter=%.2f SPBefore=%d SPAfter=%d EnergyBefore=%.2f EnergyAfter=%.2f DuplicateSame=%d"), CaseName, *Command.ActionId.ToString(), HPBefore, HPAfter, SPBefore, SPAfter, EnergyBefore, EnergyAfter, ReplayResolution.Succeeded() ? 1 : 0);
				}
			};
			RunFormalCard(TEXT("Basic_NonLethal_Duplicate"), BasicAttackSkillDefinition, 1, false);
			RunFormalCard(TEXT("Skill_NonLethal_Duplicate"), SkillSkillDefinition, -1, false);
			RunFormalCard(TEXT("Ultimate_NonLethal_Duplicate"), UltimateSkillDefinition, 0, true);
			UE_LOG(LogTemp, Log, TEXT("P7-003 Formal Harness Note=ApplyFailureAndRefundRequiresDevelopmentInjectionAtCoordinatorSpecApply; CaptureFailed/InvalidCapturedValueRequireReadOnlyExecutionCaptureInjectionAndAreNotSafelyInjectableWithinAllowlist. Heal=ExistingP6Path OldDamageGE=NoFormalRuntimeReference"));
		}
	}
#endif

	Coordinator->OnBattleResultReady().AddUObject(this, &AHSRBattleGameMode::HandleBattleResultReady);
	CommandViewModel = NewObject<UHSRBattleCommandViewModel>(this);
	CommandViewModel->BindCoordinator(Coordinator);
	CommandStateReadyHandle = Coordinator->OnCommandStateReady().AddUObject(this, &AHSRBattleGameMode::HandleCommandStateReady);
	HandleCommandStateReady(Coordinator->GetCommandViewState());
	if (BattleCommandWidgetClass)
	{
		BattleCommandWidget = CreateWidget<UHSRBattleCommandWidget>(GetWorld(), BattleCommandWidgetClass);
		if (BattleCommandWidget)
		{
			BattleCommandWidget->AddToViewport();
			UE_LOG(LogTemp, Log, TEXT("P6-004A GameMode WidgetCreate Result=SUCCESS Class=%s Widget=%s"), *BattleCommandWidgetClass->GetName(), *BattleCommandWidget->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("P6-004A GameMode WidgetCreate Result=FAIL Class=%s"), *BattleCommandWidgetClass->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("P6-004A GameMode WidgetCreate Result=SKIPPED Reason=BattleCommandWidgetClassNotConfigured"));
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

#if WITH_EDITOR
	Coordinator->ClearDamageTestInjection();
	if (bRunLegacyBattleHarnesses)
	{
		HSRBattleDevelopmentTest::Run(Coordinator);
		HSRBattleDevelopmentTest::RunP6Ultimate(Coordinator, UltimateSkillDefinition);
		HSRBattleDevelopmentTest::RunP6SkillPoints(Coordinator, SkillSkillDefinition);
		HSRBattleDevelopmentTest::RunP6HealAndViewState(Coordinator, HealSkillDefinition);
	}
	if (bRunP7DamageHarness && Coordinator && Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Spawned && Coordinator->GetParticipants().Num() == 2)
	{
		const auto RunInjectedFailure = [this](const TCHAR* CaseName, EHSRDamageTestInjection Injection, const UHSRSkillDefinition* Definition, EHSRDamageResultType Expected)
		{
			const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0]; const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1];
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f); Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
			Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f); Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 1000.0f);
			Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), 100.0f); Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(), 100.0f);
			Coordinator->SetTeamSkillPointsForDevelopmentTest(2, 3); Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants());
			const float HPBefore = Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); const float EnergyBefore = Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()); const int32 SPBefore = Coordinator->GetTeamResourceState().CurrentSkillPoints; const int32 RNGBefore = Coordinator->GetDevelopmentDamageConsumeCount(); const FName TurnBefore = Coordinator->GetTurnManager()->GetCurrentParticipantId();
			FHSRBattleActionCommand Command; Command.ActionId=FGuid::NewGuid(); Command.BattleId=Coordinator->GetCurrentRequestId(); Command.ActorParticipantId=Source.ParticipantId; Command.SkillId=Definition->SkillId; Command.TargetParticipantIds.Add(Target.ParticipantId);
			Coordinator->ClearDamageTestInjection(); Coordinator->SetDamageTestInjectionForAction(Command.ActionId,Injection); const FHSRAbilityResolution First=Coordinator->RequestAction(Command); const FHSRFormalDamageExecutionResult Execution=Coordinator->GetLastDevelopmentFormalExecutionResult(); Coordinator->ClearDamageTestInjection(); const FHSRAbilityResolution Replay=Coordinator->RequestAction(Command);
			const float HPAfter=Target.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()); const float EnergyAfter=Source.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute());
			const bool bRefundCase=Injection==EHSRDamageTestInjection::ForcePostCostApplyFailure;
			const bool bPass=First.Status==EHSRAbilityResolutionStatus::Rejected && First.bHasDamageResult && First.DamageResult.Result==Expected && Replay.Status==First.Status && FMath::IsNearlyEqual(HPBefore,HPAfter) && FMath::IsNearlyEqual(EnergyBefore,EnergyAfter) && SPBefore==Coordinator->GetTeamResourceState().CurrentSkillPoints && RNGBefore==Coordinator->GetDevelopmentDamageConsumeCount() && TurnBefore==Coordinator->GetTurnManager()->GetCurrentParticipantId() && Coordinator->GetCurrentState()==EHSRBattleCoordinatorState::Spawned && (!bRefundCase || (Execution.bCostCommitted && Execution.bRefundApplied && FMath::IsNearlyEqual(Execution.EnergyBefore,100.0f) && FMath::IsNearlyEqual(Execution.EnergyAfter,100.0f)));
			if (bPass) { UE_LOG(LogTemp,Log,TEXT("P7-004 Matrix Case=%s Result=PASS ActionId=%s DamageResult=%d HP=%.2f Energy=%.2f SP=%d RNG=%d Turn=%s Cost=%d Refund=%d EnergyExec=%.2f->%.2f"),CaseName,*Command.ActionId.ToString(),static_cast<int32>(First.DamageResult.Result),HPAfter,EnergyAfter,Coordinator->GetTeamResourceState().CurrentSkillPoints,Coordinator->GetDevelopmentDamageConsumeCount(),*TurnBefore.ToString(),Execution.bCostCommitted?1:0,Execution.bRefundApplied?1:0,Execution.EnergyBefore,Execution.EnergyAfter); }
			else { UE_LOG(LogTemp,Error,TEXT("P7-004 Matrix Case=%s Result=FAIL ActionId=%s DamageResult=%d HPBefore=%.2f HPAfter=%.2f EnergyBefore=%.2f EnergyAfter=%.2f SPBefore=%d SPAfter=%d RNGBefore=%d RNGAfter=%d"),CaseName,*Command.ActionId.ToString(),static_cast<int32>(First.DamageResult.Result),HPBefore,HPAfter,EnergyBefore,EnergyAfter,SPBefore,Coordinator->GetTeamResourceState().CurrentSkillPoints,RNGBefore,Coordinator->GetDevelopmentDamageConsumeCount()); }
		};
		RunInjectedFailure(TEXT("CaptureFailed"),EHSRDamageTestInjection::ForceCaptureFailed,BasicAttackSkillDefinition,EHSRDamageResultType::CaptureFailed);
		RunInjectedFailure(TEXT("InvalidCapturedValue"),EHSRDamageTestInjection::ForceInvalidCapturedValue,BasicAttackSkillDefinition,EHSRDamageResultType::InvalidCapturedValue);
		RunInjectedFailure(TEXT("UltimatePostCostApplyFailureRefund"),EHSRDamageTestInjection::ForcePostCostApplyFailure,UltimateSkillDefinition,EHSRDamageResultType::EffectApplicationFailed);
		const FHSRBattleParticipant& OverkillSource=Coordinator->GetParticipants()[0]; const FHSRBattleParticipant& OverkillTarget=Coordinator->GetParticipants()[1];
		OverkillSource.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(),120.0f); OverkillTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(),80.0f); OverkillSource.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(),10.0f); OverkillTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(),100.0f); OverkillTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(),1.0f); Coordinator->SetTeamSkillPointsForDevelopmentTest(1,3); Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants());
		FHSRBattleActionCommand OverkillCommand; OverkillCommand.ActionId=FGuid::NewGuid(); OverkillCommand.BattleId=Coordinator->GetCurrentRequestId(); OverkillCommand.ActorParticipantId=OverkillSource.ParticipantId; OverkillCommand.SkillId=BasicAttackSkillDefinition->SkillId; OverkillCommand.TargetParticipantIds.Add(OverkillTarget.ParticipantId);
		const int32 OverkillRNGBefore=Coordinator->GetDevelopmentDamageConsumeCount(); const FHSRAbilityResolution OverkillFirst=Coordinator->RequestAction(OverkillCommand); const int32 OverkillRNGAfter=Coordinator->GetDevelopmentDamageConsumeCount(); const int32 OverkillSPAfter=Coordinator->GetTeamResourceState().CurrentSkillPoints; const FHSRAbilityResolution OverkillReplay=Coordinator->RequestAction(OverkillCommand); FHSRBattleActionCommand TerminalCommand=OverkillCommand; TerminalCommand.ActionId=FGuid::NewGuid(); const FHSRAbilityResolution TerminalRejected=Coordinator->RequestAction(TerminalCommand);
		const bool bOverkillPass=OverkillFirst.Succeeded() && OverkillFirst.bHasDamageResult && OverkillFirst.DamageResult.Breakdown.FinalDamage>OverkillFirst.DamageResult.Breakdown.AppliedDamage && FMath::IsNearlyEqual(OverkillFirst.DamageResult.Breakdown.AppliedDamage,1.0f) && FMath::IsNearlyEqual(OverkillTarget.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()),0.0f) && Coordinator->GetCurrentState()==EHSRBattleCoordinatorState::Finished && OverkillReplay.Succeeded() && TerminalRejected.Status==EHSRAbilityResolutionStatus::Rejected && Coordinator->GetDevelopmentDamageConsumeCount()==OverkillRNGAfter && Coordinator->GetTeamResourceState().CurrentSkillPoints==OverkillSPAfter;
		if(bOverkillPass){UE_LOG(LogTemp,Log,TEXT("P7-004 Matrix Case=Overkill Result=PASS ActionId=%s Final=%.2f Applied=%.2f HPAfter=0 RNG=%d->%d SP=%d TerminalReason=%d"),*OverkillCommand.ActionId.ToString(),OverkillFirst.DamageResult.Breakdown.FinalDamage,OverkillFirst.DamageResult.Breakdown.AppliedDamage,OverkillRNGBefore,OverkillRNGAfter,OverkillSPAfter,static_cast<int32>(TerminalRejected.FailureReason));}else{UE_LOG(LogTemp,Error,TEXT("P7-004 Matrix Case=Overkill Result=FAIL ActionId=%s Final=%.2f Applied=%.2f State=%d RNG=%d->%d"),*OverkillCommand.ActionId.ToString(),OverkillFirst.DamageResult.Breakdown.FinalDamage,OverkillFirst.DamageResult.Breakdown.AppliedDamage,static_cast<int32>(Coordinator->GetCurrentState()),OverkillRNGBefore,Coordinator->GetDevelopmentDamageConsumeCount());}
		const FHSRBattleInitResult ResetResult=Coordinator->ResetAndRebuildForDevelopmentTest(GetWorld());
		bool bResetSamePass=false; float ResetHPBefore=0.0f,ResetHPAfter=0.0f; int32 ResetRNGAfter=Coordinator->GetDevelopmentDamageConsumeCount();
		if(ResetResult.IsSuccess()&&Coordinator->GetParticipants().Num()==2){const FHSRBattleParticipant& NewSource=Coordinator->GetParticipants()[0];const FHSRBattleParticipant& NewTarget=Coordinator->GetParticipants()[1];NewSource.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(),120.0f);NewTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(),80.0f);NewSource.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(),10.0f);NewTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(),100.0f);NewTarget.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(),100.0f);Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants());ResetHPBefore=NewTarget.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());FHSRBattleActionCommand Reused=OverkillCommand;Reused.BattleId=Coordinator->GetCurrentRequestId();const FHSRAbilityResolution Fresh=Coordinator->RequestAction(Reused);ResetHPAfter=NewTarget.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());ResetRNGAfter=Coordinator->GetDevelopmentDamageConsumeCount();bResetSamePass=Fresh.Succeeded()&&ResetHPAfter<ResetHPBefore&&ResetRNGAfter==1&&Coordinator->GetCurrentState()==EHSRBattleCoordinatorState::Spawned;}
		if (bResetSamePass) { UE_LOG(LogTemp,Log,TEXT("P7-004 Matrix Case=RealResetSameActionId Result=PASS ActionId=%s BattleId=%s State=%d Participants=%d HP=%.2f->%.2f RNG=%d SP=%d"),*OverkillCommand.ActionId.ToString(),*Coordinator->GetCurrentRequestId().ToString(),static_cast<int32>(Coordinator->GetCurrentState()),Coordinator->GetParticipants().Num(),ResetHPBefore,ResetHPAfter,ResetRNGAfter,Coordinator->GetTeamResourceState().CurrentSkillPoints); }
		else { UE_LOG(LogTemp,Error,TEXT("P7-004 Matrix Case=RealResetSameActionId Result=FAIL State=%d Participants=%d HP=%.2f->%.2f RNG=%d SP=%d"),static_cast<int32>(Coordinator->GetCurrentState()),Coordinator->GetParticipants().Num(),ResetHPBefore,ResetHPAfter,ResetRNGAfter,Coordinator->GetTeamResourceState().CurrentSkillPoints); }
	}
	RunTerminalScenarioForDevelopment();
#endif
}

void AHSRBattleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CommandViewModel)
	{
		CommandViewModel->UnbindCoordinator();
	}
	if (BattleCommandWidget)
	{
		BattleCommandWidget->RemoveFromParent();
		BattleCommandWidget = nullptr;
	}
	if (Coordinator)
	{
		if (CommandStateReadyHandle.IsValid())
		{
			Coordinator->OnCommandStateReady().Remove(CommandStateReadyHandle);
			CommandStateReadyHandle.Reset();
		}
		Coordinator->Reset();
	}
	Coordinator = nullptr;
	CommandViewModel = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AHSRBattleGameMode::HandleCommandStateReady(const FHSRBattleCommandViewState& State)
{
	if (CommandViewModel)
	{
		CommandViewModel->SetState(State);
	}
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

	const FHSRExplorationReturnResult ReturnResult = Subsystem->RequestBattleReturn(ConsumedResult);
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
