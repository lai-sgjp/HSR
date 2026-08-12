#include "HSRRewardChest.h"

#include "../Character/HSRExplorationCharacter.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

// 构造函数：创建交互球 + 可见的宝箱本体（Cube 占位）。
AHSRRewardChest::AHSRRewardChest()
{
	PrimaryActorTick.bCanEverTick = false;
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetSphereRadius(80.0f);

	// 可见的宝箱本体，让奖励在世界上可发现、可交互。
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(CollisionComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(MeshFinder.Object);
	}
	MeshComponent->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.0f));
	MeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -10.0f));
}

void AHSRRewardChest::BeginPlay()
{
	Super::BeginPlay();
	// 校验领取配置：稳定领取 ID 必须有效，且全世界唯一。
	bClaimConfigurationValid = StableClaimId.IsValid();
	if (bClaimConfigurationValid)
	{
		for (TActorIterator<AHSRRewardChest> It(GetWorld()); It; ++It)
		{
			const AHSRRewardChest* Other = *It;
			// 出现重复 StableClaimId 时，路径名更大的那个视为配置无效（去重）。
			if (Other != this && Other->StableClaimId == StableClaimId && GetPathName().Compare(Other->GetPathName()) > 0)
			{
				bClaimConfigurationValid = false;
				UE_LOG(LogTemp, Error, TEXT("P13-003 RewardChest Configuration=INVALID Reason=DuplicateStableClaimId ClaimId=%s Actor=%s Other=%s"), *StableClaimId.ToString(), *GetPathName(), *Other->GetPathName());
				break;
			}
		}
	}
	// 配置有效时把奖励包注册进奖励子系统（物品/掉率表/奖励定义）。
	UGameInstance* GameInstance = GetGameInstance();
	UHSRRewardSubsystem* Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	if (bClaimConfigurationValid && Reward && DropTableDefinition && RewardDefinition)
	{
		const EHSRRewardOperationResult Result = Reward->RegisterBundle(ItemDefinitions, *DropTableDefinition, *RewardDefinition);
		bRewardBundleRegistered = Result == EHSRRewardOperationResult::Success || Result == EHSRRewardOperationResult::NoOp;
		if (!bRewardBundleRegistered)
		{
			UE_LOG(LogTemp, Error, TEXT("P13-004 RewardChest DefinitionRegistration=FAILED Result=%d Actor=%s"), static_cast<int32>(Result), *GetPathName());
		}
	}
}

// 交互可用性：配置有效、尚未被领取、且未待销毁。
bool AHSRRewardChest::IsInteractionAvailable_Implementation() const
{
	return bClaimConfigurationValid && !bClaimed && !IsPendingKillPending();
}

// 交互提示文本。
FText AHSRRewardChest::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("HSRRewardChest", "Prompt", "Open reward chest");
}

// 执行交互：校验交互者/范围/未领取状态后，向奖励子系统提交奖励请求（恰好一次）。
FHSRInteractionResult AHSRRewardChest::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
	AActor* Interactor = Context.InteractorActor.Get();
	if (!Interactor)
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::TargetInvalid);
	}
	if (!CollisionComponent->IsOverlappingActor(Interactor))
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::OutOfRange);
	}
	if (bClaimed)
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::Unavailable);
	}
	UGameInstance* GameInstance = GetGameInstance();
	UHSRRewardSubsystem* Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	// 所有依赖必须就绪才能发奖。
	if (!bClaimConfigurationValid || !bRewardBundleRegistered || !StableClaimId.IsValid()
		|| !Reward || !DropTableDefinition || !RewardDefinition)
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
	}
	// 用稳定领取 ID 提交奖励请求。
	FHSRRewardRequest Request;
	Request.ClaimId = StableClaimId;
	Request.RewardDefinitionId = RewardDefinition->RewardDefinitionId;
	Request.Seed = RewardSeed;
	FHSRRewardReceipt Receipt;
	const EHSRRewardOperationResult Result = Reward->SubmitReward(Request, Receipt);
	if (Result != EHSRRewardOperationResult::Success && Result != EHSRRewardOperationResult::NoOp)
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed);
	}
	// 已领取：从交互候选注销。
	bClaimed = true;
	if (UHSRInteractionComponent* Interaction = Interactor->FindComponentByClass<UHSRInteractionComponent>())
	{
		Interaction->UnregisterCandidate(this);
	}
	UE_LOG(LogTemp, Log, TEXT("P13-003 RewardChest Result=%s ClaimId=%s Revision=%lld"), Result == EHSRRewardOperationResult::Success ? TEXT("SUCCESS") : TEXT("NOOP"), *StableClaimId.ToString(), Receipt.Revision);
	return FHSRInteractionResult::MakeSuccess();
}

// 进入重叠：登记为探索角色的交互候选。
void AHSRRewardChest::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	if (AHSRExplorationCharacter* Character = Cast<AHSRExplorationCharacter>(OtherActor))
	{
		if (UHSRInteractionComponent* Interaction = Character->FindComponentByClass<UHSRInteractionComponent>())
		{
			Interaction->RegisterCandidate(this);
		}
	}
}

// 离开重叠：从探索角色的交互候选注销。
void AHSRRewardChest::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	if (AHSRExplorationCharacter* Character = Cast<AHSRExplorationCharacter>(OtherActor))
	{
		if (UHSRInteractionComponent* Interaction = Character->FindComponentByClass<UHSRInteractionComponent>())
		{
			Interaction->UnregisterCandidate(this);
		}
	}
}
