#include "HSREnemyCharacter.h"
#include "HSREnemyAIController.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"

AHSREnemyCharacter::AHSREnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	EncounterCollision = CreateDefaultSubobject<USphereComponent>(TEXT("EncounterCollision"));
	EncounterCollision->SetupAttachment(GetRootComponent());
	EncounterCollision->SetSphereRadius(200.0f);
	EncounterCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EncounterCollision->SetGenerateOverlapEvents(true);

	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
}

void AHSREnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	CaptureSpawnOrigin();
	if (EnemyDefinition && EncounterCollision)
	{
		EncounterCollision->SetSphereRadius(FMath::Max(0.0f, EnemyDefinition->EncounterRadius));
	}
	UE_LOG(LogTemp, Log, TEXT("AHSREnemyCharacter::BeginPlay - %s SpawnOrigin=%s"), *GetName(), *SpawnOrigin.ToString());
}

void AHSREnemyCharacter::CaptureSpawnOrigin()
{
	SpawnOrigin = GetActorLocation();
	bSpawnOriginCaptured = true;
}

#if WITH_DEV_AUTOMATION_TESTS
void AHSREnemyCharacter::CaptureSpawnOriginForAutomation()
{
	CaptureSpawnOrigin();
}
#endif

void AHSREnemyCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!OtherActor)
		return;

	// Only trigger Encounter from ExplorationCharacter overlap
	if (Cast<AHSRExplorationCharacter>(OtherActor))
	{
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyCharacter::NotifyActorBeginOverlap - %s overlapped by %s"), *GetName(), *OtherActor->GetName());
		TryRequestEncounter();
	}
}

void AHSREnemyCharacter::TryRequestEncounter()
{
	AHSREnemyAIController* AICtrl = Cast<AHSREnemyAIController>(GetController());
	if (!AICtrl)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSREnemyCharacter::TryRequestEncounter - %s FAILED (AIController is null)"), *GetName());
		return;
	}

	AICtrl->TryRequestEncounterFromCharacter();
}
