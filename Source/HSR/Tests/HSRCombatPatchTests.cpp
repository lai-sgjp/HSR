#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRTurnManager.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Status/HSRStatusComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStatusGenericPatchTest, "HSR.Battle.Patch.StatusGeneric", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRRepeatableBreakPatchTest, "HSR.Battle.Patch.RepeatableBreak", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRRepeatableBreakPatchTest::RunTest(const FString& Parameters)
{
	if (!TestNotNull(TEXT("Engine is available"), GEngine)) return false;
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	if (!TestNotNull(TEXT("Standalone GameInstance is created"), GameInstance)) return false;
	GameInstance->AddToRoot();
	UWorld* BattleWorld = nullptr;
	UHSRBattleCoordinator* Coordinator = nullptr;
	ON_SCOPE_EXIT
	{
		if (Coordinator) Coordinator->Reset();
		if (GameInstance)
		{
			GameInstance->Shutdown();
			if (BattleWorld)
			{
				BattleWorld->DestroyWorld(false);
				GEngine->DestroyWorldContext(BattleWorld);
			}
			GameInstance->RemoveFromRoot();
		}
	};
	GameInstance->InitializeStandalone(FName(*FString::Printf(TEXT("HSRRepeatableBreak_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits))));
	BattleWorld = GameInstance->GetWorld();
	if (!TestNotNull(TEXT("InitializeStandalone creates a World"), BattleWorld)
		|| !TestEqual(TEXT("World owns the standalone GameInstance"), BattleWorld->GetGameInstance(), GameInstance)) return false;

	TSubclassOf<AHSRBattleGameMode> GameModeClass = LoadClass<AHSRBattleGameMode>(nullptr, TEXT("/Game/Blueprints/Framework/BP_HSRBattleGameMode.BP_HSRBattleGameMode_C"));
	if (!TestNotNull(TEXT("Configured battle GameMode loads"), GameModeClass.Get())) return false;
	FText Failure;
	Coordinator = AHSRBattleGameMode::CreateRepeatableBreakAutomationFixture(GameInstance, BattleWorld, GameModeClass, Failure);
	if (!TestNotNull(*FString::Printf(TEXT("Production fixture builds: %s"), *Failure.ToString()), Coordinator)) return false;

	const auto Prepare = [this, Coordinator](float Toughness, bool bWeakness, float Health = 1000.0f)
	{
		if (Coordinator->GetParticipants().Num() != 2 || !Coordinator->GetBasicAttackDefinition()) return false;
		const FHSRBattleParticipant& Source = Coordinator->GetParticipants()[0];
		const FHSRBattleParticipant& Target = Coordinator->GetParticipants()[1];
		if (!Source.AbilitySystemComponent.IsValid() || !Target.AbilitySystemComponent.IsValid() || !Coordinator->GetTurnManager()) return false;
		const UHSRSkillDefinition* Skill = Coordinator->GetBasicAttackDefinition();
		const FString Element = Skill->ElementTag.ToString();
		const FGameplayTag Weakness = Element.StartsWith(TEXT("Element."))
			? FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Weakness.%s"), *Element.RightChop(8))), false) : FGameplayTag();
		FHSRBattleParticipant& MutableTarget = const_cast<FHSRBattleParticipant&>(Target);
		MutableTarget.WeaknessTags.Reset();
		if (bWeakness && Weakness.IsValid()) MutableTarget.WeaknessTags.AddTag(Weakness);
		Source.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 120.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 80.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 1000.0f);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), Health);
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxToughnessAttribute(), FMath::Max(Toughness, Skill->ToughnessDamage));
		Target.AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetToughnessAttribute(), Toughness);
		Coordinator->SetTeamSkillPointsForDevelopmentTest(1, 3);
		return Coordinator->GetTurnManager()->Initialize(Coordinator->GetParticipants());
	};
	const auto CommandFor = [Coordinator](const FGuid& ActionId, const FGuid& BattleId = FGuid())
	{
		FHSRBattleActionCommand Command;
		Command.ActionId = ActionId;
		Command.BattleId = BattleId.IsValid() ? BattleId : Coordinator->GetCurrentRequestId();
		Command.ActorParticipantId = Coordinator->GetParticipants()[0].ParticipantId;
		Command.SkillId = Coordinator->GetBasicAttackDefinition()->SkillId;
		Command.TargetParticipantIds.Add(Coordinator->GetParticipants()[1].ParticipantId);
		return Command;
	};
	const auto Counts = [Coordinator]() { return FIntPoint(Coordinator->GetBreakStatusRequestCountForDevelopmentTest(), Coordinator->GetBreakDelayRegistrationCountForDevelopmentTest()); };
	const float BreakDamage = Coordinator->GetBasicAttackDefinition()->ToughnessDamage;
	if (!TestTrue(TEXT("Configured attack has Toughness damage"), BreakDamage > 0.0f) || !Prepare(BreakDamage, true)) return false;

	const FGuid FirstId = FGuid::NewGuid();
	const FIntPoint BeforeFirst = Counts();
	const uint64 TurnBeforeFirst = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution First = Coordinator->RequestAction(CommandFor(FirstId));
	TestTrue(TEXT("First positive-to-zero edge triggers Break"), First.Succeeded() && First.bHasBreakResult && First.BreakResult.bTriggered);
	TestEqual(TEXT("First edge reaches zero Toughness"), First.BreakResult.ToughnessAfter, 0.0f);
	TestEqual(TEXT("First edge adds one successful Status"), Counts().X, BeforeFirst.X + 1);
	TestEqual(TEXT("First edge adds one accepted Delay"), Counts().Y, BeforeFirst.Y + 1);
	TestEqual(TEXT("First Status result succeeds"), Coordinator->GetLastBreakStatusResultForDevelopmentTest(), EHSRStatusOperationResult::Success);
	TestTrue(TEXT("First Delay is accepted"), Coordinator->WasLastBreakDelayAcceptedForDevelopmentTest());
	TestTrue(TEXT("First action advances the turn"), Coordinator->GetTurnManager()->GetTurnSequence() > TurnBeforeFirst);
	const FIntPoint BeforeReplay = Counts();
	const uint64 TurnBeforeReplay = Coordinator->GetTurnManager()->GetTurnSequence();
	const FHSRAbilityResolution Replay = Coordinator->RequestAction(CommandFor(FirstId));
	TestEqual(TEXT("Replay returns cached Break ActionId"), Replay.BreakResult.ActionId, First.BreakResult.ActionId);
	TestEqual(TEXT("Replay has zero Status delta"), Counts().X, BeforeReplay.X);
	TestEqual(TEXT("Replay has zero Delay delta"), Counts().Y, BeforeReplay.Y);
	TestEqual(TEXT("Replay has zero turn delta"), Coordinator->GetTurnManager()->GetTurnSequence(), TurnBeforeReplay);

	Coordinator->GetParticipants()[1].AbilitySystemComponent->SetNumericAttributeBase(UHSRCoreAttributeSet::GetToughnessAttribute(), BreakDamage);
	TestEqual(TEXT("Recovery has zero Status delta"), Counts().X, BeforeReplay.X);
	TestEqual(TEXT("Recovery has zero Delay delta"), Counts().Y, BeforeReplay.Y);
	// Advance the existing turn lifecycle until the one-turn Break status has
	// naturally expired. This is fixture progression, not a recovery rule.
	UHSRStatusComponent* BreakComponent = Coordinator->GetStatusComponent(Coordinator->GetParticipants()[1].ParticipantId);
	for (int32 Step = 0; BreakComponent && Step < 4
		&& BreakComponent->GetSnapshot(TEXT("Status.Debuff.Break")).InstanceCount > 0; ++Step)
	{
		Coordinator->GetTurnManager()->ResolveAction(Coordinator->GetTurnManager()->GetCurrentParticipantId());
	}
	TestTrue(TEXT("Second action runtime prepares"), Prepare(BreakDamage, true));
	const FGuid SecondId = FGuid::NewGuid();
	const FHSRAbilityResolution Second = Coordinator->RequestAction(CommandFor(SecondId));
	TestTrue(TEXT("Second independent edge triggers Break"), Second.bHasBreakResult && Second.BreakResult.bTriggered && SecondId != FirstId);
	TestEqual(TEXT("Two edges produce two Status successes"), Counts().X, BeforeFirst.X + 2);
	TestEqual(TEXT("Two edges produce two accepted Delays"), Counts().Y, BeforeFirst.Y + 2);

	TestTrue(TEXT("Zero-to-zero runtime prepares"), Prepare(0.0f, true));
	const FIntPoint BeforeZero = Counts();
	const FHSRAbilityResolution Zero = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestTrue(TEXT("Initial zero does not trigger Break"), Zero.bHasBreakResult && !Zero.BreakResult.bTriggered);
	TestEqual(TEXT("Initial zero has zero Break side effects"), Counts(), BeforeZero);
	TestTrue(TEXT("Continued zero runtime prepares"), Prepare(0.0f, true));
	Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestEqual(TEXT("Continued zero has zero Break side effects"), Counts(), BeforeZero);

	TestTrue(TEXT("Non-zero runtime prepares"), Prepare(BreakDamage * 2.0f, true));
	const FIntPoint BeforeNonZero = Counts();
	const FHSRAbilityResolution NonZero = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestTrue(TEXT("Non-zero result does not trigger Break"), NonZero.bHasBreakResult && !NonZero.BreakResult.bTriggered && NonZero.ToughnessResult.After > 0.0f);
	TestEqual(TEXT("Non-zero result has zero Break side effects"), Counts(), BeforeNonZero);
	TestTrue(TEXT("No-weakness runtime prepares"), Prepare(BreakDamage, false));
	const FIntPoint BeforeNoWeakness = Counts();
	Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestEqual(TEXT("No weakness has zero Break side effects"), Counts(), BeforeNoWeakness);

	const FGuid OldBattleId = Coordinator->GetCurrentRequestId();
	// FirstId was processed in the old battle and must become valid again in
	// the fresh battle-local resolution cache.
	const FGuid ReusedId = FirstId;
	const FHSRBattleInitResult Rebuild = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
	TestTrue(TEXT("Reset rebuild succeeds"), Rebuild.IsSuccess());
	TestTrue(TEXT("New battle has a new BattleId"), Coordinator->GetCurrentRequestId() != OldBattleId);
	TestTrue(TEXT("New battle runtime prepares"), Prepare(BreakDamage, true));
	const FIntPoint BeforeStale = Counts();
	const FHSRAbilityResolution Stale = Coordinator->RequestAction(CommandFor(FGuid::NewGuid(), OldBattleId));
	TestFalse(TEXT("Old BattleId is rejected"), Stale.Succeeded());
	TestEqual(TEXT("Old BattleId has zero side effects"), Counts(), BeforeStale);
	const FHSRAbilityResolution FreshReused = Coordinator->RequestAction(CommandFor(ReusedId));
	TestTrue(TEXT("ActionId is battle-local after Reset"), FreshReused.bHasBreakResult && FreshReused.BreakResult.bTriggered);

	const FHSRBattleInitResult LethalRebuild = Coordinator->ResetAndRebuildForDevelopmentTest(BattleWorld);
	TestTrue(TEXT("Lethal matrix rebuild succeeds"), LethalRebuild.IsSuccess());
	TestTrue(TEXT("Lethal runtime prepares"), Prepare(BreakDamage, true, 1.0f));
	const FIntPoint BeforeLethal = Counts();
	const int32 DefeatBefore = Coordinator->GetDefeatCountForDevelopmentTest();
	const FHSRAbilityResolution Lethal = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestTrue(TEXT("Same-frame lethal publishes Break before terminal defeat"), Lethal.bHasBreakResult && Lethal.BreakResult.bTriggered
		&& Counts() == BeforeLethal + FIntPoint(1, 1) && Coordinator->GetDefeatCountForDevelopmentTest() == DefeatBefore + 1
		&& Coordinator->GetCurrentState() == EHSRBattleCoordinatorState::Finished);
	const FIntPoint BeforeFinished = Counts();
	const FHSRAbilityResolution Finished = Coordinator->RequestAction(CommandFor(FGuid::NewGuid()));
	TestFalse(TEXT("Finished battle rejects new action"), Finished.Succeeded());
	TestEqual(TEXT("Finished battle has zero Break side effects"), Counts(), BeforeFinished);
	return true;
}

