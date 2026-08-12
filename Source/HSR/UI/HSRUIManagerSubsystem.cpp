#include "HSRUIManagerSubsystem.h"
#include "HSRInputModeCoordinator.h"
#include "HSRScreenStack.h"
#include "HSRScreenWidget.h"
#include "HSRCharacterDetailWidget.h"
#include "HSRInventoryRewardWidget.h"
#include "HSRInventoryRewardViewModel.h"
#include "Inventory/HSRInventoryModuleWidget.h"
#include "Dialogue/HSRDialogueOverlayWidget.h"
#include "Dialogue/HSRDialoguePresentationViewModel.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Dialogue/HSRDialogueSubsystem.h"
#include "../Equipment/HSREquipmentTypes.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Party/HSRPartyTypes.h"
#include "../Reward/HSRRewardSubsystem.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "HSRUserWidget.h"
#include "HSRHUD.h"
#include "Frontend/HSRFrontendRouter.h"
#include "Frontend/HSRFrontendShellWidget.h"
#include "Frontend/HSRFrontendModuleRootWidget.h"
#include "../Player/HSRPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// 本文件匿名命名空间：模块内私有的 UI 标识常量。
// 这些 FName 是屏幕栈（ScreenStack）与焦点协调器（InputModeCoordinator）之间约定的
// 屏幕 ID / 焦点 Token，贯穿整个前端 UI 生命周期，因此集中定义避免散落各处出现拼写漂移。
namespace
{
	// 探索世界常驻的根屏幕，位于 HUD 层，代表探索状态本身
	const FName ExplorationRootId(TEXT("UI.Screen.ExplorationRoot"));
	// 暂停（Pause）屏幕，Modal 层，是前端各子页（角色/背包/队伍…）的宿主外壳
	const FName PauseScreenId(TEXT("UI.Screen.Pause"));
	// 暂停屏幕打开后默认交给外壳的焦点 Token
	const FName PauseFocusToken(TEXT("UI.Focus.Pause.Primary"));
	// 角色详情屏（旧式独立屏幕路径，P17 后并入前端模块）
	const FName CharacterDetailScreenId(TEXT("UI.Screen.CharacterDetail"));
	const FName CharacterDetailFocusToken(TEXT("UI.Focus.CharacterDetail.Back"));
	// 背包屏（旧式独立屏幕路径，P17 后并入前端模块）
	const FName InventoryScreenId(TEXT("UI.Screen.Inventory"));
	const FName InventoryFocusToken(TEXT("UI.Focus.Inventory.Back"));
}

// 子系统初始化：创建三大 UI 核心对象并订阅地图到达事件。
// - ScreenStack：屏幕栈，管理逻辑屏幕的推入/弹出；
// - InputModeCoordinator：把"当前屏幕意图"解析为输入策略并应用到 PlayerController；
// - FrontendRouter：前端模块路由，决定当前停留的前端模块（暂停集线/角色/背包…）。
// 另外这里订阅 UHSRMapSubsystem 的 OnArrivalCommitted：跨地图旅行（进战斗/返回）后，
// 地图到达提交事件会驱动 UI 的旅行恢复（TravelRestore）逻辑。
void UHSRUIManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 三个核心对象均以本子系统为 Outer，保证生命周期随子系统释放而回收
	ScreenStack = NewObject<UHSRScreenStack>(this);
	InputModeCoordinator = NewObject<UHSRInputModeCoordinator>(this);
	FrontendRouter = NewObject<UHSRFrontendRouter>(this);

	// 请求 Token 从 1 开始递增，保证每个屏幕请求/路由请求都有唯一单调编号
	NextRequestToken = 1;
	NextFrontendRequestToken = 1;

	// 初始化为健康状态：已初始化、无不一致、不可从旅行恢复
	bInitialized = true;
	bInconsistent = false;
	bInconsistencyIsTravelRecoverable = false;

	// 从 LocalPlayer 反查 GameInstance，再取地图子系统：
	// 这样只在地图子系统存在时才建立订阅，避免对不存在子系统的空引用
	if (UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
	{
		if (UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
		{
			// 用 AddUObject 绑定成员函数并保存句柄，Deinitialize 时按句柄解绑
			ArrivalCommittedHandle = Maps->OnArrivalCommitted().AddUObject(this, &ThisClass::HandleArrivalCommitted);
		}
	}
}

// 子系统析构：按依赖顺序反注册并释放所有 UI 资源。
// 顺序很重要：先解绑外部事件，再拆掉可能持有其他对象的 Widget/ViewModel，
// 最后清空核心对象指针，避免任何对象在释放过程中回调一个已失效的子系统。
void UHSRUIManagerSubsystem::Deinitialize()
{
	// 1) 撤销地图到达订阅，防止世界正在卸载时再收到回调
	if (UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
	{
		if (UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
		{
			Maps->OnArrivalCommitted().Remove(ArrivalCommittedHandle);
		}
	}
	// 清空旅行恢复相关的所有暂存状态
	ArrivalCommittedHandle.Reset();
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
	TravelRestoreScreenId = NAME_None;

	// 2) 释放对话浮层（Widget 与 ViewModel 成对释放）
	ReleaseDialogueOverlay();

	// 3) 背包屏：先解绑 ViewModel，再移除 Widget，最后关闭 ViewModel
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr);
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
	}
	if (InventoryViewModelInstance)
	{
		InventoryViewModelInstance->Shutdown();
		InventoryViewModelInstance = nullptr;
	}

	// 4) 角色详情屏：仅移除 Widget（该屏不拥有独立 ViewModel）
	if (CharacterDetailWidgetInstance)
	{
		CharacterDetailWidgetInstance->RemoveFromParent();
		CharacterDetailWidgetInstance = nullptr;
	}

	// 5) 前端外壳与模块根容器依次卸载
	if (FrontendShellInstance)
	{
		FrontendShellInstance->RemoveFromParent();
		FrontendShellInstance = nullptr;
	}
	if (FrontendModuleRootInstance)
	{
		// 模块根容器持有内容 Widget，必须先释放内容再拆容器
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}

	// 6) 如果暂停屏还开着（PauseOwnerToken 有效）且世界仍处于暂停，
	//    必须把世界恢复为运行态，否则玩家会卡在一个无法操作的空暂停
	if (PauseOwnerToken.IsValid())
	{
		if (AHSRPlayerController* PC = RegisteredPlayerController.Get())
		{
			if (UWorld* World = PC->GetWorld(); World && World->IsPaused())
			{
				UGameplayStatics::SetGamePaused(World, false);
			}
		}
	}
	PauseOwnerToken.Invalidate();

	// 7) 清空宿主引用与核心对象，标记未初始化
	ClearHostReferences();
	InputModeCoordinator = nullptr;
	FrontendRouter = nullptr;
	ScreenStack = nullptr;
	bInitialized = false;

	Super::Deinitialize();
}

// 转发屏幕请求给屏幕栈；屏幕栈不存在时按"非法请求"失败
EHSRScreenStackResult UHSRUIManagerSubsystem::SubmitScreenRequest(const FHSRScreenRequest& Request)
{
	return ScreenStack ? ScreenStack->SubmitRequest(Request) : EHSRScreenStackResult::InvalidRequest;
}

// 让输入模式协调器基于当前屏幕栈快照解析出实际输入策略（UI 独占 / 仅游戏等）
FHSRInputModePolicy UHSRUIManagerSubsystem::GetResolvedInputPolicy() const
{
	return InputModeCoordinator ? InputModeCoordinator->ResolvePolicy(ScreenStack) : FHSRInputModePolicy{};
}

// 返回屏幕栈当前逻辑屏幕数量（用于日志与一致性命中判断）
int32 UHSRUIManagerSubsystem::GetLogicalScreenCount() const
{
	return ScreenStack ? ScreenStack->GetSnapshot().Entries.Num() : 0;
}

// 注册探索宿主（Exploration Host）：由探索世界的 HUD 在 BeginPlay 时调用，
// 把 HUD、PlayerController、根 Widget 以及各类前端 Widget 类交给 UI 管理器。
// 这是 UI 管理器与世界"握手"的入口——只有注册成功后，暂停/背包/角色等前端功能才能使用。
// 同名宿主重复注册会刷新 Widget 类（NoOp）；不同宿主则拒绝（InvalidHost）。
EHSRUIScreenResult UHSRUIManagerSubsystem::RegisterExplorationHost(AHSRHUD* HUD,
	AHSRPlayerController* PlayerController, UHSRUserWidget* RootWidget,
	TSubclassOf<UHSRFrontendShellWidget> InFrontendShellClass,
	TSubclassOf<UHSRFrontendModuleRootWidget> InFrontendModuleRootClass,
	TSubclassOf<UHSRScreenWidget> InCharacterDetailWidgetClass,
	TSubclassOf<UHSRInventoryWidget> InInventoryWidgetClass,
	TSubclassOf<UHSRInventoryModuleWidget> InInventoryModuleWidgetClass,
	TSubclassOf<UHSRDialogueOverlayWidget> InDialogueOverlayWidgetClass,
	const TMap<EHSRFrontendModule, TSubclassOf<UUserWidget>>& InFrontendModuleWidgetClasses)
{
	// 子系统尚未初始化，无法安全操作
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	// 宿主参数校验：必须持有本地控制器与根 Widget
	if (!HUD || !PlayerController || !PlayerController->IsLocalPlayerController() || !RootWidget)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	// 背包持有不一致（Widget 与 ViewModel 不成对）说明上一次生命周期未收尾，
	// 视为不可恢复的不一致，直接拒绝注册
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		bInconsistencyIsTravelRecoverable = false;
		return EHSRUIScreenResult::Inconsistent;
	}
	// 完全相同的宿主再次注册：只刷新 Widget 类配置，不重建任何实例（NoOp）
	if (RegisteredHUD.Get() == HUD && RegisteredPlayerController.Get() == PlayerController
		&& RegisteredRootWidget.Get() == RootWidget)
	{
		FrontendShellClass = InFrontendShellClass;
		FrontendModuleRootClass = InFrontendModuleRootClass;
		CharacterDetailWidgetClass = InCharacterDetailWidgetClass;
		InventoryWidgetClass = InInventoryWidgetClass;
		InventoryModuleWidgetClass = InInventoryModuleWidgetClass;
		DialogueOverlayWidgetClass = InDialogueOverlayWidgetClass;
		FrontendModuleWidgetClasses = InFrontendModuleWidgetClasses;
		return EHSRUIScreenResult::NoOp;
	}
	// 若已注册过宿主，或还有任何前端实例存活，说明当前宿主尚未干净收尾，拒绝更换宿主
	if (RegisteredHUD.IsValid() || FrontendShellInstance || FrontendModuleContentInstance
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance || InventoryViewModelInstance
		|| DialogueOverlayWidgetInstance || DialogueViewModelInstance)
	{
		return EHSRUIScreenResult::InvalidHost;
	}

	// 确保屏幕栈底部是探索根屏幕：空栈先推入根，非空栈则要求栈底就是根
	if (ScreenStack->GetSnapshot().Entries.IsEmpty())
	{
		if (ScreenStack->SubmitRequest(MakeRootRequest(AllocateRequestToken())) != EHSRScreenStackResult::Success)
		{
			return EHSRUIScreenResult::StackRejected;
		}
	}
	else if (ScreenStack->GetSnapshot().Entries[0].ScreenId != ExplorationRootId)
	{
		return EHSRUIScreenResult::InvalidHost;
	}

	// 记录宿主引用并分配新的宿主代数（用于旅行恢复时区分新旧宿主）
	RegisteredHUD = HUD;
	RegisteredPlayerController = PlayerController;
	RegisteredRootWidget = RootWidget;
	ActiveHostGeneration = NextHostGeneration++;

	// 保存全部前端 Widget 类，供后续按需创建实例
	FrontendShellClass = InFrontendShellClass;
	FrontendModuleRootClass = InFrontendModuleRootClass;
	CharacterDetailWidgetClass = InCharacterDetailWidgetClass;
	InventoryWidgetClass = InInventoryWidgetClass;
	InventoryModuleWidgetClass = InInventoryModuleWidgetClass;
	DialogueOverlayWidgetClass = InDialogueOverlayWidgetClass;
	FrontendModuleWidgetClasses = InFrontendModuleWidgetClasses;

	// Clear before restoring: a travel-scoped inconsistency would otherwise reject the restore
	// and every later OpenFrontendModule call on this otherwise-healthy host.
	// 先清除可恢复的不一致，再尝试恢复旅行描述符：
	// 若上次旅行中断留下"可恢复的不一致"，新宿主注册正好提供了重新校验的机会
	TryClearRecoverableInconsistency();
	TryRestoreTravelDescriptor();
	return EHSRUIScreenResult::Success;
}

// 反注册探索宿主：仅当 HUD 与控制器完全匹配时才允许收尾
EHSRUIScreenResult UHSRUIManagerSubsystem::UnregisterExplorationHost(AHSRHUD* HUD, AHSRPlayerController* PlayerController)
{
	if (RegisteredHUD.Get() != HUD || RegisteredPlayerController.Get() != PlayerController)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	return TeardownCurrentHost(false);
}

// 为旅行而拆除探索宿主：要求宿主代数有效，随后捕获并拆除旅行宿主
EHSRUIScreenResult UHSRUIManagerSubsystem::TeardownExplorationHostForTravel(AHSRHUD* HUD,
	AHSRPlayerController* PlayerController)
{
	if (RegisteredHUD.Get() != HUD || RegisteredPlayerController.Get() != PlayerController || ActiveHostGeneration == 0)
		return EHSRUIScreenResult::InvalidHost;
	return CaptureAndTeardownTravelHost();
}

