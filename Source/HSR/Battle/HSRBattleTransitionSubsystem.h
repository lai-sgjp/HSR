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
	FHSREncounterResult RequestEncounter(UHSREncounterDefinition* Definition);

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	FHSREncounterResult ConsumePendingEncounter();

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void ClearPending();

	UFUNCTION(BlueprintPure, Category = "Encounter")
	bool HasPending() const;

	UFUNCTION(BlueprintPure, Category = "Encounter")
	EHSREncounterState GetCurrentState() const { return CurrentState; }

private:
	EHSREncounterState CurrentState;
	FHSREncounterRequest PendingRequest;
};