bool FHSRStatusGenericPatchTest::RunTest(const FString& Parameters)
{
	UGameplayEffect* EffectDefault = UGameplayEffect::StaticClass()->GetDefaultObject<UGameplayEffect>();
	const EGameplayEffectDurationType OriginalDurationPolicy = EffectDefault->DurationPolicy;
	const EGameplayEffectStackingType OriginalStackingType = EffectDefault->StackingType;
	EffectDefault->DurationPolicy = EGameplayEffectDurationType::Infinite;
	EffectDefault->StackingType = EGameplayEffectStackingType::AggregateByTarget;

	const auto MakeTransientBuff = [](FName StatusId, EHSRStatusRefreshPolicy Policy, int32 MaxStacks, int32 DurationTurns)
	{
		UHSRStatusDefinition* Definition = NewObject<UHSRStatusDefinition>();
		Definition->StatusId = StatusId;
		Definition->GrantedStatusTag = FGameplayTag::RequestGameplayTag(StatusId, false);
		Definition->InfiniteGameplayEffectClass = UGameplayEffect::StaticClass();
		Definition->Classification = EHSRStatusClassification::Buff;
		Definition->EffectKind = EHSRStatusEffectKind::TagOnly;
		Definition->RefreshPolicy = Policy;
		Definition->DurationTurns = DurationTurns;
		Definition->MaxStacks = MaxStacks;
		return Definition;
	};

	UHSRStatusDefinition* Attack = MakeTransientBuff(TEXT("Status.Buff.AttackUp"), EHSRStatusRefreshPolicy::RefreshDuration, 1, 2);
	UHSRStatusDefinition* Speed = MakeTransientBuff(TEXT("Status.Buff.SpeedUp"), EHSRStatusRefreshPolicy::AddStack, 2, 2);
	UHSRStatusDefinition* Shield = MakeTransientBuff(TEXT("Status.Buff.Shield"), EHSRStatusRefreshPolicy::RefreshDuration, 1, 1);
	TestEqual(TEXT("Attack transient definition is field-valid"), Attack->Validate(), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed transient definition is field-valid"), Speed->Validate(), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Shield transient definition is field-valid"), Shield->Validate(), EHSRStatusOperationResult::Success);

	Speed->GrantedStatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff.AttackUp"), false);
	TestEqual(TEXT("Tag/id mismatch is structured"), Speed->Validate(), EHSRStatusOperationResult::InvalidDefinition);
	Speed->GrantedStatusTag = FGameplayTag::RequestGameplayTag(TEXT("Status.Buff.SpeedUp"), false);
	Shield->StatusId = TEXT("Buff.Shield");
	TestEqual(TEXT("Invalid root is structured"), Shield->Validate(), EHSRStatusOperationResult::InvalidStatusId);
	Shield->StatusId = TEXT("Status.Buff.Shield");
	Shield->RefreshPolicy = EHSRStatusRefreshPolicy::AddStack;
	Shield->MaxStacks = 1;
	TestEqual(TEXT("Invalid policy is structured"), Shield->Validate(), EHSRStatusOperationResult::InvalidPolicy);
	Shield->RefreshPolicy = EHSRStatusRefreshPolicy::RefreshDuration;
	Shield->MaxStacks = 1;

	UWorld* World = UWorld::CreateWorld(EWorldType::GamePreview, false);
	AActor* Actor = World->SpawnActor<AActor>();
	UAbilitySystemComponent* AbilitySystem = Cast<UAbilitySystemComponent>(Actor->AddComponentByClass(UHSRAbilitySystemComponent::StaticClass(), false, FTransform::Identity, false));
	const UHSRCoreAttributeSet* Attributes = Cast<UHSRCoreAttributeSet>(AbilitySystem->InitStats(UHSRCoreAttributeSet::StaticClass(), nullptr));
	TestNotNull(TEXT("Transient core attributes initialize"), Attributes);
	AbilitySystem->InitAbilityActorInfo(Actor, Actor);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->MaxHealth.SetBaseValue(100.0f);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->MaxHealth.SetCurrentValue(100.0f);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->Health.SetBaseValue(100.0f);
	const_cast<UHSRCoreAttributeSet*>(Attributes)->Health.SetCurrentValue(100.0f);
	AbilitySystem->SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(), 100.0f);
	AbilitySystem->SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), 100.0f);
	TestTrue(TEXT("Transient participant has positive health"), AbilitySystem->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute()) > 0.0f);
	FHSRBattleParticipant Participant;
	Participant.ParticipantId = TEXT("StatusGenericTarget");
	Participant.DefinitionId = TEXT("StatusGenericDefinition");
	Participant.Team = EHSRBattleParticipantTeam::Player;
	Participant.InitiativeSpeed = 100.0f;
	Participant.Actor = Actor;
	Participant.AbilitySystemComponent = AbilitySystem;
	UHSRTurnManager* TurnManager = NewObject<UHSRTurnManager>();
	TestTrue(TEXT("Transient turn manager initializes"), TurnManager->Initialize({ Participant }));
	UHSRStatusComponent* Component = NewObject<UHSRStatusComponent>(Actor);
	TestTrue(TEXT("Transient status component initializes"), Component->InitializeStatusRuntime(Participant.ParticipantId, AbilitySystem));
	TestTrue(TEXT("Transient status component binds turn manager"), Component->BindTurnManager(TurnManager));

	const FGuid AttackOperation = FGuid::NewGuid();
	TestEqual(TEXT("Attack adds through generic runtime"), Component->AddOrRefreshStatus(Attack, Participant.ParticipantId, Participant.ParticipantId, AttackOperation), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Duplicate operation is ignored"), Component->AddOrRefreshStatus(Attack, Participant.ParticipantId, Participant.ParticipantId, AttackOperation), EHSRStatusOperationResult::IgnoredEvent);
	FHSRStatusPublicSnapshot AttackSnapshot;
	FHSRStatusPublicSnapshot SpeedSnapshot;
	FHSRStatusPublicSnapshot ShieldSnapshot;
	TestEqual(TEXT("Attack typed query succeeds"), Component->GetPublicSnapshot(Attack->StatusId, AttackSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Attack begins with one stack"), AttackSnapshot.Stacks, 1);
	TestEqual(TEXT("Attack refreshes through generic runtime"), Component->AddOrRefreshStatus(Attack, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Refreshed);

	FHSRTurnLifecycleEvent FirstEnd;
	FirstEnd.EventType = EHSRTurnLifecycleEventType::TurnEnded;
	FirstEnd.BattleEpoch = TurnManager->GetBattleEpoch();
	FirstEnd.ParticipantId = Participant.ParticipantId;
	FirstEnd.TurnSequence = 1;
	Component->ConsumeLifecycleEventForDevelopmentTest(FirstEnd);

	FHSRTurnLifecycleEvent SecondEnd = FirstEnd;
	SecondEnd.TurnSequence = 2;
	Component->ConsumeLifecycleEventForDevelopmentTest(SecondEnd);
	TestEqual(TEXT("Attack expires after refreshed duration"), Component->GetPublicSnapshot(Attack->StatusId, AttackSnapshot), EHSRStatusOperationResult::UnknownStatus);

	TestEqual(TEXT("Speed adds through generic runtime"), Component->AddOrRefreshStatus(Speed, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed typed query succeeds"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed begins with one stack"), SpeedSnapshot.Stacks, 1);
	TestEqual(TEXT("Speed stacks through generic runtime"), Component->AddOrRefreshStatus(Speed, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::StackAdded);
	TestEqual(TEXT("Speed stack count is public"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Speed exposes two stacks"), SpeedSnapshot.Stacks, 2);
	FHSRTurnLifecycleEvent SpeedFirstEnd = FirstEnd;
	SpeedFirstEnd.TurnSequence = 3;
	Component->ConsumeLifecycleEventForDevelopmentTest(SpeedFirstEnd);
	TestEqual(TEXT("Speed keeps stacked state after one turn"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::Success);
	FHSRTurnLifecycleEvent SpeedSecondEnd = FirstEnd;
	SpeedSecondEnd.TurnSequence = 4;
	Component->ConsumeLifecycleEventForDevelopmentTest(SpeedSecondEnd);
	TestEqual(TEXT("Speed expires after stacked duration"), Component->GetPublicSnapshot(Speed->StatusId, SpeedSnapshot), EHSRStatusOperationResult::UnknownStatus);

	TestEqual(TEXT("Shield adds through generic runtime"), Component->AddOrRefreshStatus(Shield, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Shield typed query succeeds"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Shield begins with one stack"), ShieldSnapshot.Stacks, 1);
	FHSRTurnLifecycleEvent ShieldEnd = FirstEnd;
	ShieldEnd.TurnSequence = 5;
	Component->ConsumeLifecycleEventForDevelopmentTest(ShieldEnd);
	TestEqual(TEXT("Shield expires on its first target turn"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::UnknownStatus);
	TestEqual(TEXT("Shield re-adds for clear coverage"), Component->AddOrRefreshStatus(Shield, Participant.ParticipantId, Participant.ParticipantId), EHSRStatusOperationResult::Success);
	Component->SetForceClearRemoveFailureForDevelopmentTest(true);
	TestEqual(TEXT("Controlled clear removal failure is structured"), Component->ClearStatus(), EHSRStatusOperationResult::RemoveFailed);
	TestEqual(TEXT("Failed clear retains typed ownership"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::Success);
	TestTrue(TEXT("Failed clear retains an active handle"), Component->GetSnapshot(Shield->StatusId).bHandleActiveInAbilitySystem);
	Component->SetForceClearRemoveFailureForDevelopmentTest(false);
	TestEqual(TEXT("Clear retry succeeds"), Component->ClearStatus(), EHSRStatusOperationResult::Success);
	TestEqual(TEXT("Clear retry removes typed ownership"), Component->GetPublicSnapshot(Shield->StatusId, ShieldSnapshot), EHSRStatusOperationResult::UnknownStatus);

	FHSRStatusPublicSnapshot UnknownSnapshot;
	TestEqual(TEXT("Unknown lookup returns structured result"), Component->GetPublicSnapshot(TEXT("Status.Buff.Unknown"), UnknownSnapshot), EHSRStatusOperationResult::UnknownStatus);
	TestEqual(TEXT("Unknown lookup preserves requested id"), UnknownSnapshot.StatusId, FName(TEXT("Status.Buff.Unknown")));
	EffectDefault->DurationPolicy = OriginalDurationPolicy;
	EffectDefault->StackingType = OriginalStackingType;
	World->DestroyWorld(false);
	return true;
}

#endif
