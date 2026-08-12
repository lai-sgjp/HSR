#include "HSRHUD.h"
#include "HSRUserWidget.h"
#include "HSRAttributeViewModel.h"
#include "HSRInteractionViewModel.h"
#include "HSRInventoryRewardViewModel.h"
#include "HSRInventoryRewardWidget.h"
#include "Inventory/HSRInventoryModuleWidget.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Interaction/HSRInteractionComponent.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "../Character/HSRCharacterBase.h"
#include "HSRScreenWidget.h"
#include "HSRUIManagerSubsystem.h"
#include "../Player/HSRPlayerController.h"
#include "Engine/LocalPlayer.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"

// HUD 进入世界后立即展示探索界面。
// 探索 HUD 是常驻的（作为屏幕栈的根界面），在游戏开始时一次性构建。
void AHSRHUD::BeginPlay()
{
	Super::BeginPlay();
	ShowExplorationHUD();
}

// HUD 结束播放：需要区分“正常退出”与“因旅行（探索→战斗 / 战斗→探索）而拆除”。
// 若是旅行途中被拆除，必须先经由 UIManager 把“探索宿主”的注册摘掉，
// 避免旅行回来时出现重复宿主或残留状态。
void AHSRHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameInstance* GI = GetGameInstance();
	const UHSRMapSubsystem* Maps = GI ? GI->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	const UHSRBattleTransitionSubsystem* BattleTravel = GI ? GI->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	// 是否存在“已授权的旅行”待处理：地图子系统或战斗转场子系统任一有待完成的旅行都算。
	const bool bAuthorizedTravelPending = (Maps && Maps->HasPendingTravel())
		|| (BattleTravel && (BattleTravel->HasPending() || BattleTravel->HasReturnPending()));
	const bool bCaptureTravel = ShouldCaptureTravelRestore(EndPlayReason, bAuthorizedTravelPending);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 HUD EndPlay Reason=%d CaptureTravel=%s"),
		static_cast<int32>(EndPlayReason), bCaptureTravel ? TEXT("true") : TEXT("false"));
	// 只有确实处于旅行拆除时，才需要摘掉 UI 宿主注册（记录结果避免后续重复摘除）。
	if (bCaptureTravel)
	{
		if (AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(GetOwningPlayerController()))
		{
			if (ULocalPlayer* LP = HSRPC->GetLocalPlayer())
			{
				if (UHSRUIManagerSubsystem* UIManager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
				{
					const EHSRUIScreenResult Result = UIManager->TeardownExplorationHostForTravel(this, HSRPC);
					// 若宿主本就不存在（返回 InvalidHost），说明无需注册也无需恢复，置位避免后续误判。
					bUIHostAlreadyUnregistered = Result != EHSRUIScreenResult::InvalidHost;
				}
			}
		}
	}
	RemoveExplorationHUD();
	Super::EndPlay(EndPlayReason);
}

// 把“前端模块 -> 其 Widget 类”的映射构建出来，供 UIManager 在按需打开
// 某个模块界面（队伍/地图/挑战/任务/存档）时查找对应 Widget 类来实例化。
TMap<EHSRFrontendModule, TSubclassOf<UUserWidget>> AHSRHUD::BuildFrontendModuleWidgetClasses() const
{
	TMap<EHSRFrontendModule, TSubclassOf<UUserWidget>> Classes;
	Classes.Add(EHSRFrontendModule::Party, PartyWidgetClass);
	Classes.Add(EHSRFrontendModule::Map, MapWidgetClass);
	Classes.Add(EHSRFrontendModule::Challenge, ChallengeWidgetClass);
	Classes.Add(EHSRFrontendModule::Quest, QuestWidgetClass);
	Classes.Add(EHSRFrontendModule::Save, SaveWidgetClass);
	return Classes;
}

