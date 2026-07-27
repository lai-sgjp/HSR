#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameplayTagContainer.h"
#include "../Data/Definitions/HSRStatusDefinition.h"
#include "../Status/HSRStatusComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStatusGenericPatchTest, "HSR.Battle.Patch.StatusGeneric", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRStatusGenericPatchTest::RunTest(const FString& Parameters)
{
	const auto MakeTransientBuff = [](FName StatusId)
	{
		UHSRStatusDefinition* Definition = NewObject<UHSRStatusDefinition>();
		Definition->StatusId = StatusId;
		Definition->GrantedStatusTag = FGameplayTag::RequestGameplayTag(StatusId, false);
		Definition->InfiniteGameplayEffectClass = UGameplayEffect::StaticClass();
		Definition->Classification = EHSRStatusClassification::Buff;
		Definition->EffectKind = EHSRStatusEffectKind::TagOnly;
		Definition->RefreshPolicy = EHSRStatusRefreshPolicy::RefreshDuration;
		Definition->DurationTurns = 2;
		Definition->MaxStacks = 1;
		return Definition;
	};

	UHSRStatusDefinition* Attack = MakeTransientBuff(TEXT("Status.Buff.AttackUp"));
	UHSRStatusDefinition* Speed = MakeTransientBuff(TEXT("Status.Buff.SpeedUp"));
	UHSRStatusDefinition* Shield = MakeTransientBuff(TEXT("Status.Buff.Shield"));
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
	TestEqual(TEXT("Invalid stack policy is structured"), Shield->Validate(), EHSRStatusOperationResult::InvalidPolicy);

	UHSRStatusComponent* Component = NewObject<UHSRStatusComponent>();
	FHSRStatusPublicSnapshot Snapshot;
	TestEqual(TEXT("Unknown lookup returns structured result"), Component->GetPublicSnapshot(TEXT("Status.Buff.Unknown"), Snapshot), EHSRStatusOperationResult::UnknownStatus);
	TestEqual(TEXT("Unknown lookup preserves requested id"), Snapshot.StatusId, FName(TEXT("Status.Buff.Unknown")));
	TestEqual(TEXT("Unknown lookup is recorded in snapshot"), Snapshot.LastResult, EHSRStatusOperationResult::UnknownStatus);
	return true;
}

#endif
