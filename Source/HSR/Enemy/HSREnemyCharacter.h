#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "HSREnemyCharacter.generated.h"

class UHSREnemyDefinition;
class USphereComponent;

UCLASS()
class HSR_API AHSREnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AHSREnemyCharacter();

	UFUNCTION(BlueprintCallable, Category = "Encounter")
	void TryRequestEncounter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy")
	TObjectPtr<UHSREnemyDefinition> EnemyDefinition;

	UFUNCTION(BlueprintPure, Category = "Enemy")
	FVector GetSpawnOrigin() const { return SpawnOrigin; }

protected:
	virtual void BeginPlay() override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy|Collision")
	TObjectPtr<USphereComponent> EncounterCollision;

	UPROPERTY()
	FVector SpawnOrigin;
};