// 准备进入跨地图旅行：若当前宿主健康则捕获其 UI 状态并拆除，以便旅行到达后恢复
EHSRUIScreenResult UHSRUIManagerSubsystem::PrepareExplorationTravel()
{
	// 子系统不健康时不处理
	if (!bInitialized || bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	// 尚无宿主（例如旅行发起时 UI 尚未注册），无需拆除
	if (ActiveHostGeneration == 0)
	{
		return EHSRUIScreenResult::Success;
	}
	// 已有未消费的旅行恢复描述符，说明前一次旅行还没落地，拒绝重复进入
	if (bTravelRestorePending)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	return CaptureAndTeardownTravelHost();
}

// 拆除当前探索宿主：按逆序关闭所有前端 UI（对话浮层 → 前端外壳 → 背包 → 角色详情）。
// bForTravel 为 true 表示这是旅行前的清理——此时应尽量恢复到干净状态以便旅行恢复；
// 为 false 表示宿主永久退出。任何一步关闭失败都会强制清理并标记不一致。
EHSRUIScreenResult UHSRUIManagerSubsystem::TeardownCurrentHost(const bool bForTravel)
{
	// bRecovered 追踪是否所有步骤都成功；任何失败都会导致整体不一致
	bool bRecovered = true;
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;

	// 1) 关闭对话浮层：非旅行拆除时恢复输入策略；若仍无法关闭则强制释放
	if (DialogueOverlayWidgetInstance || DialogueViewModelInstance)
	{
		const EHSRUIScreenResult DialogueCloseResult = CloseDialogueOverlayInternal(!bForTravel);
		// "NothingOpen"（本来就没有）也算干净，因此一并计入恢复成功
		bRecovered &= DialogueCloseResult == EHSRUIScreenResult::Success
			|| DialogueCloseResult == EHSRUIScreenResult::NothingOpen;
		if (DialogueOverlayWidgetInstance || DialogueViewModelInstance)
		{
			// 正常关闭失败，强制释放这对 Widget/ViewModel
			ReleaseDialogueOverlay();
		}
	}

	// 2) 关闭前端外壳（暂停集线）；成功后外壳内部会释放模块根
	if (FrontendShellInstance)
	{
		bRecovered &= CloseFrontendToRoot() == EHSRUIScreenResult::Success;
	}

	// 3) 关闭背包屏：先走正常关闭，失败则强制拆 Widget 并 Shutdown ViewModel
	if (InventoryWidgetInstance || InventoryViewModelInstance)
	{
		const EHSRUIScreenResult CloseResult = CloseInventoryScreen();
		bRecovered &= CloseResult == EHSRUIScreenResult::Success;
		if (InventoryWidgetInstance || InventoryViewModelInstance)
		{
			// 正常关闭失败：强制解绑、移除并回收 ViewModel，同时恢复输入策略
			if (InventoryWidgetInstance)
			{
				InventoryWidgetInstance->SetViewModel(nullptr);
				InventoryWidgetInstance->RemoveFromParent();
				InventoryWidgetInstance = nullptr;
			}
			if (InventoryViewModelInstance)
			{
				InventoryViewModelInstance->Shutdown();
				InventoryViewModelInstance = nullptr;
			}
			bRecovered &= ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration);
		}
	}

	// 4) 关闭角色详情屏，失败时同样强制拆除并恢复策略
	if (CharacterDetailWidgetInstance)
	{
		const EHSRUIScreenResult CloseResult = CloseCharacterDetailScreen();
		bRecovered &= CloseResult == EHSRUIScreenResult::Success;
		if (CharacterDetailWidgetInstance)
		{
			CharacterDetailWidgetInstance->RemoveFromParent();
			CharacterDetailWidgetInstance = nullptr;
			bRecovered &= ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration);
		}
	}

	// 5) 外壳若仍在（说明 CloseFrontendToRoot 未完全生效），执行强制外壳拆除
	if (FrontendShellInstance)
	{
		const EHSRUIScreenResult CloseResult = RequestBack();
		bRecovered &= CloseResult == EHSRUIScreenResult::Success;
		if (FrontendShellInstance)
		{
			FrontendShellInstance->RemoveFromParent();
			FrontendShellInstance = nullptr;
			// The shell owns the module root; CloseFrontendToRoot() only reaches its own
			// clear on the success path, so a forced shell teardown must release it here or
			// the retired module root outlives the host and blocks every later recovery.
			// 外壳持有模块根：强制拆除外壳时若模块根仍在，必须一并释放，
			// 否则退役的模块根会悬空存活并阻塞后续所有恢复
			if (FrontendModuleRootInstance)
			{
				ReleaseFrontendModuleContent();
				FrontendModuleRootInstance->RemoveFromParent();
				FrontendModuleRootInstance = nullptr;
			}
			// 同步让前端路由回到根（CloseToRoot），保持路由与实例状态一致
			if (FrontendRouter)
			{
				FHSRFrontendRouteRequest ForcedClose;
				ForcedClose.RequestToken = AllocateFrontendRequestToken();
				ForcedClose.Operation = EHSRFrontendRouteOperation::CloseToRoot;
				FrontendRouter->Submit(ForcedClose);
			}
			// 强制把屏幕栈弹回仅剩探索根（外壳层应被弹出）
			if (ScreenStack && ScreenStack->GetSnapshot().Entries.Num() > 1)
			{
				bRecovered &= ScreenStack->SubmitRequest(MakePopRequest(AllocateRequestToken())) == EHSRScreenStackResult::Success;
			}
			// 恢复输入策略为探索模式，必要时恢复世界暂停状态
			bRecovered &= ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration);
			if (PauseOwnerToken.IsValid() && IsBackendPaused(World))
			{
				bRecovered &= ApplyPauseBackend(World, false);
			}
		}
	}

	// 宿主已不再拥有暂停屏
	PauseOwnerToken.Invalidate();

	// Evaluate containment while the retired host is still observable: ClearHostReferences()
	// zeroes ActiveHostGeneration, and IsAtCleanExplorationRoot() would then read a torn state.
	// 在 ClearHostReferences() 之前评估"是否已回到干净探索根"：
	// 因为清除引用会把 ActiveHostGeneration 清零，之后再判断会读到撕裂的中间状态
	const bool bContainedToRetiredHost = IsAtCleanExplorationRoot();

	// 清空所有宿主引用与前端实例指针
	ClearHostReferences();
#if WITH_DEV_AUTOMATION_TESTS
	AutomationHostIdentity = 0;
	bAutomationHostValid = false;
#endif

	// 任一环节失败：标记不一致。若栈已回到干净根，说明损坏被限制在退役宿主内，可随新宿主恢复
	if (!bRecovered)
	{
		bInconsistent = true;
		// Host references and module instances are cleared above, so if the stack is back at a
		// clean root the damage is contained to the retired host and a fresh one can recover.
		bInconsistencyIsTravelRecoverable = bContainedToRetiredHost;
		UE_LOG(LogTemp, Error,
			TEXT("HSRUI P17 Host teardown required forced cleanup; host references cleared Recoverable=%s Stack=%d"),
			bContainedToRetiredHost ? TEXT("true") : TEXT("false"), GetLogicalScreenCount());
		return EHSRUIScreenResult::Inconsistent;
	}
	return EHSRUIScreenResult::Success;
}

// 打开暂停屏（Pause）。暂停屏是前端模块的宿主外壳：先推入 Pause 屏幕到屏幕栈，
// 创建并挂载外壳 Widget，应用 UI 输入策略与世界暂停，最后让前端路由定位到 PauseHub。
// 每一步失败都走补偿路径（CompensatePop）回滚到打开前的状态。
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenPauseScreen()
{
	// 前置校验：未初始化 / 不一致 / 对话浮层占道 / 旅行中 / 背包持有不一致 全部拒绝
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	// 对话浮层会拦截输入，此时打开前端会与浮层冲突
	if (HasDialogueOverlayBlockingFrontend())
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}
	if (IsTravelPending())
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	// 已有任何前端实例存活（外壳/角色详情/背包），不允许叠加暂停屏
	if (FrontendShellInstance || CharacterDetailWidgetInstance || InventoryWidgetInstance)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}

	// 读取宿主引用并校验世界侧状态
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!IsBackendExploration(PC))
	{
		return EHSRUIScreenResult::NotExploration;
	}

	// 暂停屏要求屏幕栈必须恰好只有探索根（暂停屏是叠加在根之上的 Modal）
	const FHSRScreenStackSnapshot PreflightSnapshot = ScreenStack->GetSnapshot();
	if (PreflightSnapshot.Entries.Num() != 1 || PreflightSnapshot.Entries[0].ScreenId != ExplorationRootId)
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}

	// 世界已被暂停但没有我们颁发的暂停 Token：说明暂停来自外部，避免破坏其状态
	if (IsBackendPaused(World) && !PauseOwnerToken.IsValid())
	{
		return EHSRUIScreenResult::ExternalPause;
	}

	// 外壳 Widget 类必须可用（自动化后端可用自动化标志代替）
	if (!FrontendShellClass
#if WITH_DEV_AUTOMATION_TESTS
		&& !(bUseAutomationBackend && bAutomationHasPauseClass)
#endif
	)
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}

	// 创建暂停外壳候选对象
	UHSRFrontendShellWidget* Candidate = CreatePauseCandidate(PC);
	if (!Candidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	// 让外壳持有所属 UI 管理器，便于回调
	Candidate->SetOwningUIManager(this);

	// 记录打开前的输入策略、屏幕栈与路由快照，用于失败补偿回滚
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRScreenStackSnapshot OldStack = ScreenStack->GetSnapshot();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter ? FrontendRouter->GetSnapshot() : FHSRFrontendRouteSnapshot{};
	const int64 OpenToken = AllocateRequestToken();

	// 1) 先推入暂停屏幕到屏幕栈；失败则直接返回
	if (ScreenStack->SubmitRequest(MakePauseRequest(OpenToken)) != EHSRScreenStackResult::Success)
	{
		return EHSRUIScreenResult::StackRejected;
	}

	// 2) 挂载外壳 Widget 到视口
	if (!AttachPauseCandidate(Candidate))
	{
		// 挂载失败：弹出刚才推入的屏幕并恢复输入策略
		return CompensatePop(OldPolicy, PC, Candidate) ? EHSRUIScreenResult::ViewportAttachFailed
			: EHSRUIScreenResult::CompensationFailed;
	}

	// 3) 应用 UI 独占输入策略
	const FHSRInputModePolicy PausePolicy = GetResolvedInputPolicy();
	if (!ApplyPolicyBackend(PC, PausePolicy, EHSRPlayerControlMode::UIOnly))
	{
		return CompensatePop(OldPolicy, PC, Candidate) ? EHSRUIScreenResult::PolicyApplyFailed
			: EHSRUIScreenResult::CompensationFailed;
	}

	// 4) 暂停整个世界（暂停屏的语义：游戏时间停止）
	if (!ApplyPauseBackend(World, true))
	{
		return CompensatePop(OldPolicy, PC, Candidate) ? EHSRUIScreenResult::PauseApplyFailed
			: EHSRUIScreenResult::CompensationFailed;
	}

	// 暂停生效后颁发暂停所有权 Token，表明这个暂停是由本 UI 管理器发起的
	PauseOwnerToken = FGuid::NewGuid();

	// 5) 应用焦点到外壳的首选焦点 Widget
	const EHSRFocusApplyResult FocusResult = ApplyFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		// 焦点不可用：回滚暂停与输入策略，移除外壳，恢复屏幕栈与路由快照
		const bool bPauseRestored = ApplyPauseBackend(World, false);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::Exploration);
		Candidate->RemoveFromParent();
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		if (FrontendRouter)
		{
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		}
		PauseOwnerToken.Invalidate();
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}

	// 6) 让前端路由定位到 PauseHub，使外壳展示暂停集线内容
	FHSRFrontendRouteRequest RouteRequest;
	RouteRequest.RequestToken = AllocateFrontendRequestToken();
	RouteRequest.Route.Module = EHSRFrontendModule::PauseHub;
	if (!FrontendRouter || FrontendRouter->Submit(RouteRequest) != EHSRFrontendRouteResult::Success)
	{
		// 路由失败：同样完整回滚
		const bool bPauseRestored = ApplyPauseBackend(World, false);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::Exploration);
		Candidate->RemoveFromParent();
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		if (FrontendRouter)
		{
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		}
		PauseOwnerToken.Invalidate();
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::StackRejected);
	}

	// 全部成功：记录外壳实例并向其呈现当前路由快照
	FrontendShellInstance = Candidate;
	Candidate->PresentRoute(FrontendRouter->GetSnapshot());
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 OpenPause Success Token=%lld Stack=%d FocusResult=%d"),
		OpenToken, GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

// 打开角色详情屏：经由前端模块入口路由到 Character 模块
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenCharacterDetailScreen()
{
	return OpenFrontendModule(EHSRFrontendModule::Character);
}

// 打开角色详情（内部实现）：依赖暂停外壳已存在（暂停屏 + PauseHub 路由）。
// 流程：创建模块根容器与角色详情 Widget → 把详情塞进容器 → 挂载容器 → 应用策略/焦点
// → 路由定位到 Character 模块。任一步失败都走对应的回滚补偿。
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenCharacterDetailInternal()
{
	// 前置校验
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (CharacterDetailWidgetInstance)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}

	// 宿主校验：角色详情允许从暂停中打开（PauseOwnerToken 有效）或探索模式打开
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!IsBackendExploration(PC) && !PauseOwnerToken.IsValid())
	{
		return EHSRUIScreenResult::NotExploration;
	}

	// 预检屏幕栈：必须恰好是 [探索根, 暂停屏]
	const FHSRScreenStackSnapshot PreflightSnapshot = ScreenStack->GetSnapshot();
	if (PreflightSnapshot.Entries.Num() != 2
		|| PreflightSnapshot.Entries[0].ScreenId != ExplorationRootId
		|| PreflightSnapshot.Entries.Last().ScreenId != PauseScreenId)
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}

	// 所需 Widget 类必须齐备
	if (!HasModuleRootClass() || !HasCharacterDetailClass())
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}

	// 创建角色详情 Widget 候选
	UHSRScreenWidget* Candidate = CreateCharacterDetailCandidate(PC);
	if (!Candidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	Candidate->SetOwningUIManager(this);

	// 创建模块根容器候选，并让它以 Character 模块身份展示
	UHSRFrontendModuleRootWidget* RootCandidate = CreateFrontendModuleRootCandidate(PC);
	if (!RootCandidate)
	{
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	RootCandidate->SetOwningUIManager(this);
	RootCandidate->PresentModule(EHSRFrontendModule::Character);

	// 记录打开前的策略与路由快照，用于失败回滚
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	bool bContentAttached = false;
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		bContentAttached = bAutomationDetailAttachSucceeds;
	}
	else
#endif
	{
		bContentAttached = RootCandidate->SetModuleContent(Candidate);
	}
	// 内容挂载失败或根容器挂载失败：清理候选并回滚策略
	if (!bContentAttached || !AttachFrontendModuleRootCandidate(RootCandidate))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		Candidate->RemoveFromParent();
		const bool bRestore = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::ViewportAttachFailed;
	}

	// 应用角色详情的 UI 输入策略
	if (!ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		Candidate->RemoveFromParent();
		const bool bRestore = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::PolicyApplyFailed;
	}

	// 应用焦点到角色详情的首选焦点 Widget
	const EHSRFocusApplyResult FocusResult = ApplyCharacterDetailFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		Candidate->RemoveFromParent();
		const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}

	// 外壳仍存活时，把前端路由定位到 Character 模块
	if (FrontendRouter && FrontendShellInstance)
	{
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Route.Module = EHSRFrontendModule::Character;
		if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			// 路由失败：清理候选并回滚路由、策略、焦点
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			Candidate->RemoveFromParent();
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(PC, OldRoute.GetActiveRoute().Module);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		// 让外壳呈现更新后的路由
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	}

	// 若此前挂着别的模块根（理论上不应发生），先释放旧的再换新的
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}

	// 若此前开着旧式背包（理论上不应发生），一并拆除并回收 ViewModel
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr);
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
		UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance;
		InventoryViewModelInstance = nullptr;
		ShutdownInventoryViewModelCandidate(OldVM);
	}

	// 记录新的模块根与角色详情实例
	FrontendModuleRootInstance = RootCandidate;
	CharacterDetailWidgetInstance = Candidate;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail Open Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

// 打开背包屏：经由前端模块入口路由到 Inventory 模块
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenInventoryScreen()
{
	return OpenFrontendModule(EHSRFrontendModule::Inventory);
}

