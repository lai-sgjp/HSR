#include "HSRSceneContent.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Challenge/HSRChallengeProgressionSubsystem.h"
#include "Engine/GameInstance.h"

void AHSRSceneContent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetGameInstance()) return;
    if (auto* Equipment = GetGameInstance()->GetSubsystem<UHSREquipmentSubsystem>())
    {
        const auto Register = [Equipment](const auto& Definitions)
        {
            for (const auto& Definition : Definitions)
            {
                if (!Definition || Equipment->HasDefinition(Definition->DefinitionId)) continue;
                if (Equipment->RegisterDefinition(*Definition) != EHSREquipmentOperationResult::Success)
                    UE_LOG(LogTemp, Error, TEXT("Scene content cannot register equipment definition %s"), *Definition->GetPathName());
            }
        };
        Register(EquipmentDefinitions);
        Register(RelicDefinitions);
    }
    if (!QuestDefinition) return;
    auto* Inventory=GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>();
    auto* Reward=GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>();
    auto* Quest=GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>();
    auto* Progress=GetGameInstance()->GetSubsystem<UHSRChallengeProgressionSubsystem>();
    if (!Inventory || !Reward || !Quest || !Progress) return;
    for (const auto& Item : Items) if (Item) Inventory->RegisterDefinition(*Item);
    if (DropTable) Reward->RegisterDropTable(*DropTable);
    for (const auto& Definition : Rewards) if (Definition) Reward->RegisterRewardDefinition(*Definition);
    Quest->RegisterQuestDefinition(*QuestDefinition);
    FHSRQuestRuntimeState State; Quest->StartQuest(QuestDefinition->QuestId,State);
    RewardHandle=Reward->OnRewardCommitted().AddUObject(this,&ThisClass::RewardChanged);
    ProgressHandle=Progress->OnProgressionChanged().AddUObject(this,&ThisClass::ProgressChanged);
    Sync();
}
void AHSRSceneContent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetGameInstance())
    {
        if (auto* R=GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>()) R->OnRewardCommitted().Remove(RewardHandle);
        if (auto* P=GetGameInstance()->GetSubsystem<UHSRChallengeProgressionSubsystem>()) P->OnProgressionChanged().Remove(ProgressHandle);
    }
    Super::EndPlay(Reason);
}
void AHSRSceneContent::RewardChanged(const FHSRRewardReceipt&) { Sync(); }
void AHSRSceneContent::ProgressChanged(const FHSRChallengeProgressionSnapshot&) { Sync(); }
void AHSRSceneContent::Sync()
{
    if (bSyncing || !GetGameInstance() || !QuestDefinition) return;
    TGuardValue<bool> Guard(bSyncing,true);
    auto* Quest=GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>();
    auto* Reward=GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>();
    auto* Progress=GetGameInstance()->GetSubsystem<UHSRChallengeProgressionSubsystem>();
    if (!Quest || !Reward || !Progress) return;
    TSet<FName> Events;
    FHSRRewardSaveData Receipts; Reward->ExportSaveData(Receipts);
    for (const auto& Receipt : Receipts.Receipts)
        if (const FName* Event=RewardEvents.Find(Receipt.Request.RewardDefinitionId)) Events.Add(*Event);
    for (const auto& Pair : EncounterEvents) if (Progress->IsCompleted(Pair.Key)) Events.Add(Pair.Value);
    FHSRQuestRuntimeState State;
    if (!Quest->GetQuestState(QuestDefinition->QuestId,State)) return;
    for (const auto& Definition : QuestDefinition->Objectives)
    {
        const auto* Current=State.Objectives.FindByPredicate([&](const FHSRQuestRuntimeObjective& O){return O.ObjectiveId==Definition.ObjectiveId;});
        if (Current && !Current->bCompleted && Events.Contains(Definition.EventId))
        {
            FHSRQuestDomainEvent Event; Event.EventId=Definition.EventId; Event.Count=Definition.RequiredCount-Current->CurrentCount;
            TArray<FHSRQuestRuntimeState> Changed; Quest->SubmitEvent(Event,Changed);
        }
    }
}
