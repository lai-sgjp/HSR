#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "AbilitySystemComponent.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../Battle/HSRBattleParticipant.h"
#include "../Battle/HSRStageBuffAuthority.h"
#include "../Data/Definitions/HSRStageBuffDefinition.h"
#include "GameplayEffect.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStageBuffAuthorityContractTest,
	"HSR.Battle.StageBuff.AuthorityContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRStageBuffAuthorityContractTest::RunTest(const FString&)
{
	UHSRStageBuffAuthority* Authority = NewObject<UHSRStageBuffAuthority>();
	UHSRStageBuffDefinition* Valid = NewObject<UHSRStageBuffDefinition>(Authority);
	Valid->BuffId = TEXT("Buff.Stage.Attack");
	Valid->ResourceItemId = TEXT("Item.StageToken");
	Valid->ResourceCost = 2;
	Valid->GameplayEffectClass = UGameplayEffect::StaticClass();
	UHSRStageBuffDefinition* Locked = NewObject<UHSRStageBuffDefinition>(Authority);
	Locked->BuffId = TEXT("Buff.Stage.Locked");
	Locked->bEnabled = false;
	Locked->GameplayEffectClass = UGameplayEffect::StaticClass();

	TestTrue(TEXT("register valid encounter buff definitions"),
		Authority->RegisterEncounterBuffs(TEXT("Encounter.Stage"), {Valid, Locked}));
	TestTrue(TEXT("valid buff accepted"), Authority->ValidateBuffIds(TEXT("Encounter.Stage"),
		{TEXT("Buff.Stage.Attack")}));
	TestFalse(TEXT("unknown buff rejected"), Authority->ValidateBuffIds(TEXT("Encounter.Stage"),
		{TEXT("Buff.Stage.Unknown")}));
	TestFalse(TEXT("locked buff rejected"), Authority->ValidateBuffIds(TEXT("Encounter.Stage"),
		{TEXT("Buff.Stage.Locked")}));
	TestFalse(TEXT("duplicate buff rejected"), Authority->ValidateBuffIds(TEXT("Encounter.Stage"),
		{TEXT("Buff.Stage.Attack"), TEXT("Buff.Stage.Attack")}));
	TestTrue(TEXT("empty selection is valid"), Authority->ValidateBuffIds(TEXT("Encounter.Stage"), {}));

	UHSRStageBuffDefinition* MissingEffect = NewObject<UHSRStageBuffDefinition>(Authority);
	MissingEffect->BuffId = TEXT("Buff.Stage.MissingEffect");
	TestFalse(TEXT("missing GameplayEffect rejects registration"),
		Authority->RegisterEncounterBuffs(TEXT("Encounter.Stage"), {MissingEffect}));
	TestEqual(TEXT("failed registration preserves prior valid definition"),
		Authority->FindBuff(TEXT("Encounter.Stage"), TEXT("Buff.Stage.Attack")),
		static_cast<const UHSRStageBuffDefinition*>(Valid));

	UHSRStageBuffDefinition* MissingResource = NewObject<UHSRStageBuffDefinition>(Authority);
	MissingResource->BuffId = TEXT("Buff.Stage.MissingResource");
	MissingResource->GameplayEffectClass = UGameplayEffect::StaticClass();
	MissingResource->ResourceCost = 1;
	TestFalse(TEXT("positive cost without resource item rejects registration"),
		Authority->RegisterEncounterBuffs(TEXT("Encounter.Other"), {MissingResource}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRStageBuffPlayerResolutionTest,
	"HSR.Battle.StageBuff.PlayerResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHSRStageBuffPlayerResolutionTest::RunTest(const FString&)
{
	UHSRBattleCoordinator* Coordinator = NewObject<UHSRBattleCoordinator>();
	Coordinator->SetPlayerCharacterDefinition(TEXT("Character.A"), nullptr);

	FHSRBattleParticipant RuntimePlayer;
	RuntimePlayer.ParticipantId = TEXT("Player");
	RuntimePlayer.Team = EHSRBattleParticipantTeam::Player;
	RuntimePlayer.AbilitySystemComponent = NewObject<UAbilitySystemComponent>(Coordinator);
	Coordinator->SetParticipantsForEquipmentProjectionDevelopmentTest({RuntimePlayer});

	TestTrue(TEXT("Stage Buff resolves player by team when runtime and character IDs differ"),
		Coordinator->HasStageBuffPlayerForDevelopmentTest());
	return true;
}

#endif