// 打开背包屏（内部实现）：与 OpenCharacterDetailInternal 同构，但额外拥有
// 独立 ViewModel（UHSRInventoryRewardViewModel）。创建顺序：先建 ViewModel 并初始化，
// 再建 Widget 绑定 ViewModel，最后塞进模块根容器挂载。
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenInventoryInternal()
{
	// 前置校验（风格上保持与其他前端打开路径一致的守卫顺序）
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}
	if (InventoryWidgetInstance)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}

	// 宿主校验：允许从暂停或探索中打开
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!IsBackendExploration(PC) && !PauseOwnerToken.IsValid())
	{
		return EHSRUIScreenResult::NotExploration;
	}

	// 预检屏幕栈：必须恰好是 [探索根, 暂停屏]
	const FHSRScreenStackSnapshot Preflight = ScreenStack->GetSnapshot();
	if (Preflight.Entries.Num() != 2
		|| Preflight.Entries[0].ScreenId != ExplorationRootId
		|| Preflight.Entries.Last().ScreenId != PauseScreenId)
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}

	// 所需 Widget 类必须齐备
	if (!HasModuleRootClass() || !HasInventoryClass())
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}

#if WITH_DEV_AUTOMATION_TESTS
	// 自动化后端可强制依赖失败以测试错误路径
	if (bUseAutomationBackend && !bAutomationInventoryDependenciesSucceed)
		return EHSRUIScreenResult::ViewModelInitializationFailed;
#endif

	// 从 GameInstance 取背包子系统与奖励子系统作为 ViewModel 的数据源
	UHSRInventorySubsystem* Inventory = nullptr;
	UHSRRewardSubsystem* Reward = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	if (!bUseAutomationBackend)
#endif
	{
		UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
		Inventory = GameInstance ? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
		Reward = GameInstance ? GameInstance->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
		if (!Inventory || !Reward)
			return EHSRUIScreenResult::ViewModelInitializationFailed;
	}

	// 创建 ViewModel 候选
	UHSRInventoryRewardViewModel* ViewModelCandidate = CreateInventoryViewModelCandidate();
	if (!ViewModelCandidate)
	{
		return EHSRUIScreenResult::ViewModelInitializationFailed;
	}
#if WITH_DEV_AUTOMATION_TESTS
	// 自动化后端可强制快照失败
	if (bUseAutomationBackend && !bAutomationInventorySnapshotSucceeds)
	{
		ShutdownInventoryViewModelCandidate(ViewModelCandidate);
		return EHSRUIScreenResult::ViewModelInitializationFailed;
	}
	if (!bUseAutomationBackend)
#endif
	{
		// 初始化 ViewModel 并验证首帧快照可取；失败则回收 ViewModel
		ViewModelCandidate->Initialize(Inventory, Reward);
		FHSRInventoryRewardSnapshot Snapshot;
		if (!ViewModelCandidate->GetSnapshot(Snapshot))
		{
			ShutdownInventoryViewModelCandidate(ViewModelCandidate);
			return EHSRUIScreenResult::ViewModelInitializationFailed;
		}
	}

	// 创建背包 Widget 候选并绑定 ViewModel
	UHSRInventoryWidget* Candidate = CreateInventoryCandidate(PC);
	if (!Candidate)
	{
		ShutdownInventoryViewModelCandidate(ViewModelCandidate);
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	Candidate->SetOwningUIManager(this);
	Candidate->SetViewModel(ViewModelCandidate);
#if WITH_DEV_AUTOMATION_TESTS
	// Automation widgets are NewObject'd and never enter the UMG construct path, so
	// SetViewModel's IsConstructed() guard skips the bind. Real attach reaches it via
	// NativeConstruct; mirror that here so bind/unbind accounting matches production.
	// 自动化 Widget 是 NewObject 出来的，不会走 UMG 构造流程，
	// SetViewModel 的 IsConstructed() 守卫会跳过绑定；这里手动补一次绑定使计数与生产一致
	if (bUseAutomationBackend)
	{
		Candidate->AttachForAutomation();
	}
#endif

	// 创建模块根容器候选并挂上 Inventory 模块身份
	UHSRFrontendModuleRootWidget* RootCandidate = CreateFrontendModuleRootCandidate(PC);
	if (!RootCandidate)
	{
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	RootCandidate->SetOwningUIManager(this);
	RootCandidate->PresentModule(EHSRFrontendModule::Inventory);

	// 记录打开前的策略与路由快照
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	bool bContentAttached = false;
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		bContentAttached = bAutomationInventoryAttachSucceeds;
	}
	else
#endif
	{
		bContentAttached = RootCandidate->SetModuleContent(Candidate);
	}

	// 内容挂载失败或根容器挂载失败：清理候选并回滚策略
	if (!bContentAttached || !AttachFrontendModuleRootCandidate(RootCandidate))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bRestore = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::ViewportAttachFailed;
	}

	// 应用背包的 UI 输入策略
	if (!ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bRestore = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		if (!bRestore)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::PolicyApplyFailed;
	}

	// 应用焦点到背包 Widget 的首选焦点
	const EHSRFocusApplyResult FocusResult = ApplyInventoryFocusBackend(PC, Candidate->GetPreferredFocusWidget(), Candidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		RootCandidate->ClearModuleContent();
		RootCandidate->RemoveFromParent();
		ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
		const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}

	// 外壳仍存活时，把前端路由定位到 Inventory 模块
	if (FrontendRouter && FrontendShellInstance)
	{
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Route.Module = EHSRFrontendModule::Inventory;
		if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			// 路由失败：清理候选并回滚路由、策略、焦点
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ReleaseInventoryCandidates(Candidate, ViewModelCandidate);
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(PC, OldRoute.GetActiveRoute().Module);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	}

	// 若此前挂着别的模块根（理论上不应发生），先释放旧的再换新的
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}

	// 若此前开着角色详情（理论上不应发生），强制拆除
	if (CharacterDetailWidgetInstance)
	{
		CharacterDetailWidgetInstance->RemoveFromParent();
		CharacterDetailWidgetInstance = nullptr;
	}

	// 记录新的模块根、背包 Widget 与 ViewModel 实例
	FrontendModuleRootInstance = RootCandidate;
	InventoryWidgetInstance = Candidate;
	InventoryViewModelInstance = ViewModelCandidate;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory Open Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

// 请求"返回/后退"：前端 UI 的核心导航原语，按当前状态分派到不同的关闭路径。
// 优先级：对话浮层 → 共享模块根的前端模块 → 旧式背包/角色详情 → 暂停集线外壳。
// 每个分支失败时都尝试恢复到打开前的策略/焦点状态，且失败路径绝不触发"关到根"。
EHSRUIScreenResult UHSRUIManagerSubsystem::RequestBack()
{
	// 基础守卫
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}

	// 若对话浮层开着，返回键直接关闭对话浮层（它是最顶层）
	if (HasOpenDialogueOverlay())
	{
		return CloseDialogueOverlay();
	}

	// 背包持有不一致视为不可恢复
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}

	// 屏幕栈只剩探索根时无可返回
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	if (Snapshot.Entries.Num() <= 1)
	{
		return EHSRUIScreenResult::NothingOpen;
	}

	// 当前活跃前端模块（供策略/焦点分派用）
	const EHSRFrontendModule ActiveFrontendModule = FrontendRouter
		? FrontendRouter->GetSnapshot().GetActiveRoute().Module : EHSRFrontendModule::None;

	// 分支 A：共享模块根的前端模块（角色/背包/队伍/地图/挑战/任务/存档等）。
	// 这些模块的内容都挂在同一个模块根容器上，返回 = 路由回退一级 + 关闭该模块内容。
	if (FrontendModuleRootInstance || HSRFrontendModule::UsesSharedModuleRoot(ActiveFrontendModule))
	{
		const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
		const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();

		// 按模块分发输入策略应用：角色/背包各自走专门后端，其余走通用后端
		const auto ApplyActiveModulePolicy = [this, ActiveFrontendModule](const FHSRInputModePolicy& Policy)
		{
			switch (ActiveFrontendModule)
			{
			case EHSRFrontendModule::Character:
				return ApplyCharacterDetailPolicyBackend(RegisteredPlayerController.Get(), Policy, EHSRPlayerControlMode::UIOnly);
			case EHSRFrontendModule::Inventory:
				return ApplyInventoryPolicyBackend(RegisteredPlayerController.Get(), Policy, EHSRPlayerControlMode::UIOnly);
			default:
				return ApplyPolicyBackend(RegisteredPlayerController.Get(), Policy, EHSRPlayerControlMode::UIOnly);
			}
		};
		// 按模块分发焦点应用：同样角色/背包走专门后端
		const auto ApplyActiveModuleFocus = [this, ActiveFrontendModule]()
		{
			switch (ActiveFrontendModule)
			{
			case EHSRFrontendModule::Character:
				return ApplyCharacterDetailFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance, FrontendShellInstance);
			case EHSRFrontendModule::Inventory:
				return ApplyInventoryFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance, FrontendShellInstance);
			default:
				return ApplyFocusBackend(RegisteredPlayerController.Get(), FrontendShellInstance->GetPreferredFocusWidget(), FrontendShellInstance);
			}
		};

		// Policy and focus report separately on purpose: a caller that sees FocusApplyFailed for a
		// policy rejection cannot tell which backend refused, and the close-path tests distinguish
		// the two. Both arms compensate by restoring the previous policy before returning.
		// 策略与焦点分开上报：若二者合并，调用方看到 FocusApplyFailed 时无法区分是被策略
		// 拒绝还是被焦点拒绝，关闭路径的测试也需要区分二者。两个失败分支都先恢复旧策略再返回。
		if (!ApplyActiveModulePolicy(GetResolvedInputPolicy()))
		{
			const bool bPolicyRestored = ApplyActiveModulePolicy(OldPolicy);
			return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed);
		}
		if (ApplyActiveModuleFocus() == EHSRFocusApplyResult::Unavailable)
		{
			const bool bPolicyRestored = ApplyActiveModulePolicy(OldPolicy);
			return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
		}

		// 让路由执行 Back 操作
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
		if (!FrontendRouter || SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
		{
			// 路由失败：回滚路由快照、策略与焦点
			if (FrontendRouter)
			{
				FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			}
			const bool bPolicyRestored = ApplyActiveModulePolicy(OldPolicy);
			const bool bFocusRestored = RestoreFrontendModuleFocus(RegisteredPlayerController.Get(), ActiveFrontendModule);
			return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
		}

		// 路由回退成功后，释放模块根容器里挂着的模块内容
		if (FrontendModuleRootInstance)
		{
			ReleaseFrontendModuleContent();
			FrontendModuleRootInstance->RemoveFromParent();
		}
		FrontendModuleRootInstance = nullptr;
		FrontendModuleContentModule = EHSRFrontendModule::None;

		// 按模块清空对应的旧式实例引用
		if (ActiveFrontendModule == EHSRFrontendModule::Character)
		{
			CharacterDetailWidgetInstance = nullptr;
		}
		else if (ActiveFrontendModule == EHSRFrontendModule::Inventory)
		{
			if (InventoryWidgetInstance)
			{
				InventoryWidgetInstance->SetViewModel(nullptr);
#if WITH_DEV_AUTOMATION_TESTS
				LastReleasedInventoryBindCount = InventoryWidgetInstance->GetBindCountForAutomation();
				LastReleasedInventoryUnbindCount = InventoryWidgetInstance->GetUnbindCountForAutomation();
#endif
				InventoryWidgetInstance = nullptr;
			}
			UHSRInventoryRewardViewModel* ViewModelToShutdown = InventoryViewModelInstance;
			InventoryViewModelInstance = nullptr;
			ShutdownInventoryViewModelCandidate(ViewModelToShutdown);
		}

		// 外壳呈现回退后的路由快照
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());

		// A module reopened by travel restore has no shell layer beneath it to fall back to, so
		// backing out of it must land on the root rather than leaving the shell on the stack.
		// Placed after the compensating early returns above: a failed close must not close to root.
		// 旅行恢复重新打开的模块没有下层外壳可回退，因此从它返回必须直接关到根。
		// 这段放在所有补偿性提前返回之后：失败的关闭绝不能触发关到根。
		if (bTravelRestoredModule)
		{
			bTravelRestoredModule = false;
			return CloseFrontendToRoot();
		}
		return EHSRUIScreenResult::Success;
	}

	// 分支 B：旧式独立背包屏（InventoryWidgetInstance 形式，非共享模块根）
	if (ActiveFrontendModule == EHSRFrontendModule::Inventory)
	{
		if (!InventoryWidgetInstance || !InventoryViewModelInstance || CharacterDetailWidgetInstance)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::Inconsistent;
		}
		const EHSRUIScreenResult Result = CloseInventoryScreen();
		// 旅行恢复的模块同样要关到根
		if (Result == EHSRUIScreenResult::Success && bTravelRestoredModule)
		{
			bTravelRestoredModule = false;
			return CloseFrontendToRoot();
		}
		return Result;
	}

	// 分支 C：旧式独立角色详情屏
	if (ActiveFrontendModule == EHSRFrontendModule::Character)
	{
		if (!CharacterDetailWidgetInstance || InventoryWidgetInstance)
		{
			bInconsistent = true;
			return EHSRUIScreenResult::Inconsistent;
		}
		const EHSRUIScreenResult Result = CloseCharacterDetailScreen();
		if (Result == EHSRUIScreenResult::Success && bTravelRestoredModule)
		{
			bTravelRestoredModule = false;
			return CloseFrontendToRoot();
		}
		return Result;
	}

	// 分支 D：暂停集线外壳。栈顶必须是暂停屏且路由在 PauseHub、无任何子模块实例，
	// 否则说明栈/路由与实例状态失配，属于不可恢复不一致
	const FName TopId = Snapshot.Entries.Last().ScreenId;
	if (TopId != PauseScreenId || ActiveFrontendModule != EHSRFrontendModule::PauseHub
		|| !FrontendShellInstance || !PauseOwnerToken.IsValid()
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("HSRUI Frontend hub close invariant Top=%s Shell=%s PauseOwner=%s Character=%s Inventory=%s"),
			*TopId.ToString(), FrontendShellInstance ? TEXT("true") : TEXT("false"),
			PauseOwnerToken.IsValid() ? TEXT("true") : TEXT("false"),
			CharacterDetailWidgetInstance ? TEXT("true") : TEXT("false"), InventoryWidgetInstance ? TEXT("true") : TEXT("false"));
		bInconsistent = true;
		return EHSRUIScreenResult::Inconsistent;
	}

	// 分支 D 正常路径：关闭前端到根（弹出暂停屏、恢复探索策略与暂停状态）
	return CloseFrontendToRoot();
}

