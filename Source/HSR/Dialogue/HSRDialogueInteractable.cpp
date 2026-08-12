#include "HSRDialogueInteractable.h"

#include "HSRDialogueSubsystem.h"
#include "../Quest/HSRQuestSubsystem.h"
#include "../Data/Definitions/HSRQuestDefinition.h"
#include "../Data/Definitions/HSRDialogueDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AHSRDialogueInteractable::AHSRDialogueInteractable()
{
	PrimaryActorTick.bCanEverTick = false;

	// Give the graybox a discoverable marker and a forgiving interaction radius.
	// The base CollisionComponent starts at radius 32 and is hidden in game, which
	// makes the Dialogue interactable impossible to find or reach during PIE.
	// （中文说明）给灰盒一个可见标记和更宽容的交互半径：
	// 基类 CollisionComponent 默认半径只有 32 且在游戏中被隐藏，
	// 这会导致 PIE 里玩家很难发现或够到这个对话交互物，所以这里放大并显示出来。
	CollisionComponent->SetSphereRadius(260.f);
	CollisionComponent->SetHiddenInGame(false);

	// 视觉标记：用一个小球体悬浮在头顶，让玩家一眼看出“这是可交互物”。
	VisualMarker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMarker"));
	VisualMarker->SetupAttachment(RootComponent);
	VisualMarker->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	VisualMarker->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));
	// 标记只做显示，不能挡路、不能参与碰撞。
	VisualMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 引擎内置球体网格，运行时一定能找到，无需额外资产依赖。
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MarkerMesh(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (MarkerMesh.Succeeded())
	{
		VisualMarker->SetStaticMesh(MarkerMesh.Object);
	}
}

void AHSRDialogueInteractable::BeginPlay()
{
	Super::BeginPlay();
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	// 关卡中放置的这个 Actor 若直接引用了任务/对话定义资产，
	// 则在运行时先把它登记进对应子系统，保证后续交互时定义可用。
	if (QuestDefinition)
	{
		if (UHSRQuestSubsystem* Quest = GI->GetSubsystem<UHSRQuestSubsystem>())
		{
			Quest->RegisterQuestDefinition(*QuestDefinition);
		}
	}
	if (DialogueDefinition)
	{
		if (UHSRDialogueSubsystem* Dialogue = GI->GetSubsystem<UHSRDialogueSubsystem>())
		{
			Dialogue->RegisterDialogueDefinition(*DialogueDefinition);
		}
	}
}

// 交互提示文案：对话交互物的固定提示词。
FText AHSRDialogueInteractable::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("HSRDialogueInteractable", "Prompt", "Talk");
}

// 执行交互：校验配置后取得对话起点节点，交给 UI/上层开启对话。
FHSRInteractionResult AHSRDialogueInteractable::ExecuteInteraction_Implementation(const FHSRInteractionContext& Context)
{
	// 必须有交互者且配置了对话 ID，否则无法开始任何对话。
	if (!Context.InteractorActor.IsValid() || DialogueId.IsNone())
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed, FText::FromString(TEXT("Dialogue is not configured.")));
	}

	FHSRDialogueNodeDefinition StartNode;
	if (!GetStartDialogueNode(StartNode))
	{
		return FHSRInteractionResult::MakeFailure(EHSRInteractionFailureReason::ExecutionFailed, FText::FromString(TEXT("Dialogue definition is unavailable.")));
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRDialogueInteractable::ExecuteInteraction - DialogueId=%s StartNode=%s"), *DialogueId.ToString(), *StartNode.NodeId.ToString());
	return FHSRInteractionResult::MakeDialogueSuccess(DialogueId, StartNode.NodeId);
}

// 取得该交互物对话的起点节点。对话子系统不可用或未配置 ID 时返回 false。
bool AHSRDialogueInteractable::GetStartDialogueNode(FHSRDialogueNodeDefinition& OutNode) const
{
	const UGameInstance* GI = GetGameInstance();
	const UHSRDialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UHSRDialogueSubsystem>() : nullptr;
	return Dialogue && !DialogueId.IsNone() && Dialogue->GetStartNode(DialogueId, OutNode);
}

// 选择对话中的一个选项：转发给对话子系统，并把选择结果回填给调用方。
EHSRQuestOperationResult AHSRDialogueInteractable::SelectDialogueChoice(FName NodeId, FName ChoiceId, FHSRDialogueChoiceResult& OutResult)
{
	UGameInstance* GI = GetGameInstance();
	UHSRDialogueSubsystem* Dialogue = GI ? GI->GetSubsystem<UHSRDialogueSubsystem>() : nullptr;
	if (!Dialogue || DialogueId.IsNone())
	{
		OutResult = FHSRDialogueChoiceResult();
		return EHSRQuestOperationResult::UnknownDialogueDefinition;
	}
	return Dialogue->SelectChoice(DialogueId, NodeId, ChoiceId, OutResult);
}
