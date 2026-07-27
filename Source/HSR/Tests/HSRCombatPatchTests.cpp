#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRTurnManager.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../GAS/HSRAbilitySystemComponent.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Status/HSRStatusComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStatusGenericPatchTest, "HSR.Battle.Patch.StatusGeneric", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

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