// 打开任意前端模块：统一入口。None 直接拒绝；若对话浮层占道则拒绝；
// 外壳未开时先开暂停屏；再按模块类型分派到对应打开路径。
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenFrontendModule(const EHSRFrontendModule Module)
{
	// None 不是有效模块
	if (Module == EHSRFrontendModule::None)
	{
		return EHSRUIScreenResult::StackRejected;
	}
	// 对话浮层占道时不允许打开任何前端模块
	if (HasDialogueOverlayBlockingFrontend())
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}
	bool bOpenedShell = false;
	// 外壳未开：先打开暂停屏作为宿主
	if (!FrontendShellInstance)
	{
		const EHSRUIScreenResult ShellResult = OpenPauseScreen();
		if (ShellResult != EHSRUIScreenResult::Success)
		{
			return ShellResult;
		}
		bOpenedShell = true;
	}

	// PauseHub 模块：若外壳是刚开的，则已经停驻在 PauseHub，直接成功
	if (Module == EHSRFrontendModule::PauseHub)
	{
		if (bOpenedShell)
		{
			return EHSRUIScreenResult::Success;
		}
		// 外壳已存在但不在 PauseHub：先返回（后退）到 PauseHub；已在其上则 NoOp
		return FrontendRouter && FrontendRouter->GetSnapshot().GetActiveRoute().Module == EHSRFrontendModule::PauseHub
			? EHSRUIScreenResult::NoOp : RequestBack();
	}

	// 已停留在目标模块：发一个 NoOp 请求以保持路由幂等
	if (FrontendRouter && FrontendRouter->GetSnapshot().GetActiveRoute().Module == Module)
	{
		FHSRFrontendRouteRequest NoOpRequest;
		NoOpRequest.RequestToken = AllocateFrontendRequestToken();
		NoOpRequest.Route.Module = Module;
		return FrontendRouter->Submit(NoOpRequest) == EHSRFrontendRouteResult::NoOp
			? EHSRUIScreenResult::NoOp : EHSRUIScreenResult::StackRejected;
	}

	// CompleteModuleAttempt：当本次调用新开了外壳但模块打开失败时，必须把外壳一并收起，
	// 避免留下一个没有实际内容的前端外壳
	const auto CompleteModuleAttempt = [this, bOpenedShell](const EHSRUIScreenResult Result)
	{
		if (bOpenedShell && Result != EHSRUIScreenResult::Success && Result != EHSRUIScreenResult::NoOp)
		{
			if (CloseFrontendToRoot() != EHSRUIScreenResult::Success)
			{
				bInconsistent = true;
				return EHSRUIScreenResult::CompensationFailed;
			}
		}
		return Result;
	};

	switch (Module)
	{
	case EHSRFrontendModule::Character:
		return CompleteModuleAttempt(OpenCharacterDetailInternal());
	case EHSRFrontendModule::Inventory:
	#if WITH_DEV_AUTOMATION_TESTS
		if (!InventoryModuleWidgetClass && !bAutomationUseInventoryModuleContent)
	#else
		if (!InventoryModuleWidgetClass)
	#endif
		{
			// 未配置模块化背包内容时走旧式独立背包屏路径
			return CompleteModuleAttempt(OpenInventoryInternal());
		}
		// 配置了模块化背包内容则落入共享模块根路径
		[[fallthrough]];
	case EHSRFrontendModule::Party:
	case EHSRFrontendModule::Map:
	case EHSRFrontendModule::Challenge:
	case EHSRFrontendModule::Quest:
	case EHSRFrontendModule::Save:
	{
	#if WITH_DEV_AUTOMATION_TESTS
		if ((!bUseAutomationBackend && (!FrontendModuleRootClass || !GetFrontendModuleWidgetClass(Module)))
			|| (bUseAutomationBackend && !bAutomationHasFrontendModuleClass))
	#else
		if (!FrontendModuleRootClass || !GetFrontendModuleWidgetClass(Module))
	#endif
		{
			return CompleteModuleAttempt(EHSRUIScreenResult::MissingWidgetClass);
		}

		// 共享模块根路径：创建模块根容器与内容 Widget
		UHSRFrontendModuleRootWidget* RootCandidate = CreateFrontendModuleRootCandidate(RegisteredPlayerController.Get());
		if (!RootCandidate)
		{
			return CompleteModuleAttempt(EHSRUIScreenResult::WidgetCreationFailed);
		}
		UUserWidget* ContentCandidate = CreateFrontendModuleContentCandidate(RegisteredPlayerController.Get(), Module);
		if (!ContentCandidate)
		{
			RootCandidate->RemoveFromParent();
			return CompleteModuleAttempt(EHSRUIScreenResult::WidgetCreationFailed);
		}

		// 若内容是模块化背包 Widget，需要先初始化其命令上下文（面向当前活跃队员），
		// 并验证首帧快照可取；失败则回收候选
		if (UHSRInventoryModuleWidget* InventoryContent = Cast<UHSRInventoryModuleWidget>(ContentCandidate))
		{
			InventoryContent->InitializeCommandContext(ResolveInventoryCharacterGuid());
	#if WITH_DEV_AUTOMATION_TESTS
			if (!bUseAutomationBackend)
	#endif
			{
				FHSRInventoryModuleSnapshot InventorySnapshot;
				if (!InventoryContent->GetCurrentSnapshot(InventorySnapshot)
					|| !InventorySnapshot.bIsValid)
				{
					RootCandidate->RemoveFromParent();
					ContentCandidate->RemoveFromParent();
					return CompleteModuleAttempt(EHSRUIScreenResult::ViewModelInitializationFailed);
				}
			}
		}

		RootCandidate->SetOwningUIManager(this);

		// 若内容本身是 ScreenWidget（有首选焦点），提取其首选焦点优先用于后续焦点应用
		UWidget* ContentPreferredFocus = nullptr;
		if (UHSRScreenWidget* ScreenContent = Cast<UHSRScreenWidget>(ContentCandidate))
		{
			ScreenContent->SetOwningUIManager(this);
			ContentPreferredFocus = ScreenContent->GetPreferredFocusWidget();
		}

		RootCandidate->PresentModule(Module);

		// 记录打开前的路由与策略快照
		const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
		const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();

		// 把内容塞进根容器并挂载根容器到视口
		if (!AttachFrontendModuleContentCandidate(RootCandidate, ContentCandidate)
			|| !AttachFrontendModuleRootCandidate(RootCandidate))
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			return CompleteModuleAttempt(EHSRUIScreenResult::ViewportAttachFailed);
		}

		// 应用 UI 独占输入策略
		if (!ApplyPolicyBackend(RegisteredPlayerController.Get(), GetResolvedInputPolicy(), EHSRPlayerControlMode::UIOnly))
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return CompleteModuleAttempt(ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed));
		}

		// 选择焦点目标：优先内容自己的首选焦点，否则用根容器的
		UWidget* PreferredFocus = RootCandidate->GetPreferredFocusWidget();
		if (ContentPreferredFocus)
		{
			PreferredFocus = ContentPreferredFocus;
		}
		if (ApplyFocusBackend(RegisteredPlayerController.Get(), PreferredFocus, RootCandidate)
			== EHSRFocusApplyResult::Unavailable)
		{
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			return CompleteModuleAttempt(ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed));
		}

		// 提交前端路由请求，定位到目标模块
		FHSRFrontendRouteRequest RouteRequest;
		RouteRequest.RequestToken = AllocateFrontendRequestToken();
		RouteRequest.Route.Module = Module;
		const EHSRFrontendRouteResult RouteResult = SubmitFrontendRoute(RouteRequest);
		if (RouteResult != EHSRFrontendRouteResult::Success && RouteResult != EHSRFrontendRouteResult::NoOp)
		{
			// 路由失败：完整回滚（清内容、拆容器、回滚路由/策略/焦点）
			RootCandidate->ClearModuleContent();
			RootCandidate->RemoveFromParent();
			ContentCandidate->RemoveFromParent();
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
			const bool bPolicyRestored = ApplyPolicyBackend(RegisteredPlayerController.Get(), OldPolicy, EHSRPlayerControlMode::UIOnly);
			const bool bFocusRestored = RestoreFrontendModuleFocus(
				RegisteredPlayerController.Get(), OldRoute.GetActiveRoute().Module);
			return CompleteModuleAttempt(ResolveCompensation(
				bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected));
		}

		// 成功：先释放旧的模块内容与根容器（理论上不应存在），再记录新实例
		ReleaseFrontendModuleContent();
		if (FrontendModuleRootInstance)
		{
			FrontendModuleRootInstance->RemoveFromParent();
			FrontendModuleRootInstance = nullptr;
		}

		// 若开着旧式角色详情/背包，强制拆除（与共享模块根路径互斥）
		if (CharacterDetailWidgetInstance)
		{
			CharacterDetailWidgetInstance->RemoveFromParent();
			CharacterDetailWidgetInstance = nullptr;
		}
		if (InventoryWidgetInstance)
		{
			InventoryWidgetInstance->SetViewModel(nullptr);
			InventoryWidgetInstance->RemoveFromParent();
			InventoryWidgetInstance = nullptr;
			UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance;
			InventoryViewModelInstance = nullptr;
			ShutdownInventoryViewModelCandidate(OldVM);
		}

		FrontendModuleRootInstance = RootCandidate;
		FrontendModuleContentInstance = ContentCandidate;
		FrontendModuleContentModule = Module;
		FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
		// NoOp 请求也视为成功打开，但结果类型保持 NoOp 供调用方区分
		return RouteResult == EHSRFrontendRouteResult::NoOp ? EHSRUIScreenResult::NoOp : EHSRUIScreenResult::Success;
	}
	case EHSRFrontendModule::None:
	default:
		return EHSRUIScreenResult::StackRejected;
	}
}

// 是否有对话浮层正在阻挡前端：对话浮层打开时禁止任何前端模块介入
bool UHSRUIManagerSubsystem::HasDialogueOverlayBlockingFrontend() const
{
	return DialogueOverlayWidgetInstance != nullptr || DialogueViewModelInstance != nullptr;
}

// 打开对话浮层的公开入口：转发到内部实现
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenDialogueOverlay(const FName DialogueId, const FName NodeId)
{
	return OpenDialogueOverlayInternal(DialogueId, NodeId);
}

// 打开对话浮层（内部实现）：创建对话 ViewModel → 绑定到浮层 Widget → 挂载到视口
// → 应用 UI 独占策略与焦点。对话浮层是探索世界上的最高层，不与任何前端模块并存。
EHSRUIScreenResult UHSRUIManagerSubsystem::OpenDialogueOverlayInternal(
	const FName DialogueId, const FName NodeId)
{
	// 基础守卫
	if (!bInitialized || !ScreenStack || !InputModeCoordinator)
	{
		return EHSRUIScreenResult::NotInitialized;
	}
	// 对话 ID 与节点 ID 都必须有效
	if (DialogueId.IsNone() || NodeId.IsNone())
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (bInconsistent)
	{
		return EHSRUIScreenResult::Inconsistent;
	}
	// 旅行进行中不允许叠加对话浮层
	if (IsTravelPending())
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	// 已有对话浮层或任何前端实例时拒绝重复打开
	if (HasDialogueOverlayBlockingFrontend())
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}
	if (FrontendShellInstance || FrontendModuleRootInstance || FrontendModuleContentInstance
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}

	// 宿主校验
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!IsBackendExploration(PC))
	{
		return EHSRUIScreenResult::NotExploration;
	}

	// 对话浮层要求屏幕栈恰好只有探索根（叠加在最上层）
	const FHSRScreenStackSnapshot StackSnapshot = ScreenStack->GetSnapshot();
	if (StackSnapshot.Entries.Num() != 1 || StackSnapshot.Entries[0].ScreenId != ExplorationRootId)
	{
		return EHSRUIScreenResult::AlreadyOpen;
	}

	// 浮层 Widget 类必须可用
#if WITH_DEV_AUTOMATION_TESTS
	if ((!bUseAutomationBackend && !DialogueOverlayWidgetClass)
		|| (bUseAutomationBackend && !bAutomationHasDialogueOverlayClass))
#else
	if (!DialogueOverlayWidgetClass)
#endif
	{
		return EHSRUIScreenResult::MissingWidgetClass;
	}

	// 创建对话展示 ViewModel
	UHSRDialoguePresentationViewModel* ViewModelCandidate = NewObject<UHSRDialoguePresentationViewModel>(this);
	if (!ViewModelCandidate)
	{
		return EHSRUIScreenResult::ViewModelInitializationFailed;
	}

	// 初始化 ViewModel 并启动对话：绑定对话子系统，构造请求并 BeginDialogue
#if WITH_DEV_AUTOMATION_TESTS
	if (!bUseAutomationBackend)
#endif
	{
		UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
		UHSRDialogueSubsystem* Dialogue = GameInstance
			? GameInstance->GetSubsystem<UHSRDialogueSubsystem>() : nullptr;
		if (!Dialogue)
		{
			return EHSRUIScreenResult::ViewModelInitializationFailed;
		}
		ViewModelCandidate->Initialize(Dialogue);

		// 构造对话展示请求：QueryId 用新 GUID 标识本次查询，配合对话/节点 ID 精确定位
		FHSRDialoguePresentationRequest Request;
		Request.QueryId = FGuid::NewGuid();
		Request.DialogueId = DialogueId;
		Request.NodeId = NodeId;
		if (ViewModelCandidate->BeginDialogue(Request) != EHSRDialoguePresentationResult::Success)
		{
			ViewModelCandidate->Shutdown();
			return EHSRUIScreenResult::ViewModelInitializationFailed;
		}
	}
#if WITH_DEV_AUTOMATION_TESTS
	else
	{
		ViewModelCandidate->Initialize(nullptr);
	}
#endif

	// 创建对话浮层 Widget 并绑定 ViewModel
	UHSRDialogueOverlayWidget* WidgetCandidate = CreateDialogueOverlayCandidate(PC);
	if (!WidgetCandidate)
	{
		ViewModelCandidate->Shutdown();
		return EHSRUIScreenResult::WidgetCreationFailed;
	}
	WidgetCandidate->SetOwningUIManager(this);
	WidgetCandidate->SetViewModel(ViewModelCandidate);

	// 挂载浮层到视口（最高层）
	if (!AttachDialogueOverlayCandidate(WidgetCandidate))
	{
		WidgetCandidate->SetViewModel(nullptr);
		WidgetCandidate->RemoveFromParent();
		ViewModelCandidate->Shutdown();
		return EHSRUIScreenResult::ViewportAttachFailed;
	}

	// 应用自定义的对话输入策略（UI 独占 + 显示鼠标 + 专属焦点 Token/屏幕 ID）
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();
	FHSRInputModePolicy DialoguePolicy;
	DialoguePolicy.InputIntent = EHSRUIInputIntent::UIOnly;
	DialoguePolicy.bShowMouseCursor = true;
	DialoguePolicy.PreferredFocusToken = TEXT("UI.Focus.DialogueOverlay");
	DialoguePolicy.OwningScreenId = TEXT("UI.Screen.DialogueOverlay");
	if (!ApplyPolicyBackend(PC, DialoguePolicy, EHSRPlayerControlMode::UIOnly))
	{
		// 策略失败：清理并恢复旧策略
		WidgetCandidate->SetViewModel(nullptr);
		WidgetCandidate->RemoveFromParent();
		ViewModelCandidate->Shutdown();
		if (!ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::Exploration))
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::PolicyApplyFailed;
	}

	// 应用焦点到浮层的首选焦点
	const EHSRFocusApplyResult FocusResult = ApplyDialogueFocusBackend(
		PC, WidgetCandidate->GetPreferredFocusWidget(), WidgetCandidate);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		// 焦点失败：同样清理并恢复旧策略
		WidgetCandidate->SetViewModel(nullptr);
		WidgetCandidate->RemoveFromParent();
		ViewModelCandidate->Shutdown();
		if (!ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::Exploration))
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::FocusApplyFailed;
	}

	// 全部成功：记录实例
	DialogueOverlayWidgetInstance = WidgetCandidate;
	DialogueViewModelInstance = ViewModelCandidate;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue Overlay Open Success Dialogue=%s Node=%s FocusResult=%d"),
		*DialogueId.ToString(), *NodeId.ToString(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

// 关闭对话浮层的公开入口：转发到内部实现（恢复输入策略）
EHSRUIScreenResult UHSRUIManagerSubsystem::CloseDialogueOverlay()
{
	return CloseDialogueOverlayInternal(true);
}

// 关闭对话浮层（内部实现）：bRestoreInputPolicy=false 用于旅行清理等"不恢复输入"场景
EHSRUIScreenResult UHSRUIManagerSubsystem::CloseDialogueOverlayInternal(const bool bRestoreInputPolicy)
{
	// 本来就什么都没有
	if (!DialogueOverlayWidgetInstance && !DialogueViewModelInstance)
	{
		return EHSRUIScreenResult::NothingOpen;
	}

	// 不要求恢复输入策略时直接强制释放（例如旅行拆除）
	if (!bRestoreInputPolicy)
	{
		ReleaseDialogueOverlay();
		return EHSRUIScreenResult::Success;
	}

	// 正常关闭：先校验宿主，把输入策略恢复到探索模式，再释放浮层
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (!ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration))
	{
		return EHSRUIScreenResult::PolicyApplyFailed;
	}
	ReleaseDialogueOverlay();
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue Overlay Closed"));
	return EHSRUIScreenResult::Success;
}

