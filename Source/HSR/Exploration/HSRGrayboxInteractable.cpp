#include "HSRGrayboxInteractable.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "../Interaction/HSRInteractableInterface.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Character/HSRExplorationCharacter.h"

// 构造函数：创建交互碰撞球（纯查询、产生重叠事件）。
AHSRGrayboxInteractable::AHSRGrayboxInteractable()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	SetRootComponent(CollisionComponent);

	// 只做查询与重叠，不参与物理阻挡。
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComponent->SetGenerateOverlapEvents(true);

	bAvailable = true;
}

// 设置可用性（用于开关交互）。
void AHSRGrayboxInteractable::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
}

// 交互可用性：可用且未被标记为待销毁。
bool AHSRGrayboxInteractable::IsInteractionAvailable_Implementation() const
{
	return bAvailable && !IsPendingKillPending();
}

// 交互提示文本。
FText AHSRGrayboxInteractable::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("HSRGrayboxInteractable", "Prompt", "Graybox Interactable");
}

// 执行交互：校验交互者仍有效、仍在重叠范围内；
// 若配置了遭遇定义则提交遭遇请求，否则仅记录日志。
FHSRInteractionResult AHSRGrayboxInteractable::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
	AActor* Interactor = Context.InteractorActor.Get();

	// 交互者已失效则拒绝。
	if (!Interactor)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED TargetInvalid (interactor expired)"), *GetName());
		return FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Interactor is no longer valid.")));
	}

	// 用真实重叠校验交互者仍在范围内。
	if (!CollisionComponent->IsOverlappingActor(Interactor))
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED OutOfRange (interactor=%s not overlapping)"),
			*GetName(), *Interactor->GetName());
		return FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::OutOfRange,
			FText::FromString(TEXT("Interactor is out of range.")));
	}

	// 若配置了遭遇定义，把它提交给过渡子系统（触发战斗）。
	if (EncounterDefinition)
	{
		UGameInstance* GI = GetGameInstance();
		if (GI)
		{
			UHSRBattleTransitionSubsystem* Subsystem = GI->GetSubsystem<UHSRBattleTransitionSubsystem>();
			if (Subsystem)
			{
				FHSREncounterResult EncResult = Subsystem->RequestEncounterForInteractor(
					EncounterDefinition, InteractInitiative, Interactor);
				if (EncResult.ResultType == EHSREncounterResultType::Success)
				{
					UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s submitted EncounterRequest %s"),
						*GetName(), *EncResult.RequestId.ToString());
					return FHSRInteractionResult::MakeSuccess();
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s EncounterRequest FAILED type=%d msg=%s"),
						*GetName(), static_cast<int32>(EncResult.ResultType), *EncResult.Message.ToString());
					return FHSRInteractionResult::MakeFailure(
						EHSRInteractionFailureReason::ExecutionFailed,
						FText::Format(NSLOCTEXT("Graybox", "EncounterFailed", "Encounter failed: {0}"), EncResult.Message));
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED ExecutionFailed (no subsystem)"), *GetName());
				return FHSRInteractionResult::MakeFailure(
					EHSRInteractionFailureReason::ExecutionFailed,
					FText::FromString(TEXT("Cannot access BattleTransition subsystem.")));
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s FAILED ExecutionFailed (no GameInstance)"), *GetName());
			return FHSRInteractionResult::MakeFailure(
				EHSRInteractionFailureReason::ExecutionFailed,
				FText::FromString(TEXT("Cannot access GameInstance.")));
		}
	}

	// 没有配置遭遇定义：回落为“记录日志并成功”。
	UE_LOG(LogTemp, Log, TEXT("AHSRGrayboxInteractable::ExecuteInteraction - %s interacted by %s at location (X=%.0f Y=%.0f Z=%.0f)"),
		*GetName(), *Interactor->GetName(),
		Context.InteractionLocation.X, Context.InteractionLocation.Y, Context.InteractionLocation.Z);
	return FHSRInteractionResult::MakeSuccess();
}

// 进入重叠：把本物体登记为探索角色的交互候选。
void AHSRGrayboxInteractable::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!OtherActor)
	{
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(OtherActor);
	if (!ExplorationChar)
	{
		return;
	}

	UHSRInteractionComponent* InteractionComp = ExplorationChar->FindComponentByClass<UHSRInteractionComponent>();
	if (InteractionComp)
	{
		InteractionComp->RegisterCandidate(this);
	}
}

// 离开重叠：从探索角色的交互候选注销。
void AHSRGrayboxInteractable::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (!OtherActor)
	{
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(OtherActor);
	if (!ExplorationChar)
	{
		return;
	}

	UHSRInteractionComponent* InteractionComp = ExplorationChar->FindComponentByClass<UHSRInteractionComponent>();
	if (InteractionComp)
	{
		InteractionComp->UnregisterCandidate(this);
	}
}
