#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "../Reward/HSRRewardTypes.h"
#include "../Challenge/HSRChallengeProgressionTypes.h"
#include "HSRSceneContent.generated.h"
class UHSRQuestDefinition;
class UHSRItemDefinition;
class UHSRDropTableDefinition;
class UHSRRewardDefinition;
class UHSREquipmentDefinition;
class UHSRRelicDefinition;

/** Installs authored definitions and projects existing completion ledgers into quest events. */
UCLASS()
class HSR_API AHSRSceneContent : public AActor
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere,Category="Content") TObjectPtr<UHSRQuestDefinition> QuestDefinition;
    UPROPERTY(EditAnywhere,Category="Content") TArray<TObjectPtr<UHSRItemDefinition>> Items;
    UPROPERTY(EditAnywhere,Category="Content") TArray<TObjectPtr<UHSREquipmentDefinition>> EquipmentDefinitions;
    UPROPERTY(EditAnywhere,Category="Content") TArray<TObjectPtr<UHSRRelicDefinition>> RelicDefinitions;
    UPROPERTY(EditAnywhere,Category="Content") TObjectPtr<UHSRDropTableDefinition> DropTable;
    UPROPERTY(EditAnywhere,Category="Content") TArray<TObjectPtr<UHSRRewardDefinition>> Rewards;
    UPROPERTY(EditAnywhere,Category="Content") TMap<FName,FName> RewardEvents;
    UPROPERTY(EditAnywhere,Category="Content") TMap<FName,FName> EncounterEvents;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Sync();
    void RewardChanged(const FHSRRewardReceipt&);
    void ProgressChanged(const FHSRChallengeProgressionSnapshot&);
    FDelegateHandle RewardHandle, ProgressHandle;
    bool bSyncing=false;
};