// 强制释放对话浮层的 Widget 与 ViewModel（成对解绑、移除、关闭）
void UHSRUIManagerSubsystem::ReleaseDialogueOverlay()
{
	if (DialogueOverlayWidgetInstance)
	{
		DialogueOverlayWidgetInstance->SetViewModel(nullptr);
		DialogueOverlayWidgetInstance->RemoveFromParent();
		DialogueOverlayWidgetInstance = nullptr;
	}
	if (DialogueViewModelInstance)
	{
		DialogueViewModelInstance->Shutdown();
		DialogueViewModelInstance = nullptr;
	}
}

// 创建对话浮层 Widget（自动化后端用 NewObject + 标志位模拟创建结果）
UHSRDialogueOverlayWidget* UHSRUIManagerSubsystem::CreateDialogueOverlayCandidate(
	AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationDialogueOverlayCreateSucceeds
			? NewObject<UHSRDialogueOverlayWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRDialogueOverlayWidget>(PlayerController, DialogueOverlayWidgetClass);
}

// 挂载对话浮层候选到视口（120 层，高于所有前端层）
bool UHSRUIManagerSubsystem::AttachDialogueOverlayCandidate(UHSRDialogueOverlayWidget* Candidate)
{
	if (!Candidate)
	{
		return false;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationDialogueOverlayAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(120);
	return Candidate->IsInViewport();
}

// 对话浮层焦点应用：直接复用通用焦点后端
EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyDialogueFocusBackend(
	AHSRPlayerController* PlayerController, UWidget* Preferred, UWidget* Fallback)
{
	return ApplyFocusBackend(PlayerController, Preferred, Fallback);
}

// 关闭整个前端到探索根：弹出全部前端屏幕、恢复探索输入策略、取消暂停、聚焦到根 Widget。
// 是暂停屏、旅行恢复模块等的"一键收起"路径，任何一步失败都会回滚到之前状态。
EHSRUIScreenResult UHSRUIManagerSubsystem::CloseFrontendToRoot()
{
	// 若对话浮层开着，先关闭它（对话浮层是前端之上的独立层）
	if (HasOpenDialogueOverlay())
	{
		return CloseDialogueOverlay();
	}
	// 外壳不在说明前端本就关闭
	if (!FrontendShellInstance)
	{
		return EHSRUIScreenResult::NothingOpen;
	}
	// 宿主校验
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}

	// 记录关闭前的栈/路由/策略快照，供失败回滚
	const FHSRScreenStackSnapshot OldStack = ScreenStack->GetSnapshot();
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter ? FrontendRouter->GetSnapshot() : FHSRFrontendRouteSnapshot{};
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();

	// 1) 让屏幕栈执行 CloseToRoot（弹出到仅剩探索根）
	const FHSRScreenRequest CloseRequest{AllocateRequestToken(), EHSRScreenStackOperation::CloseToRoot};
	const EHSRScreenStackResult StackResult = ScreenStack->SubmitRequest(CloseRequest);
	if (StackResult != EHSRScreenStackResult::Success && StackResult != EHSRScreenStackResult::NoOp)
		return EHSRUIScreenResult::StackRejected;

	// 2) 恢复探索输入策略；失败则回滚栈快照与策略
	if (!ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration))
	{
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PolicyApplyFailed);
	}

	// 3) 取消世界暂停（若暂停由本管理器持有且世界仍暂停）；失败则回滚
	if (PauseOwnerToken.IsValid() && IsBackendPaused(World) && !ApplyPauseBackend(World, false))
	{
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPolicyRestored, EHSRUIScreenResult::PauseApplyFailed);
	}

	// 4) 聚焦回根 Widget；失败则先恢复暂停与策略再回滚
	if (ApplyFocusBackend(PC, RootWidget, RootWidget) == EHSRFocusApplyResult::Unavailable)
	{
		const bool bPauseRestored = ApplyPauseBackend(World, true);
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::FocusApplyFailed);
	}

	// 5) 让前端路由执行 CloseToRoot；失败则回滚暂停/栈/路由/策略
	FHSRFrontendRouteRequest RouteRequest;
	RouteRequest.RequestToken = AllocateFrontendRequestToken();
	RouteRequest.Operation = EHSRFrontendRouteOperation::CloseToRoot;
	if (!FrontendRouter || (FrontendRouter->Submit(RouteRequest) != EHSRFrontendRouteResult::Success
		&& FrontendRouter->GetSnapshot().IsOpen()))
	{
		const bool bPauseRestored = ApplyPauseBackend(World, true);
		ScreenStack->RestoreSnapshotForTransaction(OldStack);
		if (FrontendRouter)
		{
			FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		}
		const bool bPolicyRestored = ApplyPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		return ResolveCompensation(bPauseRestored && bPolicyRestored, EHSRUIScreenResult::StackRejected);
	}

	// 6) 释放模块内容与模块根容器
	if (FrontendModuleRootInstance)
	{
		ReleaseFrontendModuleContent();
		FrontendModuleRootInstance->RemoveFromParent();
		FrontendModuleRootInstance = nullptr;
	}

	// 7) 拆除旧式角色详情与背包（若存在）
	if (CharacterDetailWidgetInstance)
	{
		CharacterDetailWidgetInstance->RemoveFromParent();
		CharacterDetailWidgetInstance = nullptr;
	}
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr);
		InventoryWidgetInstance->RemoveFromParent();
		InventoryWidgetInstance = nullptr;
		UHSRInventoryRewardViewModel* OldVM = InventoryViewModelInstance;
		InventoryViewModelInstance = nullptr;
		ShutdownInventoryViewModelCandidate(OldVM);
	}

	// 8) 移除前端外壳并撤销暂停所有权
	FrontendShellInstance->RemoveFromParent();
	FrontendShellInstance = nullptr;
	PauseOwnerToken.Invalidate();
	return EHSRUIScreenResult::Success;
}

// 关闭旧式角色详情屏：应用策略/焦点回退，路由 Back，再拆除详情 Widget
EHSRUIScreenResult UHSRUIManagerSubsystem::CloseCharacterDetailScreen()
{
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!CharacterDetailWidgetInstance || !IsBackendHostValid(PC, RootWidget, World))
	{
		return EHSRUIScreenResult::InvalidHost;
	}

	// 记录关闭前的路由与策略快照
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();

	// 1) 应用角色详情策略回退：若外壳仍在则为 UIOnly，否则回到探索模式
	if (!ApplyCharacterDetailPolicyBackend(PC, GetResolvedInputPolicy(), FrontendShellInstance ? EHSRPlayerControlMode::UIOnly : EHSRPlayerControlMode::Exploration))
	{
		if (!ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly))
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::PolicyApplyFailed;
	}

	// 2) 应用焦点回到外壳
	const EHSRFocusApplyResult FocusResult = ApplyCharacterDetailFocusBackend(PC, FrontendShellInstance, FrontendShellInstance);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		if (!ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly))
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::FocusApplyFailed;
	}

	// 3) 路由执行 Back（离开 Character 模块）
	FHSRFrontendRouteRequest RouteRequest;
	RouteRequest.RequestToken = AllocateFrontendRequestToken();
	RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
	if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
	{
		// 路由失败：回滚路由快照、策略与焦点
		FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		const bool bPolicyRestored = ApplyCharacterDetailPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		const bool bFocusRestored = ApplyCharacterDetailFocusBackend(PC,
			CharacterDetailWidgetInstance->GetPreferredFocusWidget(), CharacterDetailWidgetInstance) != EHSRFocusApplyResult::Unavailable;
		return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
	}

	// 4) 成功：拆除详情 Widget，外壳呈现更新后的路由
	CharacterDetailWidgetInstance->RemoveFromParent();
	CharacterDetailWidgetInstance = nullptr;
	FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail Close Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

// 关闭旧式独立背包屏：与 CloseCharacterDetailScreen 同构，但额外要 Shutdown ViewModel
EHSRUIScreenResult UHSRUIManagerSubsystem::CloseInventoryScreen()
{
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	UHSRUserWidget* RootWidget = RegisteredRootWidget.Get();
	UWorld* World = PC ? PC->GetWorld() : nullptr;
	if (!InventoryWidgetInstance || !InventoryViewModelInstance || !IsBackendHostValid(PC, RootWidget, World))
		return EHSRUIScreenResult::InvalidHost;

	// 记录关闭前的路由与策略快照
	const FHSRFrontendRouteSnapshot OldRoute = FrontendRouter->GetSnapshot();
	const FHSRInputModePolicy OldPolicy = GetResolvedInputPolicy();

	// 1) 应用背包策略回退
	if (!ApplyInventoryPolicyBackend(PC, GetResolvedInputPolicy(), FrontendShellInstance ? EHSRPlayerControlMode::UIOnly : EHSRPlayerControlMode::Exploration))
	{
		if (!ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly))
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::PolicyApplyFailed;
	}

	// 2) 应用焦点回到外壳
	const EHSRFocusApplyResult FocusResult = ApplyInventoryFocusBackend(PC, FrontendShellInstance, FrontendShellInstance);
	if (FocusResult == EHSRFocusApplyResult::Unavailable)
	{
		if (!ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly))
		{
			bInconsistent = true;
			return EHSRUIScreenResult::CompensationFailed;
		}
		return EHSRUIScreenResult::FocusApplyFailed;
	}

	// 3) 路由执行 Back（离开 Inventory 模块）
	FHSRFrontendRouteRequest RouteRequest;
	RouteRequest.RequestToken = AllocateFrontendRequestToken();
	RouteRequest.Operation = EHSRFrontendRouteOperation::Back;
	if (SubmitFrontendRoute(RouteRequest) != EHSRFrontendRouteResult::Success)
	{
		// 路由失败：回滚路由快照、策略与焦点
		FrontendRouter->RestoreSnapshotForTransaction(OldRoute);
		const bool bPolicyRestored = ApplyInventoryPolicyBackend(PC, OldPolicy, EHSRPlayerControlMode::UIOnly);
		const bool bFocusRestored = ApplyInventoryFocusBackend(PC,
			InventoryWidgetInstance->GetPreferredFocusWidget(), InventoryWidgetInstance) != EHSRFocusApplyResult::Unavailable;
		return ResolveCompensation(bPolicyRestored && bFocusRestored, EHSRUIScreenResult::StackRejected);
	}

	// 4) 成功：先解绑 ViewModel（记录自动化绑定计数），再移除 Widget，最后 Shutdown ViewModel
	InventoryWidgetInstance->SetViewModel(nullptr);
#if WITH_DEV_AUTOMATION_TESTS
	LastReleasedInventoryBindCount = InventoryWidgetInstance->GetBindCountForAutomation();
	LastReleasedInventoryUnbindCount = InventoryWidgetInstance->GetUnbindCountForAutomation();
#endif
	InventoryWidgetInstance->RemoveFromParent();
	InventoryWidgetInstance = nullptr;
	UHSRInventoryRewardViewModel* ViewModelToShutdown = InventoryViewModelInstance;
	InventoryViewModelInstance = nullptr;
	ShutdownInventoryViewModelCandidate(ViewModelToShutdown);

	// 外壳呈现更新后的路由
	FrontendShellInstance->PresentRoute(FrontendRouter->GetSnapshot());
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory Close Success Stack=%d FocusResult=%d"),
		GetLogicalScreenCount(), static_cast<uint8>(FocusResult));
	return EHSRUIScreenResult::Success;
}

// 分配屏幕栈请求 Token：保证单调递增，且不低于屏幕栈已处理的最大 Token+1
// （防止旅行恢复/快照回滚后 Token 倒退回已处理区间导致请求被判定为陈旧）
int64 UHSRUIManagerSubsystem::AllocateRequestToken()
{
	if (ScreenStack)
	{
		NextRequestToken = FMath::Max(NextRequestToken, ScreenStack->GetSnapshot().LastProcessedRequestToken + 1);
	}
	return NextRequestToken++;
}

// 恢复指定前端模块的焦点。共享模块根的模块统一走根容器焦点；
// 角色/背包/暂停集线各自走专门后端。新增模块若漏加 case 会静默丢焦点，
// 因此共享模块先经过 UsesSharedModuleRoot 谓词统一处理。
bool UHSRUIManagerSubsystem::RestoreFrontendModuleFocus(AHSRPlayerController* PlayerController,
	const EHSRFrontendModule Module)
{
	// Shared-root modules all restore focus the same way, so they route through the predicate rather
	// than a case list. Listing them here as well meant a new module compiled and silently lost focus
	// restore until someone noticed the missing case.
	if (HSRFrontendModule::UsesSharedModuleRoot(Module))
	{
		return FrontendModuleRootInstance
			&& ApplyFocusBackend(PlayerController, FrontendModuleRootInstance->GetPreferredFocusWidget(),
				FrontendModuleRootInstance) != EHSRFocusApplyResult::Unavailable;
	}
	switch (Module)
	{
	case EHSRFrontendModule::Character:
		return CharacterDetailWidgetInstance
			&& ApplyCharacterDetailFocusBackend(PlayerController,
				CharacterDetailWidgetInstance->GetPreferredFocusWidget(), CharacterDetailWidgetInstance)
				!= EHSRFocusApplyResult::Unavailable;
	case EHSRFrontendModule::Inventory:
	{
		// 旧式独立背包优先聚焦背包 Widget；模块化背包则聚焦内容（或其首选焦点）
		if (InventoryWidgetInstance)
		{
			return ApplyInventoryFocusBackend(PlayerController,
				InventoryWidgetInstance->GetPreferredFocusWidget(), InventoryWidgetInstance)
				!= EHSRFocusApplyResult::Unavailable;
		}
		UWidget* PreferredFocus = FrontendModuleContentInstance;
		if (UHSRScreenWidget* ScreenContent = Cast<UHSRScreenWidget>(FrontendModuleContentInstance))
		{
			if (UWidget* ContentPreferredFocus = ScreenContent->GetPreferredFocusWidget())
			{
				PreferredFocus = ContentPreferredFocus;
			}
		}
		return FrontendModuleContentInstance
			&& FrontendModuleContentModule == EHSRFrontendModule::Inventory
			&& ApplyFocusBackend(PlayerController, PreferredFocus,
				FrontendModuleContentInstance) != EHSRFocusApplyResult::Unavailable;
	}
	case EHSRFrontendModule::PauseHub:
		return FrontendShellInstance
			&& ApplyFocusBackend(PlayerController, FrontendShellInstance->GetPreferredFocusWidget(),
				FrontendShellInstance) != EHSRFocusApplyResult::Unavailable;
	case EHSRFrontendModule::None:
	default:
		return false;
	}
}

