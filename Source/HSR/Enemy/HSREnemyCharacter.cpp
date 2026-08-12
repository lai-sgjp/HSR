#include "HSREnemyCharacter.h"
#include "HSREnemyAIController.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"

// 构造函数：创建“遭遇触发”碰撞球体（玩家踏入即触发战斗请求）。
AHSREnemyCharacter::AHSREnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 遭遇碰撞球：只做查询、生成重叠事件，不参与物理阻挡。
	EncounterCollision = CreateDefaultSubobject<USphereComponent>(TEXT("EncounterCollision"));
	EncounterCollision->SetupAttachment(GetRootComponent());
	EncounterCollision->SetSphereRadius(200.0f);
	EncounterCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EncounterCollision->SetGenerateOverlapEvents(true);

	// 敌人自身胶囊体不产生重叠（避免与遭遇球重复触发）。
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
}

void AHSREnemyCharacter::BeginPlay()
{
	Super::BeginPlay();
	// 记录生成点（用于 AI 巡逻回位）。
	CaptureSpawnOrigin();
	// 用定义里的遭遇半径覆盖默认碰撞球半径。
	ApplyDefinitionEncounterConfig();
	UE_LOG(LogTemp, Log, TEXT("AHSREnemyCharacter::BeginPlay - %s SpawnOrigin=%s"), *GetName(), *SpawnOrigin.ToString());
}

// 用敌人定义中的遭遇半径配置碰撞球。
void AHSREnemyCharacter::ApplyDefinitionEncounterConfig()
{
	if (EnemyDefinition && EncounterCollision)
	{
		EncounterCollision->SetSphereRadius(FMath::Max(0.0f, EnemyDefinition->EncounterRadius));
	}
}

// 记录生成位置。
void AHSREnemyCharacter::CaptureSpawnOrigin()
{
	SpawnOrigin = GetActorLocation();
	bSpawnOriginCaptured = true;
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试用：强制重新记录生成点。
void AHSREnemyCharacter::CaptureSpawnOriginForAutomation()
{
	CaptureSpawnOrigin();
}
#endif

// 重叠回调：只有探索角色踏入才尝试发起遭遇。
void AHSREnemyCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!OtherActor)
	{
		return;
	}

	// 只有探索角色重叠才触发遭遇。
	if (Cast<AHSRExplorationCharacter>(OtherActor))
	{
		UE_LOG(LogTemp, Log, TEXT("AHSREnemyCharacter::NotifyActorBeginOverlap - %s overlapped by %s"), *GetName(), *OtherActor->GetName());
		TryRequestEncounter();
	}
}

// 尝试请求遭遇：需要有效的 AI 控制器（由它向过渡子系统发起）。
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
