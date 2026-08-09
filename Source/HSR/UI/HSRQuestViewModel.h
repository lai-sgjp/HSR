#pragma once

#include "CoreMinimal.h"
#include "../Quest/HSRQuestTypes.h"
#include "HSRQuestViewModel.generated.h"

class UHSRQuestSubsystem;

UENUM(BlueprintType)
enum class EHSRQuestFrontendStatus : uint8
{
	Ready,
	Empty,
	Unavailable
};

USTRUCT(BlueprintType)
struct HSR_API FHSRQuestObjectiveViewData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") FName ObjectiveId;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") int32 CurrentCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") int32 RequiredCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRQuestViewData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") FName QuestId;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") EHSRQuestState State = EHSRQuestState::NotStarted;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") TArray<FHSRQuestObjectiveViewData> Objectives;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") bool bRewardClaimed = false;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") bool bDefinitionAvailable = false;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") int64 Revision = 0;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRQuestFrontendSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") EHSRQuestFrontendStatus Status = EHSRQuestFrontendStatus::Unavailable;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") TArray<FHSRQuestViewData> Quests;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Quest") int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRQuestFrontendChanged, const FHSRQuestFrontendSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRQuestFrontendBlueprintChanged, const FHSRQuestFrontendSnapshot&, Snapshot);

UCLASS(BlueprintType)
class HSR_API UHSRQuestViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Initialize(UHSRQuestSubsystem* InQuest);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "HSR|Quest")
	bool GetSnapshot(FHSRQuestFrontendSnapshot& OutSnapshot) const;

	FHSRQuestFrontendChanged& OnChanged() { return Changed; }
	UPROPERTY(BlueprintAssignable, Category = "HSR|Quest")
	FHSRQuestFrontendBlueprintChanged OnSnapshotChanged;

private:
	void HandleQuestChanged(const FHSRQuestRuntimeState&);
	void HandleQuestRestored(int64);
	void Rebuild();

	TWeakObjectPtr<UHSRQuestSubsystem> Quest;
	FDelegateHandle QuestChangedHandle;
	FDelegateHandle QuestRestoredHandle;
	FHSRQuestFrontendSnapshot Snapshot;
	FHSRQuestFrontendChanged Changed;
};