// 分配前端路由请求 Token：与屏幕栈 Token 同理，保持单调递增且不落后于路由已处理区间
int64 UHSRUIManagerSubsystem::AllocateFrontendRequestToken()
{
	if (FrontendRouter)
		NextFrontendRequestToken = FMath::Max(NextFrontendRequestToken,
			FrontendRouter->GetSnapshot().LastProcessedRequestToken + 1);
	return NextFrontendRequestToken++;
}

// 提交前端路由请求：先让自动化后端有机会模拟失败，再转发给真实路由
EHSRFrontendRouteResult UHSRUIManagerSubsystem::SubmitFrontendRoute(const FHSRFrontendRouteRequest& Request)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend && bAutomationFailNextRouteSubmit)
	{
		bAutomationFailNextRouteSubmit = false;
		return EHSRFrontendRouteResult::InvalidRequest;
	}
#endif
	return FrontendRouter ? FrontendRouter->Submit(Request) : EHSRFrontendRouteResult::InvalidRequest;
}

// 补偿结果解析：若补偿成功则返回原始失败原因；否则标记不可恢复的不一致
EHSRUIScreenResult UHSRUIManagerSubsystem::ResolveCompensation(const bool bRecovered,
	const EHSRUIScreenResult OriginalFailure)
{
	if (bRecovered)
	{
		return OriginalFailure;
	}
	// A failed compensation left the backend in an unknown state; never auto-clear this.
	// 补偿失败说明后端进入了未知状态，绝不能自动清除该不一致标记
	bInconsistent = true;
	bInconsistencyIsTravelRecoverable = false;
	return EHSRUIScreenResult::CompensationFailed;
}

// 构造"推入探索根"的屏幕请求
FHSRScreenRequest UHSRUIManagerSubsystem::MakeRootRequest(const int64 Token) const
{
	FHSRScreenRequest Request;
	Request.RequestToken = Token;
	Request.ScreenId = ExplorationRootId;
	Request.Layer = EHSRUIScreenLayer::HUD;
	Request.InputIntent = EHSRUIInputIntent::GameOnly;
	return Request;
}

// 构造"推入暂停屏"的屏幕请求（Modal 层、UI 独占、携带焦点 Token）
FHSRScreenRequest UHSRUIManagerSubsystem::MakePauseRequest(const int64 Token) const
{
	FHSRScreenRequest Request;
	Request.RequestToken = Token;
	Request.ScreenId = PauseScreenId;
	Request.Layer = EHSRUIScreenLayer::Modal;
	Request.InputIntent = EHSRUIInputIntent::UIOnly;
	Request.FocusToken = PauseFocusToken;
	return Request;
}

// 构造"弹出栈顶"的屏幕请求
FHSRScreenRequest UHSRUIManagerSubsystem::MakePopRequest(const int64 Token) const
{
	FHSRScreenRequest Request;
	Request.RequestToken = Token;
	Request.Operation = EHSRScreenStackOperation::Pop;
	return Request;
}

// 补偿式弹出：用于暂停屏打开过程中途失败时，移除候选 Widget、弹出已推入的屏幕、恢复策略。
// 返回 false 时会把整个子系统标记为不一致。
bool UHSRUIManagerSubsystem::CompensatePop(const FHSRInputModePolicy& RestorePolicy,
	AHSRPlayerController* PlayerController, UHSRScreenWidget* CandidateWidget)
{
	if (CandidateWidget)
	{
		CandidateWidget->RemoveFromParent();
	}
	const EHSRScreenStackResult PopResult = ScreenStack->SubmitRequest(MakePopRequest(AllocateRequestToken()));
	const bool bPolicyRestored = ApplyPolicyBackend(PlayerController, RestorePolicy,
		EHSRPlayerControlMode::Exploration);
	if (PopResult != EHSRScreenStackResult::Success || !bPolicyRestored)
	{
		bInconsistent = true;
		return false;
	}
	return true;
}

// 补偿式暂停推入：失败路径反向用——把暂停屏重新推回栈并恢复 UI 策略
bool UHSRUIManagerSubsystem::CompensatePausePush(AHSRPlayerController* PlayerController)
{
	const EHSRScreenStackResult PushResult = ScreenStack->SubmitRequest(MakePauseRequest(AllocateRequestToken()));
	const bool bPolicyRestored = ApplyPolicyBackend(PlayerController, GetResolvedInputPolicy(),
		EHSRPlayerControlMode::UIOnly);
	if (PushResult != EHSRScreenStackResult::Success || !bPolicyRestored)
	{
		bInconsistent = true;
		return false;
	}
	return true;
}

// 释放背包候选对（Widget + ViewModel）：解绑、移除、Shutdown，并把引用置空
void UHSRUIManagerSubsystem::ReleaseInventoryCandidates(UHSRInventoryWidget*& Widget,
	UHSRInventoryRewardViewModel*& ViewModel)
{
	if (Widget)
	{
		Widget->SetViewModel(nullptr);
#if WITH_DEV_AUTOMATION_TESTS
		LastReleasedInventoryBindCount = Widget->GetBindCountForAutomation();
		LastReleasedInventoryUnbindCount = Widget->GetUnbindCountForAutomation();
#endif
		Widget->RemoveFromParent();
		Widget = nullptr;
	}
	ShutdownInventoryViewModelCandidate(ViewModel);
}

// 关闭背包 ViewModel 候选并置空引用（自动化下计数关闭次数）
void UHSRUIManagerSubsystem::ShutdownInventoryViewModelCandidate(UHSRInventoryRewardViewModel*& ViewModel)
{
	if (!ViewModel)
	{
		return;
	}
	ViewModel->Shutdown();
	ViewModel = nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	++InventoryCandidateShutdownCount;
#endif
}

// 检测"背包持有不一致"：旧式背包的 Widget 与 ViewModel 必须成对存在；
// 模块化背包声明了 Inventory 模块但内容实例为空也视为不一致。
// 该状态是生命周期未收尾的强信号，多个入口用它提前拦截。
bool UHSRUIManagerSubsystem::HasInventoryOwnershipMismatch() const
{
	const bool bLegacyMismatch = (InventoryWidgetInstance != nullptr)
		!= (InventoryViewModelInstance != nullptr);
	const bool bDynamicInventoryMissingContent = FrontendModuleContentModule
		== EHSRFrontendModule::Inventory && FrontendModuleContentInstance == nullptr;
	return bLegacyMismatch || bDynamicInventoryMissingContent;
}

// 清空所有宿主引用与前端实例指针（退役宿主时的统一清理入口）
void UHSRUIManagerSubsystem::ClearHostReferences()
{
	RegisteredHUD.Reset();
	RegisteredPlayerController.Reset();
	RegisteredRootWidget.Reset();
	FrontendShellClass = nullptr;
	FrontendModuleRootClass = nullptr;
	CharacterDetailWidgetClass = nullptr;
	InventoryWidgetClass = nullptr;
	InventoryModuleWidgetClass = nullptr;
	DialogueOverlayWidgetClass = nullptr;
	FrontendModuleWidgetClasses.Reset();
	FrontendModuleContentInstance = nullptr;
	FrontendModuleContentModule = EHSRFrontendModule::None;
	ActiveHostGeneration = 0;
}

// 解析背包命令上下文的角色 GUID：优先当前活跃队员槽（1-4 切换），
// 槽位无效/为空时回退到队长（槽 0）。结果用于模块化背包内容的命令上下文。
FGuid UHSRUIManagerSubsystem::ResolveInventoryCharacterGuid() const
{
	UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	FHSRPartySnapshot PartySnapshot;
	if (!Party || !Party->GetSnapshot(PartySnapshot) || PartySnapshot.Slots.IsEmpty())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("HSRUI P17 Inventory command context unavailable: Party has no slots"));
		return FGuid();
	}
	// Inventory equips the actively-controlled party member (the slot switched to with 1-4),
	// falling back to the leader when the active slot is unset or empty.
	int32 TargetSlot = PartySnapshot.ActiveSlot;
	if (TargetSlot < 0 || TargetSlot >= PartySnapshot.Slots.Num() || PartySnapshot.Slots[TargetSlot].IsEmpty())
	{
		TargetSlot = 0;
	}
	if (TargetSlot >= PartySnapshot.Slots.Num() || PartySnapshot.Slots[TargetSlot].IsEmpty())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("HSRUI P17 Inventory command context unavailable: Party has no committed member"));
		return FGuid();
	}
	const FGuid Result = HSRCharacterGuidFromProfileName(PartySnapshot.Slots[TargetSlot].CharacterId);
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory target ActiveSlot=%d CharacterId=%s"),
		TargetSlot, *PartySnapshot.Slots[TargetSlot].CharacterId.ToString());
	return Result;
}

// 选择可恢复的屏幕 ID：旅行前捕获，旅行到达后据此重开对应前端模块。
// 仅当栈健康（根+一层）且实例状态匹配时才返回具体屏幕；否则返回 NAME_None（无需恢复）。
FName UHSRUIManagerSubsystem::SelectRestorableScreenId() const
{
	if (!ScreenStack || bInconsistent || HasInventoryOwnershipMismatch())
	{
		return NAME_None;
	}
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	if (Snapshot.Entries.Num() < 2 || Snapshot.Entries[0].ScreenId != ExplorationRootId)
	{
		return NAME_None;
	}
	const EHSRFrontendModule ActiveModule = FrontendRouter
		? FrontendRouter->GetSnapshot().GetActiveRoute().Module : EHSRFrontendModule::None;
	// 角色详情：活跃模块为 Character 且详情实例存在、无背包
	if (ActiveModule == EHSRFrontendModule::Character && CharacterDetailWidgetInstance
		&& !InventoryWidgetInstance && !InventoryViewModelInstance)
	{
		return CharacterDetailScreenId;
	}
	// 背包：活跃模块为 Inventory 且（旧式或模块化背包）开着、无角色详情
	const bool bLegacyInventoryOpen = InventoryWidgetInstance && InventoryViewModelInstance;
	const bool bDynamicInventoryOpen = FrontendModuleContentModule == EHSRFrontendModule::Inventory
		&& FrontendModuleContentInstance != nullptr;
	if (ActiveModule == EHSRFrontendModule::Inventory
		&& (bLegacyInventoryOpen || bDynamicInventoryOpen) && !CharacterDetailWidgetInstance)
	{
		return InventoryScreenId;
	}
	return NAME_None;
}

// 是否已回到"干净的探索根"：栈恰好只有探索根、无任何前端实例、无背包持有不一致。
// 用于评估损坏是否被限制在退役宿主内（决定不一致是否可随新宿主恢复）。
bool UHSRUIManagerSubsystem::IsAtCleanExplorationRoot() const
{
	if (!ScreenStack || HasInventoryOwnershipMismatch())
	{
		return false;
	}
	if (FrontendShellInstance || FrontendModuleRootInstance || FrontendModuleContentInstance
		|| FrontendModuleContentModule != EHSRFrontendModule::None || CharacterDetailWidgetInstance
		|| InventoryWidgetInstance || InventoryViewModelInstance)
	{
		return false;
	}
	const FHSRScreenStackSnapshot Snapshot = ScreenStack->GetSnapshot();
	return Snapshot.Entries.Num() == 1 && Snapshot.Entries[0].ScreenId == ExplorationRootId;
}

// 尝试清除"可随旅行恢复的不一致"：只有当前有存活宿主、后端宿主有效且栈回到干净根时，
// 才能证明 UI 已重新自洽，此时清除不一致标记。否则保持原状等待新宿主再次尝试。
void UHSRUIManagerSubsystem::TryClearRecoverableInconsistency()
{
	if (!bInconsistent || !bInconsistencyIsTravelRecoverable)
	{
		return;
	}
	// Only a live registered host plus an exact, unowned root proves the UI is coherent again.
	// 只有"存活的已注册宿主 + 恰好一个无人持有的探索根"才能证明 UI 重新自洽
	AHSRPlayerController* PC = RegisteredPlayerController.Get();
	if (ActiveHostGeneration == 0
		|| !IsBackendHostValid(PC, RegisteredRootWidget.Get(), PC ? PC->GetWorld() : nullptr)
		|| !IsAtCleanExplorationRoot())
	{
		return;
	}
	bInconsistent = false;
	bInconsistencyIsTravelRecoverable = false;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inconsistency cleared by fresh host HostGeneration=%lld Stack=%d"),
		ActiveHostGeneration, GetLogicalScreenCount());
}

// 捕获并拆除旅行宿主：旅行开始前调用。
// 1) 先捕获"可恢复屏幕 ID"与宿主代数（此时旧宿主仍存活，实例可观测）；
// 2) 记录地图到达代数的基线（之后的新到达才视为本次旅行的落点）；
// 3) 拆除当前宿主，必要时强制把屏幕栈弹回根；
// 4) 记录旅行恢复描述符，等待新宿主注册 + 地图到达后消费。
EHSRUIScreenResult UHSRUIManagerSubsystem::CaptureAndTeardownTravelHost()
{
	const int64 CapturedHost = ActiveHostGeneration;
	// Capture ownership while the old host is still alive; teardown releases the
	// widget/view-model pair that SelectRestorableScreenId validates.
	// 在旧宿主仍存活时捕获可恢复屏幕 ID；拆除会释放它校验所依赖的 Widget/ViewModel 对
	const FName Restorable = SelectRestorableScreenId();
	AHSRPlayerController* CapturedPC = RegisteredPlayerController.Get();

	// 记录到达代数基线：本次旅行捕获时刻之前的所有到达都视为历史到达
	int64 ArrivalBaseline = LastObservedArrivalCommitGeneration;
	if (UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr)
	{
		if (const UHSRMapSubsystem* Maps = GameInstance->GetSubsystem<UHSRMapSubsystem>())
		{
			ArrivalBaseline = FMath::Max(ArrivalBaseline, Maps->GetArrivalCommitGeneration());
		}
	}

	EHSRUIScreenResult Result = TeardownCurrentHost(true);

	// 拆除后若屏幕栈仍有残留屏幕（>1），强制弹回根并标记需要强制清理
	bool bForcedRootCleanup = false;
	while (ScreenStack && ScreenStack->GetSnapshot().Entries.Num() > 1)
	{
		bForcedRootCleanup = true;
		if (ScreenStack->SubmitRequest(MakePopRequest(AllocateRequestToken())) != EHSRScreenStackResult::Success)
		{
			Result = EHSRUIScreenResult::Inconsistent;
			break;
		}
	}

	// 验证拆除后栈是否精确落在探索根
	const FHSRScreenStackSnapshot PostTeardown = ScreenStack ? ScreenStack->GetSnapshot() : FHSRScreenStackSnapshot{};
	const bool bAtExactRoot = PostTeardown.Entries.Num() == 1
		&& PostTeardown.Entries[0].ScreenId == ExplorationRootId;
	if (!bAtExactRoot || (bForcedRootCleanup
		&& !ApplyPolicyBackend(CapturedPC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration)))
	{
		Result = EHSRUIScreenResult::Inconsistent;
	}

	// 若发生了强制栈清理，标记不一致；但只要落在精确根上就是可恢复的（新宿主会再校验）
	if (bForcedRootCleanup)
	{
		bInconsistent = true;
		// Travel cleanup that still lands on an exact root is recoverable: the next host
		// registration re-validates the stack and clears the flag. Anything else is real corruption.
		// 旅行清理若仍落在精确根则是可恢复的：下一次宿主注册会重新校验栈并清除标记；
		// 否则就是真正的损坏
		bInconsistencyIsTravelRecoverable = bAtExactRoot;
		Result = EHSRUIScreenResult::Inconsistent;
		UE_LOG(LogTemp, Warning, TEXT("HSRUI P17 TravelFreeze forced non-owned stack cleanup to root Recoverable=%s"),
			bInconsistencyIsTravelRecoverable ? TEXT("true") : TEXT("false"));
	}

	// 记录旅行恢复描述符：新宿主注册 + 到达代数达标后消费
	TravelRestoreGeneration = NextTravelRestoreGeneration++;
	TravelCapturedHostGeneration = CapturedHost;
	MinimumArrivalCommitGeneration = ArrivalBaseline + 1;
	TravelRestoreScreenId = Result == EHSRUIScreenResult::Success ? Restorable : NAME_None;
	bTravelRestorePending = true;
	bTravelArrivalObserved = false;
	LatchedArrivalCommitGeneration = 0;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 TravelFreeze Generation=%lld HostGeneration=%lld Screen=%s MinArrival=%lld Teardown=%d"),
		TravelRestoreGeneration, TravelCapturedHostGeneration, *TravelRestoreScreenId.ToString(),
		MinimumArrivalCommitGeneration, static_cast<int32>(Result));
	return Result;
}