// 构建并显示探索 HUD 的完整生命周期：
// 1) 用配置好的 Widget 类创建探索界面实例并加入视口；
// 2) 把它连同各子界面类一起注册给 UIManager（作为 UI 宿主）；
// 3) 可选地创建“奖励摘要”ViewModel 并挂到奖励摘要 Widget 上；
// 4) 建立对当前 Pawn 交互组件的观察。
void AHSRHUD::ShowExplorationHUD()
{
	// 已有实例：直接返回，防止重复创建导致界面叠加。
	if (ExplorationWidgetInstance)
	{
		return;
	}

	// Widget 类未配置：这是配置错误，打警告并放弃显示。
	if (!ExplorationWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::ShowExplorationHUD - ExplorationWidgetClass is not set"));
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::ShowExplorationHUD - No OwningPlayerController"));
		return;
	}

	// 用玩家控制器作为外层创建 Widget，确保它属于正确的本地玩家。
	ExplorationWidgetInstance = CreateWidget<UHSRUserWidget>(PC, ExplorationWidgetClass);
	if (!ExplorationWidgetInstance)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::ShowExplorationHUD - CreateWidget failed"));
		return;
	}

	ExplorationWidgetInstance->AddToViewport();
	// 把本 HUD 注册为 UIManager 的“探索宿主”，并提供各子界面的 Widget 类，
	// 这样 UIManager 才能统一管理屏幕栈与输入模式。
	if (AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(PC))
	{
		if (ULocalPlayer* LP = HSRPC->GetLocalPlayer())
		{
			if (UHSRUIManagerSubsystem* UIManager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
			{
				UIManager->RegisterExplorationHost(this, HSRPC, ExplorationWidgetInstance, FrontendShellClass,
					FrontendModuleRootClass, CharacterDetailWidgetClass, InventoryWidgetClass,
					InventoryModuleWidgetClass,
					DialogueOverlayWidgetClass,
					BuildFrontendModuleWidgetClasses());
			}
		}
	}
	// 奖励摘要：从游戏实例子系统读取背包与奖励数据，创建对应的 ViewModel 并初始化，
	// 再创建摘要 Widget 并注入该 VM。VM 负责订阅子系统变化并转成快照。
	UGameInstance* GameInstance = GetGameInstance();
	UHSRInventorySubsystem* Inventory = GameInstance ? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	UHSRRewardSubsystem* Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	if (Inventory && Reward && RewardSummaryWidgetClass)
	{
		RewardSummaryViewModel = NewObject<UHSRInventoryRewardViewModel>(this);
		RewardSummaryViewModel->Initialize(Inventory, Reward);
		FHSRInventoryRewardSnapshot InitialSnapshot;
		// 首次快照能取到才创建 Widget；取不到说明暂无数据，也清理掉 VM。
		if (RewardSummaryViewModel->GetSnapshot(InitialSnapshot))
		{
			RewardSummaryWidgetInstance = CreateWidget<UHSRRewardSummaryWidget>(PC, RewardSummaryWidgetClass);
			if (RewardSummaryWidgetInstance)
			{
				RewardSummaryWidgetInstance->SetViewModel(RewardSummaryViewModel);
				RewardSummaryWidgetInstance->AddToViewport();
			}
			else
			{
				// 创建失败时收尾：关闭 VM 并置空，避免悬空引用。
				RewardSummaryViewModel->Shutdown();
				RewardSummaryViewModel = nullptr;
			}
		}
		else
		{
			RewardSummaryViewModel->Shutdown();
			RewardSummaryViewModel = nullptr;
		}
	}

	// 建立交互观察：把当前 Pawn 的交互组件绑定到交互 ViewModel 上。
	RefreshInteractionObserver();
}


