#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../Data/HSRSkillDefinition.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"

namespace HSR::Demo::ContentCatalogTests
{
	template <typename TObjectType>
	TObjectType* LoadAsset(const TCHAR* ObjectPath)
	{
		return LoadObject<TObjectType>(nullptr, ObjectPath);
	}

	template <typename TObjectType>
	TObjectType* RequireAsset(FAutomationTestBase& Test, const TCHAR* ObjectPath)
	{
		TObjectType* Asset = LoadAsset<TObjectType>(ObjectPath);
		Test.TestNotNull(*FString::Printf(TEXT("formal Demo asset exists: %s"), ObjectPath), Asset);
		return Asset;
	}

	struct FSkillSpec
	{
		const TCHAR* ObjectPath;
		const TCHAR* SkillId;
		EHSRSkillCategory Category;
		EHSRTargetType TargetType;
		float Multiplier;
		int32 SkillPointDelta;
		float EnergyCost;
		float ToughnessDamage;
	};

	static const FSkillSpec SkillSpecs[] =
	{
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_EvernightMoon_Basic.DA_Skill_EvernightMoon_Basic"), TEXT("Demo.Skill.EvernightMoon.Basic"), EHSRSkillCategory::BasicAttack, EHSRTargetType::SingleEnemy, 1.00f, 1, 0.0f, 1.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_EvernightMoon_Nightfall.DA_Skill_EvernightMoon_Nightfall"), TEXT("Demo.Skill.EvernightMoon.Nightfall"), EHSRSkillCategory::Skill, EHSRTargetType::SingleEnemy, 1.55f, -1, 0.0f, 2.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_EvernightMoon_FullMoon.DA_Skill_EvernightMoon_FullMoon"), TEXT("Demo.Skill.EvernightMoon.FullMoon"), EHSRSkillCategory::Ultimate, EHSRTargetType::SingleEnemy, 2.40f, 0, 100.0f, 3.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Huohua_Basic.DA_Skill_Huohua_Basic"), TEXT("Demo.Skill.Huohua.Basic"), EHSRSkillCategory::BasicAttack, EHSRTargetType::SingleEnemy, 0.80f, 1, 0.0f, 1.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Huohua_BloomingStage.DA_Skill_Huohua_BloomingStage"), TEXT("Demo.Skill.Huohua.BloomingStage"), EHSRSkillCategory::Skill, EHSRTargetType::SingleEnemy, 1.10f, -1, 0.0f, 1.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Huohua_GrandOpening.DA_Skill_Huohua_GrandOpening"), TEXT("Demo.Skill.Huohua.GrandOpening"), EHSRSkillCategory::Ultimate, EHSRTargetType::SingleEnemy, 1.70f, 0, 100.0f, 2.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Remiel_Basic.DA_Skill_Remiel_Basic"), TEXT("Demo.Skill.Remiel.Basic"), EHSRSkillCategory::BasicAttack, EHSRTargetType::SingleEnemy, 0.90f, 1, 0.0f, 2.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Remiel_Riftlight.DA_Skill_Remiel_Riftlight"), TEXT("Demo.Skill.Remiel.Riftlight"), EHSRSkillCategory::Skill, EHSRTargetType::SingleEnemy, 1.25f, -1, 0.0f, 4.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Remiel_SkyJudgment.DA_Skill_Remiel_SkyJudgment"), TEXT("Demo.Skill.Remiel.SkyJudgment"), EHSRSkillCategory::Ultimate, EHSRTargetType::SingleEnemy, 1.90f, 0, 100.0f, 6.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Verina_Basic.DA_Skill_Verina_Basic"), TEXT("Demo.Skill.Verina.Basic"), EHSRSkillCategory::BasicAttack, EHSRTargetType::SingleEnemy, 0.70f, 1, 0.0f, 1.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Verina_HealingSprout.DA_Skill_Verina_HealingSprout"), TEXT("Demo.Skill.Verina.HealingSprout"), EHSRSkillCategory::Heal, EHSRTargetType::SingleAlly, 1.00f, -1, 0.0f, 1.0f },
		{ TEXT("/Game/Data/VerticalSlice/Skills/DA_Skill_Verina_BreathingGarden.DA_Skill_Verina_BreathingGarden"), TEXT("Demo.Skill.Verina.BreathingGarden"), EHSRSkillCategory::Heal, EHSRTargetType::SingleAlly, 1.80f, 0, 100.0f, 1.0f },
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDemoMapsAndTravelTest,
	"HSR.Demo.ContentCatalog.MapsAndTravel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDemoMapsAndTravelTest::RunTest(const FString&)
{
	using namespace HSR::Demo::ContentCatalogTests;
	struct FMapSpec { const TCHAR* Path; const TCHAR* Id; const TCHAR* Region; const TCHAR* Arrival; const TCHAR* WorldPackage; };
	const FMapSpec Maps[] =
	{
		{ TEXT("/Game/Data/VerticalSlice/Maps/DA_Map_ObservationCar.DA_Map_ObservationCar"), TEXT("Demo.Map.ObservationCar"), TEXT("Demo.Region.DomainRoute"), TEXT("Demo.Arrival.ObservationCar.DomainAnchor"), TEXT("/Game/Maps/VerticalSlice/Map_ObservationCar") },
		{ TEXT("/Game/Data/VerticalSlice/Maps/DA_Map_NewEriduSixthStreetMetro.DA_Map_NewEriduSixthStreetMetro"), TEXT("Demo.Map.NewEriduSixthStreetMetro"), TEXT("Demo.Region.DomainRoute"), TEXT("Demo.Arrival.SixthStreet.DomainAnchor"), TEXT("/Game/Maps/VerticalSlice/Map_NewEriduSixthStreetMetro") },
		{ TEXT("/Game/Data/VerticalSlice/Maps/DA_Map_HertaSupportSection.DA_Map_HertaSupportSection"), TEXT("Demo.Map.HertaSupportSection"), TEXT("Demo.Region.Battle"), TEXT("Demo.Arrival.HertaSupportSection.BattleStart"), TEXT("/Game/Maps/VerticalSlice/Map_HertaSupportSection") },
	};
	for (const FMapSpec& Spec : Maps)
	{
		if (const UHSRMapDefinition* Map = RequireAsset<UHSRMapDefinition>(*this, Spec.Path))
		{
			TestEqual(TEXT("map id"), Map->MapId, FName(Spec.Id));
			TestEqual(TEXT("map region"), Map->RegionId, FName(Spec.Region));
			TestEqual(TEXT("default arrival"), Map->DefaultArrivalId, FName(Spec.Arrival));
			TestEqual(TEXT("world package"), Map->World.ToSoftObjectPath().GetLongPackageName(), FString(Spec.WorldPackage));
			TestFalse(TEXT("map display name is authored"), Map->DisplayName.IsEmpty());
		}
	}

	struct FTeleportSpec { const TCHAR* Path; const TCHAR* Id; const TCHAR* Source; const TCHAR* Destination; const TCHAR* Arrival; };
	const FTeleportSpec Teleports[] =
	{
		{ TEXT("/Game/Data/VerticalSlice/Teleports/DA_Teleport_ObservationCarToSixthStreet.DA_Teleport_ObservationCarToSixthStreet"), TEXT("Demo.Teleport.ObservationCarToSixthStreet"), TEXT("Demo.Map.ObservationCar"), TEXT("Demo.Map.NewEriduSixthStreetMetro"), TEXT("Demo.Arrival.SixthStreet.DomainAnchor") },
		{ TEXT("/Game/Data/VerticalSlice/Teleports/DA_Teleport_SixthStreetToObservationCar.DA_Teleport_SixthStreetToObservationCar"), TEXT("Demo.Teleport.SixthStreetToObservationCar"), TEXT("Demo.Map.NewEriduSixthStreetMetro"), TEXT("Demo.Map.ObservationCar"), TEXT("Demo.Arrival.ObservationCar.DomainAnchor") },
	};
	for (const FTeleportSpec& Spec : Teleports)
	{
		if (const UHSRTeleportDefinition* Teleport = RequireAsset<UHSRTeleportDefinition>(*this, Spec.Path))
		{
			TestEqual(TEXT("teleport id"), Teleport->TeleportId, FName(Spec.Id));
			TestEqual(TEXT("teleport source"), Teleport->SourceMapId, FName(Spec.Source));
			TestEqual(TEXT("teleport destination"), Teleport->DestinationMapId, FName(Spec.Destination));
			TestEqual(TEXT("teleport arrival"), Teleport->DestinationArrivalId, FName(Spec.Arrival));
			TestTrue(TEXT("bidirectional Demo travel begins unlocked"), Teleport->bInitiallyUnlocked);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDemoCharactersAndSkillsTest,
	"HSR.Demo.ContentCatalog.CharactersAndSkills",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDemoCharactersAndSkillsTest::RunTest(const FString&)
{
	using namespace HSR::Demo::ContentCatalogTests;
	struct FCharacterSpec { const TCHAR* Path; const TCHAR* Id; float Health; float Attack; float Defense; float Speed; int32 SkillOffset; };
	const FCharacterSpec Characters[] =
	{
		{ TEXT("/Game/Data/VerticalSlice/Characters/DA_Character_EvernightMoon.DA_Character_EvernightMoon"), TEXT("Demo.Character.EvernightMoon"), 125, 22, 10, 104, 0 },
		{ TEXT("/Game/Data/VerticalSlice/Characters/DA_Character_Huohua.DA_Character_Huohua"), TEXT("Demo.Character.Huohua"), 105, 15, 10, 116, 3 },
		{ TEXT("/Game/Data/VerticalSlice/Characters/DA_Character_Remiel.DA_Character_Remiel"), TEXT("Demo.Character.Remiel"), 115, 17, 12, 110, 6 },
		{ TEXT("/Game/Data/VerticalSlice/Characters/DA_Character_Verina.DA_Character_Verina"), TEXT("Demo.Character.Verina"), 110, 14, 11, 112, 9 },
	};
	for (const FCharacterSpec& Spec : Characters)
	{
		if (const UHSRCharacterDefinition* Character = RequireAsset<UHSRCharacterDefinition>(*this, Spec.Path))
		{
			TestEqual(TEXT("character id"), Character->CharacterId, FName(Spec.Id));
			TestFalse(TEXT("character display name is authored"), Character->DisplayName.IsEmpty());
			TestEqual(TEXT("base health"), Character->BaseMaxHealth, Spec.Health);
			TestEqual(TEXT("base attack"), Character->BaseAttack, Spec.Attack);
			TestEqual(TEXT("base defense"), Character->BaseDefense, Spec.Defense);
			TestEqual(TEXT("base speed"), Character->BaseSpeed, Spec.Speed);
			TestEqual(TEXT("three skill references"), Character->SkillDefinitions.Num(), 3);
			for (int32 Index = 0; Index < 3 && Character->SkillDefinitions.IsValidIndex(Index); ++Index)
			{
				TestEqual(TEXT("skill reference path"), Character->SkillDefinitions[Index].ToSoftObjectPath().ToString(), FString(SkillSpecs[Spec.SkillOffset + Index].ObjectPath));
			}
		}
	}

	TSet<FName> SkillIds;
	for (const FSkillSpec& Spec : SkillSpecs)
	{
		if (const UHSRSkillDefinition* Skill = RequireAsset<UHSRSkillDefinition>(*this, Spec.ObjectPath))
		{
			TestEqual(TEXT("skill id"), Skill->SkillId, FName(Spec.SkillId));
			TestTrue(TEXT("skill id is unique"), !SkillIds.Contains(Skill->SkillId));
			SkillIds.Add(Skill->SkillId);
			TestFalse(TEXT("skill display name is authored"), Skill->DisplayName.IsEmpty());
			TestEqual(TEXT("skill category"), Skill->Category, Spec.Category);
			TestEqual(TEXT("target type"), Skill->TargetType, Spec.TargetType);
			TestTrue(TEXT("ability multiplier"), FMath::IsNearlyEqual(Skill->AbilityMultiplier, Spec.Multiplier));
			TestEqual(TEXT("skill point delta"), Skill->SkillPointDelta, Spec.SkillPointDelta);
			TestTrue(TEXT("toughness damage"), FMath::IsNearlyEqual(Skill->ToughnessDamage, Spec.ToughnessDamage));
			TestTrue(TEXT("category-required references are valid"), Skill->IsValidForCategory());
			if (Spec.EnergyCost > 0.0f)
			{
				float AuthoredCost = 0.0f;
				TestTrue(TEXT("energy display cost is known"), Skill->TryGetDisplayEnergyCost(AuthoredCost));
				TestTrue(TEXT("energy display cost"), FMath::IsNearlyEqual(AuthoredCost, Spec.EnergyCost));
			}
		}
	}
	TestEqual(TEXT("twelve unique formal skills"), SkillIds.Num(), 12);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDemoRelicsTest,
	"HSR.Demo.ContentCatalog.Relics",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDemoRelicsTest::RunTest(const FString&)
{
	using namespace HSR::Demo::ContentCatalogTests;
	const UHSRRelicSetDefinition* MagicalGirl = RequireAsset<UHSRRelicSetDefinition>(*this, TEXT("/Game/Data/VerticalSlice/Relics/Sets/DA_RelicSet_ShiningMagicalGirl.DA_RelicSet_ShiningMagicalGirl"));
	const UHSRRelicSetDefinition* LiveRoom = RequireAsset<UHSRRelicSetDefinition>(*this, TEXT("/Game/Data/VerticalSlice/Relics/Sets/DA_RelicSet_HeavenLiveRoom.DA_RelicSet_HeavenLiveRoom"));
	if (MagicalGirl)
	{
		TestEqual(TEXT("MagicalGirl set id"), MagicalGirl->SetId, FName(TEXT("Demo.RelicSet.ShiningMagicalGirl")));
		TestEqual(TEXT("MagicalGirl threshold"), MagicalGirl->Threshold, 4);
		TestNotNull(TEXT("MagicalGirl effect"), MagicalGirl->SetGameplayEffectClass.Get());
	}
	if (LiveRoom)
	{
		TestEqual(TEXT("LiveRoom set id"), LiveRoom->SetId, FName(TEXT("Demo.RelicSet.HeavenLiveRoom")));
		TestEqual(TEXT("LiveRoom threshold"), LiveRoom->Threshold, 2);
		TestNotNull(TEXT("LiveRoom effect"), LiveRoom->SetGameplayEffectClass.Get());
	}

	struct FPieceSpec { const TCHAR* Suffix; const TCHAR* Id; const TCHAR* SetId; EHSRRelicSlot Slot; };
	const FPieceSpec Pieces[] =
	{
		{ TEXT("ShiningMagicalGirl_Head"), TEXT("Demo.Relic.ShiningMagicalGirl.Head"), TEXT("Demo.RelicSet.ShiningMagicalGirl"), EHSRRelicSlot::Head },
		{ TEXT("ShiningMagicalGirl_Hands"), TEXT("Demo.Relic.ShiningMagicalGirl.Hands"), TEXT("Demo.RelicSet.ShiningMagicalGirl"), EHSRRelicSlot::Hands },
		{ TEXT("ShiningMagicalGirl_Body"), TEXT("Demo.Relic.ShiningMagicalGirl.Body"), TEXT("Demo.RelicSet.ShiningMagicalGirl"), EHSRRelicSlot::Body },
		{ TEXT("ShiningMagicalGirl_Feet"), TEXT("Demo.Relic.ShiningMagicalGirl.Feet"), TEXT("Demo.RelicSet.ShiningMagicalGirl"), EHSRRelicSlot::Feet },
		{ TEXT("HeavenLiveRoom_PlanarSphere"), TEXT("Demo.Relic.HeavenLiveRoom.PlanarSphere"), TEXT("Demo.RelicSet.HeavenLiveRoom"), EHSRRelicSlot::PlanarSphere },
		{ TEXT("HeavenLiveRoom_LinkRope"), TEXT("Demo.Relic.HeavenLiveRoom.LinkRope"), TEXT("Demo.RelicSet.HeavenLiveRoom"), EHSRRelicSlot::LinkRope },
	};
	TSet<EHSRRelicSlot> Slots;
	for (const FPieceSpec& Spec : Pieces)
	{
		const FString Path = FString::Printf(TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_%s.DA_Relic_%s"), Spec.Suffix, Spec.Suffix);
		if (const UHSRRelicDefinition* Piece = RequireAsset<UHSRRelicDefinition>(*this, *Path))
		{
			TestEqual(TEXT("piece id"), Piece->DefinitionId, FName(Spec.Id));
			TestEqual(TEXT("piece set"), Piece->SetId, FName(Spec.SetId));
			TestEqual(TEXT("piece slot"), Piece->Slot, Spec.Slot);
			TestTrue(TEXT("slot is unique across the two sets"), !Slots.Contains(Piece->Slot));
			Slots.Add(Piece->Slot);
			TestTrue(TEXT("piece has a main stat"), !Piece->DefaultModifiers.IsEmpty());
		}
	}
	TestEqual(TEXT("all six relic slots are covered"), Slots.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRDemoFlowCatalogTest,
	"HSR.Demo.ContentCatalog.Flow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRDemoFlowCatalogTest::RunTest(const FString&)
{
	using namespace HSR::Demo::ContentCatalogTests;
	const UHSRQuestDefinition* Quest = RequireAsset<UHSRQuestDefinition>(*this, TEXT("/Game/Data/VerticalSlice/Quests/DA_Quest_DomainEcho.DA_Quest_DomainEcho"));
	if (Quest)
	{
		TestEqual(TEXT("quest id"), Quest->QuestId, FName(TEXT("Demo.Quest.DomainEcho")));
		TestEqual(TEXT("five convergence objectives"), Quest->Objectives.Num(), 5);
		TestEqual(TEXT("quest reward id"), Quest->RewardDefinitionId, FName(TEXT("Demo.Reward.DomainEcho")));
		TSet<FName> ObjectiveIds;
		TSet<FName> EventIds;
		for (const FHSRQuestObjectiveDefinition& Objective : Quest->Objectives)
		{
			TestTrue(TEXT("objective id is unique"), !ObjectiveIds.Contains(Objective.ObjectiveId));
			TestTrue(TEXT("event id is unique"), !EventIds.Contains(Objective.EventId));
			TestEqual(TEXT("objective count is one"), Objective.RequiredCount, 1);
			ObjectiveIds.Add(Objective.ObjectiveId);
			EventIds.Add(Objective.EventId);
		}
	}

	struct FEncounterSpec { const TCHAR* Path; const TCHAR* Id; const TCHAR* EnemyId; const TCHAR* RewardId; };
	const FEncounterSpec Encounters[] =
	{
		{ TEXT("/Game/Data/VerticalSlice/Encounters/DA_Encounter_SupportSectionInspector.DA_Encounter_SupportSectionInspector"), TEXT("Demo.Encounter.SupportSectionInspector"), TEXT("Demo.Enemy.SupportSectionInspector"), TEXT("Demo.Reward.SupportSectionInspector") },
		{ TEXT("/Game/Data/VerticalSlice/Encounters/DA_Encounter_Laigushi.DA_Encounter_Laigushi"), TEXT("Demo.Encounter.Laigushi"), TEXT("Demo.Boss.Laigushi"), TEXT("Demo.Reward.Laigushi") },
	};
	for (const FEncounterSpec& Spec : Encounters)
	{
		if (const UHSREncounterDefinition* Encounter = RequireAsset<UHSREncounterDefinition>(*this, Spec.Path))
		{
			TestEqual(TEXT("encounter id"), Encounter->EncounterId, FName(Spec.Id));
			TestEqual(TEXT("encounter enemy id"), Encounter->EnemyDefinitionId, FName(Spec.EnemyId));
			TestEqual(TEXT("battle map"), Encounter->BattleMap.ToSoftObjectPath().GetLongPackageName(), FString(TEXT("/Game/Maps/Map_Battle")));
			TestNotNull(TEXT("victory reward definition"), Encounter->VictoryRewardDefinition.Get());
			if (Encounter->VictoryRewardDefinition)
			{
				TestEqual(TEXT("victory reward id"), Encounter->VictoryRewardDefinition->RewardDefinitionId, FName(Spec.RewardId));
			}
		}
	}

	struct FRewardSpec { const TCHAR* Path; const TCHAR* Id; int32 ItemCount; };
	const FRewardSpec Rewards[] =
	{
		{ TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_WangXiaYiTong.DA_Reward_WangXiaYiTong"), TEXT("Demo.Reward.WangXiaYiTong"), 1 },
		{ TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_SupportSectionInspector.DA_Reward_SupportSectionInspector"), TEXT("Demo.Reward.SupportSectionInspector"), 1 },
		{ TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_Laigushi.DA_Reward_Laigushi"), TEXT("Demo.Reward.Laigushi"), 4 },
		{ TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_DomainEcho.DA_Reward_DomainEcho"), TEXT("Demo.Reward.DomainEcho"), 1 },
	};
	for (const FRewardSpec& Spec : Rewards)
	{
		if (const UHSRRewardDefinition* Reward = RequireAsset<UHSRRewardDefinition>(*this, Spec.Path))
		{
			TestEqual(TEXT("reward id"), Reward->RewardDefinitionId, FName(Spec.Id));
			TestEqual(TEXT("fixed reward item count"), Reward->FixedItems.Num(), Spec.ItemCount);
		}
	}
	return true;
}

#endif
