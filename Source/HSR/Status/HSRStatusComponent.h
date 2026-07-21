#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSRStatusTypes.h"
#include "HSRStatusComponent.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UHSRStatusDefinition;
class UHSRTurnManager;
class UHSRBattleCoordinator;
struct FHSRTurnLifecycleEvent;

DECLARE_MULTICAST_DELEGATE(FHSRStatusChangedDelegate);

UCLASS(ClassGroup=(HSR), meta=(BlueprintSpawnableComponent))
class HSR_API UHSRStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHSRStatusComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	bool InitializeStatusRuntime(FName InParticipantId, UAbilitySystemComponent* InAbilitySystem);
	bool BindTurnManager(UHSRTurnManager* InTurnManager);
	void BindBattleCoordinator(UHSRBattleCoordinator* InCoordinator);
	void UnbindTurnManager();
	EHSRStatusOperationResult AddOrRefreshStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, FGuid OperationId = FGuid());
	EHSRStatusOperationResult ReplaceStatus(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId);
	EHSRStatusOperationResult ClearStatus();
	EHSRStatusOperationResult DispelOneStatus();
	int32 HandleSourceInvalid(FName SourceParticipantId);
	FHSRStatusRuntimeSnapshot GetSnapshot(FName StatusId = NAME_None) const;
	TArray<FHSRStatusPublicSnapshot> GetPublicSnapshots() const;
	const FHSRStatusPublicOperationEvent& GetLastPublicOperation() const { return LastPublicOperation; }
	FHSRStatusChangedDelegate& OnStatusChanged() { return StatusChanged; }

#if WITH_EDITOR
	void SetForceApplyFailureForDevelopmentTest(bool bInForce) { bForceApplyFailure = bInForce; }
	void SetForceOldRemoveFailureForDevelopmentTest(bool bInForce) { bForceOldRemoveFailure = bInForce; }
	void SetForceDispelRemoveFailureForDevelopmentTest(bool bInForce) { bForceDispelRemoveFailure = bInForce; }
	void ConsumeLifecycleEventForDevelopmentTest(const FHSRTurnLifecycleEvent& Event) { HandleTurnEnded(Event); }
	void InvalidateAbilitySystemForDevelopmentTest() { AbilitySystem.Reset(); }
	FActiveGameplayEffectHandle GetPrimaryHandleForDevelopmentTest() const { return ActiveStatus.IsSet() ? ActiveStatus->ActiveGameplayEffectHandle : FActiveGameplayEffectHandle(); }
	FActiveGameplayEffectHandle GetHandleForDevelopmentTest(FName StatusId) const { const FHSRStatusInstance* Instance = AdditionalStatuses.Find(StatusId); return Instance ? Instance->ActiveGameplayEffectHandle : GetPrimaryHandleForDevelopmentTest(); }
	FActiveGameplayEffectHandle GetSecondaryHandleForDevelopmentTest() const { return SecondaryOwnedHandle; }
#endif

private:
	void HandleTurnEnded(const FHSRTurnLifecycleEvent& Event);
	EHSRStatusOperationResult ClearStatusById(FName StatusId, EHSRStatusPublicOperation Operation = EHSRStatusPublicOperation::Clear);
	EHSRStatusOperationResult ValidateAdd(const UHSRStatusDefinition* Definition, FName SourceParticipantId, FName TargetParticipantId, TSubclassOf<UGameplayEffect>& OutEffectClass) const;
	void NotifyStatusChanged() { StatusChanged.Broadcast(); }
	void RecordPublicOperation(EHSRStatusPublicOperation Operation, EHSRStatusOperationResult Result, FName StatusId, FName TargetId);

	FName ParticipantId;
	TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
	TWeakObjectPtr<UHSRTurnManager> BoundTurnManager;
	TWeakObjectPtr<UHSRBattleCoordinator> BoundCoordinator;
	FDelegateHandle TurnEndedHandle;
	TOptional<FHSRStatusInstance> ActiveStatus;
	TMap<FName, FHSRStatusInstance> AdditionalStatuses;
	UPROPERTY()
	TMap<FName, TObjectPtr<UHSRStatusDefinition>> RuntimeDefinitions;
	TSet<FString> ProcessedStatusOperations;
	TSet<FString> ProcessedInvalidSources;
	TSet<FGuid> ProcessedAddOperations;
	FActiveGameplayEffectHandle SecondaryOwnedHandle;
	EHSRStatusOperationResult LastResult = EHSRStatusOperationResult::Success;
	int32 ApplyCount = 0;
	int32 RemoveCount = 0;
	int32 LastRemovedRemainingTurns = 0;
	uint64 LastRemovedTurnSequence = 0;
	bool bInitialized = false;
	FHSRStatusChangedDelegate StatusChanged;
	FHSRStatusPublicOperationEvent LastPublicOperation;
	uint64 PublicOperationSequence = 0;
#if WITH_EDITOR
	bool bForceApplyFailure = false;
	bool bForceOldRemoveFailure = false;
	bool bForceDispelRemoveFailure = false;
#endif
};
