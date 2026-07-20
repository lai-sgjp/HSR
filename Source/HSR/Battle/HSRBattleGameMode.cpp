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
#include "../UI/HSRBattleCommandViewModel.h"
#include "../UI/HSRBattleCommandWidget.h"
#include "Blueprint/UserWidget.h"

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
	HSRBattleDevelopmentTest::Run(Coordinator);
	HSRBattleDevelopmentTest::RunP6Ultimate(Coordinator, UltimateSkillDefinition);
	HSRBattleDevelopmentTest::RunP6SkillPoints(Coordinator, SkillSkillDefinition);
	HSRBattleDevelopmentTest::RunP6HealAndViewState(Coordinator, HealSkillDefinition);
	RunTerminalScenarioForDevelopment();
#endif
}

void AHSRBattleGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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
