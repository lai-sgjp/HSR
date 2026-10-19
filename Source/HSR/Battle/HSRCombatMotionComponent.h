#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HSRCombatMotionComponent.generated.h"

class UAnimSequenceBase;
class UStaticMeshComponent;
class UPointLightComponent;

/** Cosmetic motion only. The coordinator owns damage, admission and bounded action timing. */
UCLASS()
class HSR_API UHSRCombatMotionComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UHSRCombatMotionComponent();
	void Attack(AActor* Target, UAnimSequenceBase* Animation, bool bLunge, bool bUltimate = false, bool bHealing = false);
	void React(UAnimSequenceBase* Animation, bool bDefeated);
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
private:
	void Play(UAnimSequenceBase* Animation, bool bHoldLastPose);
	FVector Origin = FVector::ZeroVector;
	FVector Offset = FVector::ZeroVector;
	float Elapsed = 0.f;
	float Duration = 1.3f;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> UltimateRing;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> UltimateLight;
};