// 地图到达提交回调（UHSRMapSubsystem 的 OnArrivalCommitted 订阅）：
// 1) 始终推进"最后观察到的到达代数"；
// 2) 若正处于旅行恢复等待期，且本次到达代数 >= 恢复门槛，则锁存到达并尝试恢复。
void UHSRUIManagerSubsystem::HandleArrivalCommitted(const FHSRMapArrivalCommitInfo& Info)
{
	LastObservedArrivalCommitGeneration = FMath::Max(LastObservedArrivalCommitGeneration, Info.CommitGeneration);
	if (!bTravelRestorePending || Info.CommitGeneration < MinimumArrivalCommitGeneration)
	{
		return;
	}
	bTravelArrivalObserved = true;
	LatchedArrivalCommitGeneration = Info.CommitGeneration;
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 TravelArrival Generation=%lld ArrivalGeneration=%lld Map=%s Kind=%d HostGeneration=%lld"),
		TravelRestoreGeneration, Info.CommitGeneration, *Info.MapId.ToString(), static_cast<int32>(Info.Kind), ActiveHostGeneration);
	TryRestoreTravelDescriptor();
}

// 尝试消费旅行恢复描述符：只有满足全部条件（等待中、已观察到到达、到达代数达标、
// 新宿主代数已超过捕获代数、后端宿主有效）才真正重开之前的前端模块。
// 重开成功且恢复了模块时置 bTravelRestoredModule，使返回键行为改为"关到根"。
void UHSRUIManagerSubsystem::TryRestoreTravelDescriptor()
{
	AHSRPlayerController* RegisteredPC = RegisteredPlayerController.Get();
	UHSRUserWidget* RegisteredRoot = RegisteredRootWidget.Get();
	UWorld* RegisteredWorld = RegisteredPC ? RegisteredPC->GetWorld() : nullptr;
	if (!bTravelRestorePending || !bTravelArrivalObserved
		|| LatchedArrivalCommitGeneration < MinimumArrivalCommitGeneration
		|| ActiveHostGeneration <= TravelCapturedHostGeneration
		|| !IsBackendHostValid(RegisteredPC, RegisteredRoot, RegisteredWorld))
	{
		return;
	}

	// 本地保存描述符字段后立即清空，避免重入
	const int64 DescriptorGeneration = TravelRestoreGeneration;
	const FName ScreenId = TravelRestoreScreenId;
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
	LatchedArrivalCommitGeneration = 0;
	TravelRestoreScreenId = NAME_None;

	// 按捕获到的屏幕 ID 重开对应前端模块；无屏幕 ID 时只需恢复探索输入策略与焦点
	EHSRUIScreenResult Result = EHSRUIScreenResult::Success;
	if (!ScreenId.IsNone())
	{
		if (ScreenId == CharacterDetailScreenId)
		{
			Result = OpenFrontendModule(EHSRFrontendModule::Character);
		}
		else if (ScreenId == InventoryScreenId)
		{
			Result = OpenFrontendModule(EHSRFrontendModule::Inventory);
		}
		else
		{
			// 捕获了未知屏幕 ID：数据损坏，标记不一致
			Result = EHSRUIScreenResult::Inconsistent;
		}
	}
	else
	{
		AHSRPlayerController* PC = RegisteredPlayerController.Get();
		Result = ApplyPolicyBackend(PC, GetResolvedInputPolicy(), EHSRPlayerControlMode::Exploration)
			? EHSRUIScreenResult::Success : EHSRUIScreenResult::PolicyApplyFailed;
		ApplyFocusBackend(PC, RegisteredRootWidget.Get(), RegisteredRootWidget.Get());
	}

	// 恢复的模块在返回时没有下层外壳可回退，需标记让 RequestBack 关到根
	bTravelRestoredModule = Result == EHSRUIScreenResult::Success && !ScreenId.IsNone();
	if (Result == EHSRUIScreenResult::CompensationFailed || Result == EHSRUIScreenResult::Inconsistent)
	{
		bInconsistent = true;
	}
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 TravelRestore Consume Generation=%lld NewHostGeneration=%lld Screen=%s Result=%d Stack=%d"),
		DescriptorGeneration, ActiveHostGeneration, *ScreenId.ToString(), static_cast<int32>(Result), GetLogicalScreenCount());
}

// 后端宿主有效性判断（自动化后端用标志位模拟）
bool UHSRUIManagerSubsystem::IsBackendHostValid(AHSRPlayerController* PlayerController,
	UHSRUserWidget* RootWidget, UWorld* World) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationHostValid;
	}
#endif
	return RegisteredHUD.IsValid() && PlayerController && PlayerController->IsLocalPlayerController() && RootWidget && World;
}

// 后端是否处于探索模式（自动化后端用标志位模拟）
bool UHSRUIManagerSubsystem::IsBackendExploration(AHSRPlayerController* PlayerController) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationExploration;
	}
#endif
	return PlayerController && PlayerController->GetControlMode() == EHSRPlayerControlMode::Exploration;
}

bool UHSRUIManagerSubsystem::IsBackendPaused(UWorld* World) const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationPaused;
	}
#endif
	return World && World->IsPaused();
}

// 是否有模块根容器类（自动化后端恒为 true）
bool UHSRUIManagerSubsystem::HasModuleRootClass() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return true;
	}
#endif
	return FrontendModuleRootClass != nullptr;
}

// 是否有角色详情 Widget 类（自动化后端用标志位）
bool UHSRUIManagerSubsystem::HasCharacterDetailClass() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationHasDetailClass;
	}
#endif
	return CharacterDetailWidgetClass != nullptr;
}

// 是否有背包 Widget 类（自动化后端用标志位）
bool UHSRUIManagerSubsystem::HasInventoryClass() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationHasInventoryClass;
	}
#endif
	return InventoryWidgetClass != nullptr;
}

// 是否有跨地图旅行挂起：地图子系统有挂起旅行，或战斗转换子系统有进/出战斗挂起
bool UHSRUIManagerSubsystem::IsTravelPending() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return false;
	}
#endif
	UGameInstance* GameInstance = GetLocalPlayer() ? GetLocalPlayer()->GetGameInstance() : nullptr;
	const UHSRMapSubsystem* Maps = GameInstance ? GameInstance->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	const UHSRBattleTransitionSubsystem* Battle = GameInstance ? GameInstance->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
	return (Maps && Maps->HasPendingTravel()) || (Battle && (Battle->HasPending() || Battle->HasReturnPending()));
}

// 创建暂停外壳候选（自动化后端用 NewObject + 标志位模拟创建结果）
UHSRFrontendShellWidget* UHSRUIManagerSubsystem::CreatePauseCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationCreateSucceeds ? NewObject<UHSRFrontendShellWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRFrontendShellWidget>(PlayerController, FrontendShellClass);
}

// 创建模块根容器候选
UHSRFrontendModuleRootWidget* UHSRUIManagerSubsystem::CreateFrontendModuleRootCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationCreateSucceeds ? NewObject<UHSRFrontendModuleRootWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRFrontendModuleRootWidget>(PlayerController, FrontendModuleRootClass);
}

// 创建角色详情 Widget 候选
UHSRScreenWidget* UHSRUIManagerSubsystem::CreateCharacterDetailCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationDetailCreateSucceeds ? NewObject<UHSRCharacterDetailWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRScreenWidget>(PlayerController, CharacterDetailWidgetClass);
}

// 创建背包 Widget 候选
UHSRInventoryWidget* UHSRUIManagerSubsystem::CreateInventoryCandidate(AHSRPlayerController* PlayerController)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationInventoryCreateSucceeds ? NewObject<UHSRInventoryWidget>(this) : nullptr;
	}
#endif
	return CreateWidget<UHSRInventoryWidget>(PlayerController, InventoryWidgetClass);
}

// 创建背包 ViewModel 候选
UHSRInventoryRewardViewModel* UHSRUIManagerSubsystem::CreateInventoryViewModelCandidate()
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend && !bAutomationInventoryViewModelSucceeds)
	{
		return nullptr;
	}
#endif
	return NewObject<UHSRInventoryRewardViewModel>(this);
}

// 挂载暂停外壳候选到视口（100 层）
bool UHSRUIManagerSubsystem::AttachPauseCandidate(UHSRFrontendShellWidget* Candidate)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(100);
	return Candidate->IsInViewport();
}

// 挂载模块根容器候选到视口（110 层，高于外壳）
bool UHSRUIManagerSubsystem::AttachFrontendModuleRootCandidate(UHSRFrontendModuleRootWidget* Candidate)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return bAutomationAttachSucceeds;
	}
#endif
	Candidate->AddToViewport(110);
	return Candidate->IsInViewport();
}

// 取前端模块的 Widget 类。Inventory 单独走命名成员而非 map 项：
// 因为背包路径需要具体类型 UHSRInventoryModuleWidget 做内容创建，拆成 UUserWidget 统一查表会丢类型
TSubclassOf<UUserWidget> UHSRUIManagerSubsystem::GetFrontendModuleWidgetClass(const EHSRFrontendModule Module) const
{
	// Inventory stays a named field rather than a map entry: it is typed to the concrete
	// UHSRInventoryModuleWidget because the inventory path needs that type for its own content
	// creation, and widening it to UUserWidget just to unify the lookup would lose that.
	if (Module == EHSRFrontendModule::Inventory)
	{
		return InventoryModuleWidgetClass;
	}
	const TSubclassOf<UUserWidget>* Found = FrontendModuleWidgetClasses.Find(Module);
	return Found ? *Found : nullptr;
}

// 创建前端模块内容候选 Widget（自动化后端模拟背包/通用模块的创建结果）
UUserWidget* UHSRUIManagerSubsystem::CreateFrontendModuleContentCandidate(
	AHSRPlayerController* PlayerController, const EHSRFrontendModule Module)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (Module == EHSRFrontendModule::Inventory && bAutomationUseInventoryModuleContent)
		{
			return bAutomationInventoryModuleCreateSucceeds
				? NewObject<UHSRInventoryModuleWidget>(this) : nullptr;
		}
		return bAutomationFrontendModuleCreateSucceeds ? NewObject<UHSRUserWidget>(this) : nullptr;
	}
#endif
	const TSubclassOf<UUserWidget> WidgetClass = GetFrontendModuleWidgetClass(Module);
	return WidgetClass ? CreateWidget<UUserWidget>(PlayerController, WidgetClass) : nullptr;
}

// 把内容 Widget 塞进模块根容器
bool UHSRUIManagerSubsystem::AttachFrontendModuleContentCandidate(
	UHSRFrontendModuleRootWidget* RootCandidate, UUserWidget* ContentCandidate)
{
	if (!RootCandidate || !ContentCandidate)
	{
		return false;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		return Cast<UHSRInventoryModuleWidget>(ContentCandidate)
			&& bAutomationUseInventoryModuleContent
			? bAutomationInventoryModuleAttachSucceeds : bAutomationFrontendModuleAttachSucceeds;
	}
#endif
	return RootCandidate->SetModuleContent(ContentCandidate);
}

// 释放前端模块内容：清空根容器内容、移除内容 Widget、重置模块归属
void UHSRUIManagerSubsystem::ReleaseFrontendModuleContent()
{
	if (FrontendModuleRootInstance)
	{
		FrontendModuleRootInstance->ClearModuleContent();
	}
	if (FrontendModuleContentInstance)
	{
		FrontendModuleContentInstance->RemoveFromParent();
		FrontendModuleContentInstance = nullptr;
	}
	FrontendModuleContentModule = EHSRFrontendModule::None;
}

// 通用输入策略应用后端：转发给 InputModeCoordinator
// （自动化后端支持"第 N 次失败"与"下一次失败"两种注入方式）
bool UHSRUIManagerSubsystem::ApplyPolicyBackend(AHSRPlayerController* PlayerController,
	const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		// 支持"剩余 N 次调用后失败"与"仅下一次失败"两种故障注入
		if (AutomationPolicyCallsUntilFailure > 0 && --AutomationPolicyCallsUntilFailure == 0)
		{
			return false;
		}
		if (bAutomationFailNextPolicyApply)
		{
			bAutomationFailNextPolicyApply = false;
			return false;
		}
		return bAutomationPolicySucceeds;
	}
#endif
	return InputModeCoordinator->ApplyPolicy(PlayerController, Policy, SemanticMode);
}

// 世界暂停应用后端：实际调用 UGameplayStatics::SetGamePaused
bool UHSRUIManagerSubsystem::ApplyPauseBackend(UWorld* World, const bool bPaused)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (AutomationPauseCallsUntilFailure > 0 && --AutomationPauseCallsUntilFailure == 0)
		{
			return false;
		}
		if (bPaused && bAutomationFailPauseRestore)
		{
			bAutomationFailPauseRestore = false;
			return false;
		}
		if (bAutomationFailNextPauseApply)
		{
			bAutomationFailNextPauseApply = false;
			return false;
		}
		if (!bAutomationPauseSucceeds)
		{
			return false;
		}
		bAutomationPaused = bPaused;
		return true;
	}
#endif
	return World && UGameplayStatics::SetGamePaused(World, bPaused);
}

// 角色详情输入策略应用后端（自动化后端可注入一次失败）
bool UHSRUIManagerSubsystem::ApplyCharacterDetailPolicyBackend(AHSRPlayerController* PlayerController,
	const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailNextDetailPolicyApply)
		{
			bAutomationFailNextDetailPolicyApply = false;
			return false;
		}
		return bAutomationDetailPolicySucceeds;
	}
#endif
	return InputModeCoordinator->ApplyPolicy(PlayerController, Policy, SemanticMode);
}

// 背包输入策略应用后端（自动化后端可注入一次失败）
bool UHSRUIManagerSubsystem::ApplyInventoryPolicyBackend(AHSRPlayerController* PlayerController,
	const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		if (bAutomationFailNextInventoryPolicyApply)
		{
			bAutomationFailNextInventoryPolicyApply = false;
			return false;
		}
		return bAutomationInventoryPolicySucceeds;
	}
#endif
	return InputModeCoordinator->ApplyPolicy(PlayerController, Policy, SemanticMode);
}