// 让交互 ViewModel 观察“当前 Pawn 的交互组件”，并把该 VM 注入探索 Widget。
// 当角色/交互目标变化时，VM 会广播提示，Widget 订阅后更新交互提示显示。
// 之所以每次刷新都重新比对组件，是因为 Pawn 可能变化（死亡、传送、换人）。
void AHSRHUD::RefreshInteractionObserver()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		ClearInteractionObserverInstance();
		return;
	}

	APawn* CurrentPawn = PC->GetPawn();
	if (!CurrentPawn)
	{
		ClearInteractionObserverInstance();
		return;
	}

	// 当前 Pawn 上必须存在交互组件，否则没有可观察的数据源。
	UHSRInteractionComponent* InteractComp = CurrentPawn->FindComponentByClass<UHSRInteractionComponent>();
	if (!InteractComp)
	{
		ClearInteractionObserverInstance();
		return;
	}

	// 惰性创建交互 VM（首次需要时才创建）。
	if (!InteractionViewModel)
	{
		InteractionViewModel = NewObject<UHSRInteractionViewModel>(this);
		UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RefreshInteractionObserver - Created new VM[%d]"), InteractionViewModel->GetInstanceId());
	}

	// 观察目标变化：先解绑旧组件的完成事件，再切换到新组件。
	if (ObservedInteractionComponent.Get() != InteractComp)
	{
		if (ObservedInteractionComponent.IsValid())
		{
			ObservedInteractionComponent->OnInteractionCompleted.RemoveDynamic(
				this, &ThisClass::HandleInteractionCompleted);
		}
		ObservedInteractionComponent = InteractComp;
	}
	// 用 AddUniqueDynamic 保证即使重复调用也只有一个订阅。
	InteractComp->OnInteractionCompleted.AddUniqueDynamic(this, &ThisClass::HandleInteractionCompleted);

	// 先让 VM 观察组件（建立绑定并推送一次当前候选）。
	InteractionViewModel->Observe(InteractComp);

	// 再把 VM 接入 Widget——SetInteractionViewModel 内部会对新连接做 ForceCurrentSnapshot，
	// 因此这里不需要再手动刷新一次。
	if (ExplorationWidgetInstance)
	{
		ExplorationWidgetInstance->SetInteractionViewModel(InteractionViewModel);
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RefreshInteractionObserver - VM[%d] Component=%s Pawn=%s"),
		InteractionViewModel->GetInstanceId(), *InteractComp->GetName(), *CurrentPawn->GetName());
}

// 清理交互观察相关的全部状态：解绑组件事件、Teardown 并置空 VM、把 Widget 上的 VM 摘掉。
void AHSRHUD::ClearInteractionObserverInstance()
{
	if (ObservedInteractionComponent.IsValid())
	{
		ObservedInteractionComponent->OnInteractionCompleted.RemoveDynamic(
			this, &ThisClass::HandleInteractionCompleted);
		ObservedInteractionComponent.Reset();
	}
	if (InteractionViewModel)
	{
		InteractionViewModel->Teardown();
		InteractionViewModel = nullptr;
	}
	if (ExplorationWidgetInstance)
	{
		ExplorationWidgetInstance->SetInteractionViewModel(nullptr);
	}
	UE_LOG(LogTemp, Log, TEXT("AHSRHUD::ClearInteractionObserverInstance - Cleared"));
}

// 交互完成回调：当一次交互带有“对话载荷”时，把对话请求转交给 UIManager 打开对话覆盖层。
void AHSRHUD::HandleInteractionCompleted(const FHSRInteractionResult& Result)
{
	// 没有对话载荷的交互（如纯拾取）不需要打开对话界面。
	if (!Result.HasDialoguePayload())
	{
		return;
	}

	// 从本地玩家身上取 UIManager 子系统；取不到说明环境异常，直接放弃。
	AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(GetOwningPlayerController());
	ULocalPlayer* LP = HSRPC ? HSRPC->GetLocalPlayer() : nullptr;
	UHSRUIManagerSubsystem* UIManager = LP ? LP->GetSubsystem<UHSRUIManagerSubsystem>() : nullptr;
	if (!UIManager)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::HandleInteractionCompleted - Dialogue payload has no UIManager"));
		return;
	}

	const EHSRUIScreenResult OpenResult = UIManager->OpenDialogueOverlay(
		Result.DialogueId, Result.DialogueNodeId);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue RequestOpen Result=%d Dialogue=%s Node=%s HasOverlay=%s"),
		static_cast<int32>(OpenResult), *Result.DialogueId.ToString(), *Result.DialogueNodeId.ToString(),
		UIManager->HasOpenDialogueOverlay() ? TEXT("true") : TEXT("false"));
}

