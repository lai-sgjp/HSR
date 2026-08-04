#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSREncounterTypes.h"
#include "HSRBattleTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "Containers/Ticker.h"
#include "HSRBattleTransitionSubsystem.generated.h"

class UHSREncounterDefinition;
class APawn;

UENUM()
enum class EHSRTravelKind : uint8
{
	None,
	Encounter,
	Return
};

#if WITH_DEV_AUTOMATION_TESTS
struct FHSRTransitionAutomationSnapshot
{
	EHSREncounterState State = EHSREncounterState::Empty;
	FHSREncounterRequest PendingRequest;
	EHSRTravelKind TravelKind = EHSRTravelKind::None;
	FGuid TravelRequestId;
	bool bResolvedMembership = false;
	int32 AdmissionMutationCount = 0;
	int32 TravelInitiationCount = 0;
};
#endif

UCLASS()
class HSR_API UHSRBattleTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static EHSREncounterResultType BuildEncounterRequest(const FHSRPreBattleAdmissionInput& Input,
		FHSREncounterRequest& OutRequest);
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	FHSREncounterResult RequestEncounter(UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative);

	/** Submits a previously validated, pure encounter request for travel. */
	UFUNCTION(BlueprintCallable, Category = "Encounter")
	FHSREncounterResult SubmitEncounterRequestFromUI(const FHSREncounterRequest& Request);
	FHSREncounterResult RequestEncounterForInteractor(UHSREncounterDefinition* Definition,
		EHSREncounterInitiative Initiative, AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	FHSREncounterResult ConsumePendingEncounter();

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void ClearPending();

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool HasPending() const;

	UFUNCTION(BlueprintPure, Category = "Encounter")
	EHSREncounterState GetCurrentState() const { return CurrentState; }

	UFUNCTION(BlueprintCallable, Category = "Return")
	FHSRExplorationReturnResult RequestTestReturn(const FHSREncounterRequest& FromConsumedRequest);
	/** Starts return travel from a pure battle result and resolves its encounter on success. */
	FHSRExplorationReturnResult RequestBattleReturn(const FHSRBattleResult& BattleResult);
	/** Only a victory permanently resolves an encounter; defeat and interruption remain retryable. */
	static bool ShouldResolveEncounter(EHSRBattleOutcome Outcome);
	/** Read-only preflight for the return transaction; it never writes pending return state or travels. */
	FHSRExplorationReturnResult ValidateBattleReturn(const FHSRBattleResult& BattleResult) const;

	UFUNCTION(BlueprintCallable, Category = "Return")
	FHSRExplorationReturnResult ConsumeReturnContext();
	FHSRExplorationReturnResult CommitReturnContext(APawn* PlayerPawn);

	UFUNCTION(BlueprintCallable, Category = "Return")
	void ClearReturn();

	UFUNCTION(BlueprintPure, Category = "Return")
	bool HasReturnPending() const { return bReturnPending; }

	void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);

#if WITH_DEV_AUTOMATION_TESTS
	FHSRTransitionAutomationSnapshot GetAutomationSnapshot(FName EncounterId) const;
	void SeedPendingEncounterForAutomation(const FHSREncounterRequest& InRequest);
	void SeedResolvedEncounterForAutomation(FName EncounterId);
	void ResetEncounterAutomationFixture();
	void SetTravelSuppressedForAutomation(bool bSuppress) { bSuppressTravelForAutomation = bSuppress; }
#endif
	static bool DoesTravelFailureMatch(const FString& FailureWorldPackage, const FString& SourcePackage, const FString& TargetPackage);

private:
	FHSREncounterResult SubmitEncounterRequest(const FHSREncounterRequest& Request, UWorld* World);
	FHSREncounterResult RequestEncounterInternal(UHSREncounterDefinition* Definition,
		EHSREncounterInitiative Initiative, AActor* Interactor);
	void StartTravelTimeout();
	void ClearTravelTimeout();
	bool HandleTravelTimeout(float DeltaTime);
	EHSREncounterState CurrentState;
	FHSREncounterRequest PendingRequest;
	FHSRExplorationReturnContext PendingReturnContext;
	bool bReturnPending;
	bool bReturnConsumed;
	EHSRTravelKind TravelKind;
	FGuid TravelRequestId;
	FName TravelTargetMap;
	FName TravelSourceMap;
	FName TravelCompletedEncounterId;
	FTSTicker::FDelegateHandle TravelTimeoutHandle;
	TSet<FName> ResolvedEncounterIds;
#if WITH_DEV_AUTOMATION_TESTS
	int32 AdmissionMutationCountForAutomation = 0;
	int32 TravelInitiationCountForAutomation = 0;
	bool bSuppressTravelForAutomation = false;
#endif
};
