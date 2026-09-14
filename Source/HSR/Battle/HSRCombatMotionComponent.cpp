#include "HSRCombatMotionComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UHSRCombatMotionComponent::UHSRCombatMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UHSRCombatMotionComponent::Play(UAnimSequenceBase* Animation, bool bHoldLastPose)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	UAnimInstance* Instance = Character ? Character->GetMesh()->GetAnimInstance() : nullptr;
	if (!Instance || !Animation) return;
	const float Rate = FMath::Max(1.f, Animation->GetPlayLength() / 1.2f);
	if (UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Animation, TEXT("DefaultSlot"), .12f, .15f, Rate))
	{
		// Montage instances copy this flag during Initialize, before playback.
		Montage->bEnableAutoBlendOut = !bHoldLastPose;
		Instance->Montage_Play(Montage, Rate);
	}
}

void UHSRCombatMotionComponent::Attack(AActor* Target, UAnimSequenceBase* Animation, bool bLunge)
{
	if (!GetOwner() || !Target) return;
	if (IsComponentTickEnabled()) GetOwner()->SetActorLocation(Origin);
	Play(Animation, false);
	Origin = GetOwner()->GetActorLocation();
	Offset = bLunge ? (Target->GetActorLocation() - Origin).GetSafeNormal2D() * 90.f : FVector(0, 0, 12);
	Elapsed = 0.f;
	Duration = 1.3f;
	SetComponentTickEnabled(true);
}

void UHSRCombatMotionComponent::React(UAnimSequenceBase* Animation, bool bDefeated)
{
	Play(Animation, bDefeated);
	if (!GetOwner() || Cast<ACharacter>(GetOwner())) return;
	if (IsComponentTickEnabled()) GetOwner()->SetActorLocation(Origin);
	Origin = GetOwner()->GetActorLocation();
	Offset = FVector(0, 0, bDefeated ? -35.f : 20.f);
	Elapsed = 0.f;
	Duration = .5f;
	SetComponentTickEnabled(true);
}

void UHSRCombatMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!GetOwner()) { SetComponentTickEnabled(false); return; }
	Elapsed += DeltaTime;
	const float T = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	GetOwner()->SetActorLocation(Origin + Offset * FMath::Sin(T * PI));
	if (T >= 1.f) { GetOwner()->SetActorLocation(Origin); SetComponentTickEnabled(false); }
}