// 通用焦点应用后端（自动化后端模拟失败/成功）
EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyFocusBackend(AHSRPlayerController* PlayerController,
	UWidget* Preferred, UWidget* Fallback)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		// 模拟"下一次焦点必然失败"（测试失败补偿路径）
		if (bAutomationFailNextFocusApply)
		{
			bAutomationFailNextFocusApply = false;
			return EHSRFocusApplyResult::Unavailable;
		}
		LastAutomationFocusModule = Preferred == FrontendShellInstance ? EHSRFrontendModule::PauseHub : EHSRFrontendModule::None;
		return bAutomationFocusSucceeds ? EHSRFocusApplyResult::Preferred : EHSRFocusApplyResult::Unavailable;
	}
#endif
	return InputModeCoordinator->ApplyFocus(PlayerController, Preferred, Fallback);
}

// 角色详情焦点应用后端（自动化后端可区分打开/关闭方向并独立失败）
EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyCharacterDetailFocusBackend(AHSRPlayerController* PlayerController,
	UWidget* Preferred, UWidget* Fallback)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		// 模拟"恢复旧模块焦点失败"：当焦点目标是角色详情 Widget 时触发一次
		if (bAutomationFailOldModuleFocusRestore && (Preferred == CharacterDetailWidgetInstance || Fallback == CharacterDetailWidgetInstance))
		{
			bAutomationFailOldModuleFocusRestore = false;
			return EHSRFocusApplyResult::Unavailable;
		}
		// Focusing the shell rather than the detail widget means this is the close direction, which
		// tests fail independently of the open direction.
		// 焦点目标是外壳而非详情 Widget 时说明这是"关闭方向"，测试会独立于打开方向让其失败
		const bool bIsCloseFocus = FrontendShellInstance && Preferred == FrontendShellInstance;
		if (bIsCloseFocus && !bAutomationDetailCloseFocusSucceeds)
		{
			return EHSRFocusApplyResult::Unavailable;
		}
		LastAutomationFocusModule = (Preferred == CharacterDetailWidgetInstance || Fallback == CharacterDetailWidgetInstance) ? EHSRFrontendModule::Character : EHSRFrontendModule::PauseHub;
		return bAutomationDetailFocusSucceeds ? EHSRFocusApplyResult::Preferred : EHSRFocusApplyResult::Unavailable;
	}
#endif
	return InputModeCoordinator->ApplyFocus(PlayerController, Preferred, Fallback);
}

// 背包焦点应用后端（自动化后端与角色详情对称）
EHSRFocusApplyResult UHSRUIManagerSubsystem::ApplyInventoryFocusBackend(AHSRPlayerController* PlayerController,
	UWidget* Preferred, UWidget* Fallback)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bUseAutomationBackend)
	{
		// 模拟"恢复旧模块焦点失败"：焦点目标是背包 Widget 时触发一次
		if (bAutomationFailOldModuleFocusRestore && (Preferred == InventoryWidgetInstance || Fallback == InventoryWidgetInstance))
		{
			bAutomationFailOldModuleFocusRestore = false;
			return EHSRFocusApplyResult::Unavailable;
		}
		// Close direction, as in the detail twin above.
		// 关闭方向判定与角色详情一致
		const bool bIsCloseFocus = FrontendShellInstance && Preferred == FrontendShellInstance;
		if (bIsCloseFocus && !bAutomationInventoryCloseFocusSucceeds)
		{
			return EHSRFocusApplyResult::Unavailable;
		}
		LastAutomationFocusModule = (Preferred == InventoryWidgetInstance || Fallback == InventoryWidgetInstance) ? EHSRFrontendModule::Inventory : EHSRFrontendModule::PauseHub;
		return bAutomationInventoryFocusSucceeds ? EHSRFocusApplyResult::Preferred : EHSRFocusApplyResult::Unavailable;
	}
#endif
	return InputModeCoordinator->ApplyFocus(PlayerController, Preferred, Fallback);
}

#if WITH_DEV_AUTOMATION_TESTS
// 【自动化测试专用】初始化核心对象（幂等：已初始化则跳过）
void UHSRUIManagerSubsystem::InitializeForAutomation()
{
	if (!bInitialized)
	{
		ScreenStack = NewObject<UHSRScreenStack>(this);
		InputModeCoordinator = NewObject<UHSRInputModeCoordinator>(this);
		FrontendRouter = NewObject<UHSRFrontendRouter>(this);
		NextRequestToken = 1;
		NextFrontendRequestToken = 1;
		bInitialized = true;
		bInconsistent = false;
	}
}

// 【自动化测试专用】以固定宿主标识 1 注册宿主
void UHSRUIManagerSubsystem::RegisterHostForAutomation(const bool bInExplorationMode, const bool bHasPauseClass)
{
	RegisterHostIdentityForAutomation(1, bInExplorationMode, bHasPauseClass);
}

// 【自动化测试专用】按宿主标识注册宿主：同一标识重复注册为 NoOp（仅刷新模式标志），
// 已注册其他宿主或仍有存活实例则拒绝
EHSRUIScreenResult UHSRUIManagerSubsystem::RegisterHostIdentityForAutomation(const int32 HostIdentity,
	const bool bInExplorationMode, const bool bHasPauseClass)
{
	bUseAutomationBackend = true;
	// 宿主标识必须为正数
	if (HostIdentity <= 0)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	if (HasInventoryOwnershipMismatch())
	{
		bInconsistent = true;
		bInconsistencyIsTravelRecoverable = false;
		return EHSRUIScreenResult::Inconsistent;
	}
	// 同一标识重复注册：只更新模式标志，视为 NoOp
	if (AutomationHostIdentity == HostIdentity)
	{
		bAutomationExploration = bInExplorationMode;
		bAutomationHasPauseClass = bHasPauseClass;
		return EHSRUIScreenResult::NoOp;
	}
	// 已注册别的宿主或有实例存活时拒绝换宿主
	if (AutomationHostIdentity != 0 || FrontendShellInstance || FrontendModuleContentInstance
		|| CharacterDetailWidgetInstance || InventoryWidgetInstance || InventoryViewModelInstance
		|| DialogueOverlayWidgetInstance || DialogueViewModelInstance)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	// 空栈先推入探索根
	if (ScreenStack && ScreenStack->GetSnapshot().Entries.IsEmpty()
		&& ScreenStack->SubmitRequest(MakeRootRequest(AllocateRequestToken())) != EHSRScreenStackResult::Success)
	{
		return EHSRUIScreenResult::StackRejected;
	}
	// 记录宿主身份并分配新的宿主代数
	AutomationHostIdentity = HostIdentity;
	bAutomationHostValid = true;
	ActiveHostGeneration = NextHostGeneration++;
	bAutomationExploration = bInExplorationMode;
	bAutomationHasPauseClass = bHasPauseClass;
	bAutomationHasDetailClass = true;
	bAutomationHasInventoryClass = true;
	TryClearRecoverableInconsistency();
	TryRestoreTravelDescriptor();
	return EHSRUIScreenResult::Success;
}

// 【自动化测试专用】按宿主标识反注册宿主
EHSRUIScreenResult UHSRUIManagerSubsystem::UnregisterHostIdentityForAutomation(const int32 HostIdentity)
{
	if (HostIdentity <= 0 || AutomationHostIdentity != HostIdentity)
	{
		return EHSRUIScreenResult::InvalidHost;
	}
	return TeardownCurrentHost();
}

// 【自动化测试专用】按宿主标识拆除旅行宿主
EHSRUIScreenResult UHSRUIManagerSubsystem::TeardownHostIdentityForTravelForAutomation(const int32 HostIdentity)
{
	if (HostIdentity <= 0 || AutomationHostIdentity != HostIdentity || ActiveHostGeneration == 0)
		return EHSRUIScreenResult::InvalidHost;
	return CaptureAndTeardownTravelHost();
}

// 【自动化测试专用】模拟一次地图到达提交
void UHSRUIManagerSubsystem::NotifyArrivalCommittedForAutomation(const int64 CommitGeneration, const FName MapId)
{
	FHSRMapArrivalCommitInfo Info;
	Info.CommitGeneration = CommitGeneration;
	Info.MapId = MapId;
	Info.Kind = EHSRMapArrivalCommitKind::OrdinaryTravel;
	HandleArrivalCommitted(Info);
}

// 【自动化测试专用】配置通用后端（创建/挂载/策略/暂停/焦点成败 + 初始暂停态）
void UHSRUIManagerSubsystem::ConfigureAutomationBackend(const bool bCreateSucceeds, const bool bAttachSucceeds,
	const bool bPolicySucceeds, const bool bPauseSucceeds, const bool bFocusSucceeds, const bool bInitiallyPaused)
{
	bUseAutomationBackend = true;
	bAutomationCreateSucceeds = bCreateSucceeds;
	bAutomationAttachSucceeds = bAttachSucceeds;
	bAutomationPolicySucceeds = bPolicySucceeds;
	bAutomationPauseSucceeds = bPauseSucceeds;
	bAutomationFocusSucceeds = bFocusSucceeds;
	bAutomationPaused = bInitiallyPaused;
}

// 【自动化测试专用】配置共享模块根后端（是否有类/创建成败/挂载成败）
void UHSRUIManagerSubsystem::ConfigureAutomationFrontendModuleBackend(
	const bool bHasClass, const bool bCreateSucceeds, const bool bAttachSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasFrontendModuleClass = bHasClass;
	bAutomationFrontendModuleCreateSucceeds = bCreateSucceeds;
	bAutomationFrontendModuleAttachSucceeds = bAttachSucceeds;
}

// 【自动化测试专用】配置模块化背包后端（有类即使用模块内容）
void UHSRUIManagerSubsystem::ConfigureAutomationInventoryModuleBackend(
	const bool bHasClass, const bool bCreateSucceeds, const bool bAttachSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasFrontendModuleClass = bHasClass;
	bAutomationUseInventoryModuleContent = bHasClass;
	bAutomationInventoryModuleCreateSucceeds = bCreateSucceeds;
	bAutomationInventoryModuleAttachSucceeds = bAttachSucceeds;
}

// 【自动化测试专用】配置对话浮层后端
void UHSRUIManagerSubsystem::ConfigureAutomationDialogueOverlayBackend(
	const bool bHasClass, const bool bCreateSucceeds, const bool bAttachSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasDialogueOverlayClass = bHasClass;
	bAutomationDialogueOverlayCreateSucceeds = bCreateSucceeds;
	bAutomationDialogueOverlayAttachSucceeds = bAttachSucceeds;
}

// 【自动化测试专用】查询当前模块内容实例数量（0 或 1）
int32 UHSRUIManagerSubsystem::GetFrontendModuleContentCountForAutomation() const
{
	return FrontendModuleContentInstance ? 1 : 0;
}

// 【自动化测试专用】查询当前模块内容归属的模块
EHSRFrontendModule UHSRUIManagerSubsystem::GetFrontendModuleContentModuleForAutomation() const
{
	return FrontendModuleContentModule;
}

// 【自动化测试专用】配置角色详情后端
void UHSRUIManagerSubsystem::ConfigureAutomationDetailBackend(const bool bHasClass, const bool bCreateSucceeds,
	const bool bAttachSucceeds, const bool bPolicySucceeds, const bool bFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasDetailClass = bHasClass;
	bAutomationDetailCreateSucceeds = bCreateSucceeds;
	bAutomationDetailAttachSucceeds = bAttachSucceeds;
	bAutomationDetailPolicySucceeds = bPolicySucceeds;
	bAutomationDetailFocusSucceeds = bFocusSucceeds;
}

// 【自动化测试专用】配置角色详情"关闭方向"焦点成败
void UHSRUIManagerSubsystem::ConfigureAutomationDetailCloseFocus(const bool bCloseFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationDetailCloseFocusSucceeds = bCloseFocusSucceeds;
}

// 【自动化测试专用】配置背包"关闭方向"焦点成败
void UHSRUIManagerSubsystem::ConfigureAutomationInventoryCloseFocus(const bool bCloseFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationInventoryCloseFocusSucceeds = bCloseFocusSucceeds;
}

// 【自动化测试专用】配置旧式背包后端（含 ViewModel 阶段标志）
void UHSRUIManagerSubsystem::ConfigureAutomationInventoryBackend(const bool bHasClass, const bool bCreateSucceeds,
	const bool bViewModelSucceeds, const bool bAttachSucceeds, const bool bPolicySucceeds, const bool bFocusSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationHasInventoryClass = bHasClass;
	bAutomationInventoryCreateSucceeds = bCreateSucceeds;
	bAutomationInventoryViewModelSucceeds = bViewModelSucceeds;
	bAutomationInventoryDependenciesSucceed = bViewModelSucceeds;
	bAutomationInventorySnapshotSucceeds = bViewModelSucceeds;
	bAutomationInventoryAttachSucceeds = bAttachSucceeds;
	bAutomationInventoryPolicySucceeds = bPolicySucceeds;
	bAutomationInventoryFocusSucceeds = bFocusSucceeds;
}

// 【自动化测试专用】分别配置背包 ViewModel 的三个阶段（依赖/创建/快照）
void UHSRUIManagerSubsystem::ConfigureAutomationInventoryViewModelStages(const bool bDependenciesSucceed,
	const bool bCreateSucceeds, const bool bSnapshotSucceeds)
{
	bUseAutomationBackend = true;
	bAutomationInventoryDependenciesSucceed = bDependenciesSucceed;
	bAutomationInventoryViewModelSucceeds = bCreateSucceeds;
	bAutomationInventorySnapshotSucceeds = bSnapshotSucceeds;
}

// 【自动化测试专用】注入"只有一半"的背包持有（用于测试持有不一致检测）
void UHSRUIManagerSubsystem::InjectInventoryHalfPairForAutomation(const bool bWidgetOnly)
{
	bUseAutomationBackend = true;
	InventoryWidgetInstance = bWidgetOnly ? NewObject<UHSRInventoryWidget>(this) : nullptr;
	InventoryViewModelInstance = bWidgetOnly ? nullptr : NewObject<UHSRInventoryRewardViewModel>(this);
}

// 【自动化测试专用】查询背包 Widget 的绑定计数
int32 UHSRUIManagerSubsystem::GetInventoryBindCountForAutomation() const
{
	return InventoryWidgetInstance ? InventoryWidgetInstance->GetBindCountForAutomation() : 0;
}

// 【自动化测试专用】彻底清理自动化后端状态
void UHSRUIManagerSubsystem::DeinitializeForAutomation()
{
	ReleaseDialogueOverlay();
	FrontendShellInstance = nullptr;
	FrontendModuleRootInstance = nullptr;
	CharacterDetailWidgetInstance = nullptr;
	if (InventoryWidgetInstance)
	{
		InventoryWidgetInstance->SetViewModel(nullptr);
	}
	InventoryWidgetInstance = nullptr;
	if (InventoryViewModelInstance)
	{
		InventoryViewModelInstance->Shutdown();
	}
	InventoryViewModelInstance = nullptr;
	PauseOwnerToken.Invalidate();
	ClearHostReferences();
	InputModeCoordinator = nullptr;
	FrontendRouter = nullptr;
	ScreenStack = nullptr;
	bInitialized = false;
	bInconsistent = false;
	bInconsistencyIsTravelRecoverable = false;
	bUseAutomationBackend = false;
	bAutomationHostValid = false;
	bAutomationUseInventoryModuleContent = false;
	AutomationHostIdentity = 0;
	ActiveHostGeneration = 0;
	bTravelRestorePending = false;
	bTravelArrivalObserved = false;
}
#endif