// 仅供 P2 阶段测试使用：请求重建探索 HUD。
// 在 Test/Shipping 构建中直接拒绝（该接口只用于开发期验证）。
void AHSRHUD::RequestRebuildExplorationHUDForPhase2Test()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Rejected in Test/Shipping"));
	return;
#else
	UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Rebuilding ExplorationHUD"));
	RemoveExplorationHUD();

	// 用 WeakThis + 下一帧定时器重建：确保当前帧的拆除完全完成后再重新构建，
	// 避免在同帧内 Create/Remove 冲突。WeakThis 防止 HUD 在定时器触发前被销毁。
	TWeakObjectPtr<AHSRHUD> WeakThis(this);
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - GetWorld() is null, cannot schedule rebuild"));
		return;
	}
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		// 定时器回调中先取回 HUD 指针；已被销毁则静默放弃。
		AHSRHUD* HUD = WeakThis.Get();
		if (!HUD)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - HUD destroyed before next tick"));
			return;
		}
		HUD->ShowExplorationHUD();
		// 校验 PlayerController -> Pawn -> HUD 链路是否完整（此时不应再有第二次广播）。
		APlayerController* PC = HUD->GetOwningPlayerController();
		if (!PC)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - PC is null after Show"));
			return;
		}
		APawn* Pawn = PC->GetPawn();
		if (!Pawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Pawn is null after Show"));
			return;
		}
		AHUD* HUDCheck = PC->GetHUD();
		UE_LOG(LogTemp, Log, TEXT("AHSRHUD::RequestRebuildExplorationHUDForPhase2Test - Chain valid: PC=%s, Pawn=%s, HUD=%s"),
			*PC->GetName(), *Pawn->GetName(), HUDCheck ? *HUDCheck->GetName() : TEXT("null"));
		// 快照路径：新 Widget Construct（蓝图）会调用 BroadcastCurrentValues 作为唯一入口。
	}));
#endif
}

// 拆除探索 HUD：先摘掉 UIManager 宿主注册（若尚未摘除），清理交互观察与奖励摘要 UI，
// 最后移除探索 Widget 本身。
void AHSRHUD::RemoveExplorationHUD()
{
	// 只有在旅行流程中没有预先摘除注册时，才在这里正式反注册；
	// 反注册失败说明 UIManager 已强制清理了过期宿主，这里仅记错误日志。
	if (!bUIHostAlreadyUnregistered)
	{
		if (AHSRPlayerController* HSRPC = Cast<AHSRPlayerController>(GetOwningPlayerController()))
		{
			if (ULocalPlayer* LP = HSRPC->GetLocalPlayer())
			{
				if (UHSRUIManagerSubsystem* UIManager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
				{
					const EHSRUIScreenResult Result = UIManager->UnregisterExplorationHost(this, HSRPC);
					if (Result != EHSRUIScreenResult::Success)
					{
						UE_LOG(LogTemp, Error, TEXT("HSRUI P17 HUD host teardown Result=%d; manager forced stale-host cleanup"),
							static_cast<int32>(Result));
					}
				}
			}
		}
	}
	// 复位标志，保证下次拆除会重新尝试反注册。
	bUIHostAlreadyUnregistered = false;
	ClearInteractionObserverInstance();
	// 清理奖励摘要 UI：先移除 Widget，再关闭其 ViewModel（两者都必须置空，防止残留引用）。
	if (RewardSummaryWidgetInstance)
	{
		RewardSummaryWidgetInstance->RemoveFromParent();
		RewardSummaryWidgetInstance = nullptr;
	}
	if (RewardSummaryViewModel)
	{
		RewardSummaryViewModel->Shutdown();
		RewardSummaryViewModel = nullptr;
	}

	// 探索 Widget 可能本就不存在（例如创建失败），此时没有需要移除的对象。
	if (!ExplorationWidgetInstance)
	{
		return;
	}

	ExplorationWidgetInstance->RemoveFromParent();
	ExplorationWidgetInstance = nullptr;
}
