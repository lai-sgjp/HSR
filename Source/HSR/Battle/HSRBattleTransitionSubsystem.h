#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HSREncounterTypes.h"
#include "HSRBattleTransitionSubsystem.generated.h"

class UHSREncounterDefinition;

UCLASS()
class HSR_API UHSRBattleTransitionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	FHSREncounterResult RequestEncounter(UHSREncounterDefinition* Definition, EHSREncounterInitiative Initiative);

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

	UFUNCTION(BlueprintCallable, Category = "Return")
	FHSRExplorationReturnResult ConsumeReturnContext();

	UFUNCTION(BlueprintPure, Category = "Return")
	bool HasReturnPending() const { return bReturnPending; }

	UFUNCTION(BlueprintPure, Category = "Return")
	const FHSRExplorationReturnContext& GetReturnContext() const { return PendingReturnContext; }


private:
	EHSREncounterState CurrentState;
	FHSREncounterRequest PendingRequest;
	FHSRExplorationReturnContext PendingReturnContext;
	bool bReturnPending;
};
