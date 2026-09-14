#include "HSRWorldMarkers.h"
#include "../Exploration/HSRRewardChest.h"
#include "../Exploration/HSRSceneContent.h"
#include "../Exploration/HSRSceneInteraction.h"
#include "../Enemy/HSREnemyCharacter.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Data/Definitions/HSREncounterDefinition.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "HSRMapSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"

TArray<FHSRWorldMarker> HSRWorldMarkers::Collect(UWorld* World)
{
	TArray<FHSRWorldMarker> Result;
	if (!World || !World->GetGameInstance()) return Result;
	auto* Quest = World->GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>();
	auto* Maps = World->GetGameInstance()->GetSubsystem<UHSRMapSubsystem>();
	FName EventId;
	FText ObjectiveLabel;
	AHSRSceneContent* Content = nullptr;
	for (TActorIterator<AHSRSceneContent> It(World); It; ++It)
	{
		if (!Quest || !It->QuestDefinition) continue;
		FHSRQuestRuntimeState State;
		if (!Quest->GetQuestState(It->QuestDefinition->QuestId, State)) continue;
		for (const auto& Objective : It->QuestDefinition->Objectives)
		{
			const auto* Runtime = State.Objectives.FindByPredicate([&](const auto& O) { return O.ObjectiveId == Objective.ObjectiveId; });
			if (Runtime && !Runtime->bCompleted)
			{
				EventId = Objective.EventId;
				ObjectiveLabel = Quest->GetObjectiveDescription(State.QuestId, Objective.ObjectiveId);
				Content = *It;
				break;
			}
		}
		if (Content) break;
	}
	for (TActorIterator<AHSRRewardChest> It(World); It; ++It)
		if (!It->IsHidden() && It->IsInteractionAvailable_Implementation())
		{
			const bool bQuest = Content && It->GetRewardDefinition() && Content->RewardEvents.FindRef(It->GetRewardDefinition()->RewardDefinitionId) == EventId;
			Result.Add({FName(*It->GetStableClaimId().ToString()), It->GetActorLocation(), bQuest ? ObjectiveLabel : NSLOCTEXT("HSRMarkers", "Chest", "宝箱"), bQuest});
		}
	if (EventId.IsNone()) return Result;
	bool bLocalObjective = Result.ContainsByPredicate([](const auto& M) { return M.bQuest; });
	for (TActorIterator<AHSRSceneInteraction> It(World); It; ++It)
	{
		const bool bEncounter = Content && It->EncounterDefinition && Content->EncounterEvents.FindRef(It->EncounterDefinition->EncounterId) == EventId;
		const bool bDiscovered = Maps && !It->DiscoveryId.IsNone() && Maps->GetSnapshot().ExplorationFlags.Contains(It->DiscoveryId);
		if (!It->IsHidden() && !bDiscovered && (It->QuestEventId == EventId || bEncounter))
		{
			Result.Add({It->GetFName(), It->GetActorLocation(), ObjectiveLabel, true});
			bLocalObjective = true;
		}
	}
	for (TActorIterator<AHSREnemyCharacter> It(World); It; ++It)
		if (!It->IsHidden() && Content && It->EnemyDefinition && It->EnemyDefinition->EncounterDefinition
			&& Content->EncounterEvents.FindRef(It->EnemyDefinition->EncounterDefinition->EncounterId) == EventId)
		{
			Result.Add({It->GetFName(), It->GetActorLocation(), ObjectiveLabel, true});
			bLocalObjective = true;
		}
	if (!bLocalObjective)
		for (TActorIterator<AHSRSceneInteraction> It(World); It; ++It)
			if (!It->TeleportId.IsNone() && Maps && Maps->IsTeleportUnlocked(It->TeleportId))
				Result.Add({It->TeleportId, It->GetActorLocation(), FText::Format(NSLOCTEXT("HSRMarkers", "Travel", "前往目标区域 · {0}"), ObjectiveLabel), true});
	return Result;
}
