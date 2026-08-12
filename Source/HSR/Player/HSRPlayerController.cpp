#include "HSRPlayerController.h"
#include "../UI/HSRHUD.h"
#include "../Character/HSRExplorationCharacter.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "../UI/HSRScreenStackTypes.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRExplorationReturnConsumer.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Party/HSRPartyTypes.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "EngineUtils.h"
#include "Framework/Application/NavigationConfig.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"

AHSRPlayerController::AHSRPlayerController()
{
	// 玩家控制器的输入（包括 Enhanced Input 的动作求值）依赖控制器每帧的 player-input tick，
	// 因此这里必须允许 Tick（与普通 Actor 不同，不能关）。
	PrimaryActorTick.bCanEverTick = true;
	CurrentControlMode = EHSRPlayerControlMode::Exploration;
	bControlModeApplied = false;
	bExplorationContextAdded = false;
	bFrontendNavigationContextAdded = false;
	bInputSystemReady = false;
	AppliedInputIntent = EHSRUIInputIntent::GameOnly;
}

void AHSRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	bInputSystemReady = true;
	if (InputComponent)
	{
		// 绑定 1-4 数字键用于切换探索角色（对应队伍槽位）。
		InputComponent->Priority = FrontendInputPriority;
		InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ThisClass::HandlePartySlot1);
		InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ThisClass::HandlePartySlot2);
		InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ThisClass::HandlePartySlot3);
		InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ThisClass::HandlePartySlot4);
	}
	AddFrontendNavigationContext();
	// 为前端 UI 导航绑定增强输入动作（暂停/背包/队伍/地图/挑战/关闭到根）。
	if (UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
		Enhanced && ShouldBindFrontendInputComponent(FrontendBindingsInputComponent, InputComponent))
	{
		// 局部 lambda 封装"动作为空则告警、否则绑定"的重复逻辑，避免每个动作各写一遍。
		const auto BindStarted = [Enhanced, this](UInputAction* Action, void (AHSRPlayerController::*Handler)())
		{
			if (Action)
			{
				Enhanced->BindAction(Action, ETriggerEvent::Started, this, Handler);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend action is not configured; binding skipped"));
			}
		};
		BindStarted(PauseBackAction, &ThisClass::HandlePauseBack);
		BindStarted(InventoryAction, &ThisClass::HandleInventory);
		BindStarted(PartyAction, &ThisClass::HandleParty);
		BindStarted(MapAction, &ThisClass::HandleMap);
		BindStarted(ChallengeAction, &ThisClass::HandleChallenge);
		BindStarted(CloseToRootAction, &ThisClass::HandleCloseToRoot);
		FrontendBindingsInputComponent = InputComponent;
		// 记录暂停动作是否真的存在于前端导航上下文中（用于日志排查）。
		const bool bPauseMapped = FrontendNavigationMappingContext && PauseBackAction
			&& FrontendNavigationMappingContext->GetMappings().ContainsByPredicate([this](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Action == PauseBackAction;
			});
		if (FrontendNavigationMappingContext && PauseBackAction)
		{
			for (const FEnhancedActionKeyMapping& Mapping : FrontendNavigationMappingContext->GetMappings())
			{
				if (Mapping.Action == PauseBackAction)
				{
					UE_LOG(LogTemp, Log, TEXT("HSRUI Frontend PauseBack key=%s"), *Mapping.Key.ToString());
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("HSRUI Frontend input bound Component=%s Priority=%d Context=%s PauseAction=%s PauseMapped=%s"),
			*InputComponent->GetName(), InputComponent->Priority,
			FrontendNavigationMappingContext ? *FrontendNavigationMappingContext->GetPathName() : TEXT("None"),
			PauseBackAction ? *PauseBackAction->GetPathName() : TEXT("None"), bPauseMapped ? TEXT("true") : TEXT("false"));
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetupInputComponent - PlayerInput=%s InputComponent=%s"),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("None"),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	// 本地玩家且处于探索模式时，把探索输入上下文加入增强输入子系统。
	if (IsLocalPlayerController() && CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}
}

// BeginPlay：安装前端 Slate 导航、必要时补建战斗返回消费器，并按当前所在世界解析控制模式。
void AHSRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bControlModeApplied = false;
	InstallFrontendSlateNavigation();

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - Controller=%s Local=%s Pawn=%s"),
		*GetName(),
		IsLocalController() ? TEXT("true") : TEXT("false"),
		GetPawn() ? *GetPawn()->GetName() : TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - PlayerInput=%s InputComponent=%s"),
		PlayerInput ? *PlayerInput->GetClass()->GetName() : TEXT("None"),
		InputComponent ? *InputComponent->GetClass()->GetName() : TEXT("None"));

	// 战斗返回是 GameInstance 级事务。探索地图可能没有放置返回消费器，
	// 因此在地图 PlayerController 生命周期边界提供运行时兜底，且不重复该事务。
	if (UHSRBattleTransitionSubsystem* BattleTravel = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
		BattleTravel && BattleTravel->HasReturnPending() && GetWorld())
	{
		bool bConsumerPresent = false;
		for (TActorIterator<AHSRExplorationReturnConsumer> It(GetWorld()); It; ++It)
		{
			bConsumerPresent = true;
			break;
		}
		if (ShouldEnsureBattleReturnConsumer(true, bConsumerPresent))
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AHSRExplorationReturnConsumer* Consumer = GetWorld()->SpawnActor<AHSRExplorationReturnConsumer>(
				AHSRExplorationReturnConsumer::StaticClass(), FTransform::Identity, SpawnParameters))
			{
				UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::BeginPlay - Spawned battle return consumer %s"),
					*Consumer->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::BeginPlay - Failed to spawn battle return consumer"));
			}
		}
	}
	// 根据实际落入的世界选择控制模式。在此处（而非从战斗 GameMode）决定，
	// 完全避开 PlayerController 查找时机问题：在本 BeginPlay 内控制器就是 this，
	// 无需查找也无需延迟。
	SetControlMode(ResolveControlModeForCurrentWorld());
}

void AHSRPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 离开世界时恢复默认的 Slate 导航配置，避免污染下一个世界。
	RestoreFrontendSlateNavigation();
	Super::EndPlay(EndPlayReason);
}

// 关闭 Tab 导航（前端 UI 用方向键导航而非 Tab 焦点切换）。
void AHSRPlayerController::ConfigureFrontendNavigation(FNavigationConfig& NavigationConfig)
{
	NavigationConfig.bTabNavigation = false;
}

// 安装前端 Slate 导航：把用户的导航配置替换为前端专用配置（可恢复）。
void AHSRPlayerController::InstallFrontendSlateNavigation()
{
	if (!IsLocalPlayerController() || !FSlateApplication::IsInitialized() || FrontendSlateNavigationConfig)
	{
		return;
	}

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	FrontendSlateUserIndex = SlateApplication.GetUserIndexForKeyboard();
	TSharedPtr<FSlateUser> SlateUser = SlateApplication.GetUser(FrontendSlateUserIndex);
	if (!SlateUser)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI Slate navigation unavailable User=%d"), FrontendSlateUserIndex);
		FrontendSlateUserIndex = INDEX_NONE;
		return;
	}
	// 保存旧配置以便恢复，然后安装前端导航配置。
	PreviousSlateNavigationConfig = SlateUser->GetUserNavigationConfig();
	FrontendSlateNavigationConfig = MakeShared<FNavigationConfig>();
	ConfigureFrontendNavigation(*FrontendSlateNavigationConfig);
	SlateUser->SetUserNavigationConfig(FrontendSlateNavigationConfig);

	UE_LOG(LogTemp, Log, TEXT("HSRUI Slate navigation installed User=%d TabNavigation=%s"),
		FrontendSlateUserIndex, FrontendSlateNavigationConfig->bTabNavigation ? TEXT("true") : TEXT("false"));
}

// 恢复之前保存的 Slate 导航配置并清理临时状态（幂等）。
void AHSRPlayerController::RestoreFrontendSlateNavigation()
{
	if (FrontendSlateUserIndex != INDEX_NONE && FrontendSlateNavigationConfig && FSlateApplication::IsInitialized())
	{
		if (TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(FrontendSlateUserIndex);
			SlateUser && SlateUser->GetUserNavigationConfig() == FrontendSlateNavigationConfig)
		{
			SlateUser->SetUserNavigationConfig(PreviousSlateNavigationConfig);
			UE_LOG(LogTemp, Log, TEXT("HSRUI Slate navigation restored User=%d"), FrontendSlateUserIndex);
		}
	}

	FrontendSlateUserIndex = INDEX_NONE;
	PreviousSlateNavigationConfig.Reset();
	FrontendSlateNavigationConfig.Reset();
}

// 根据当前所在世界解析控制模式：战斗世界 -> Battle，否则 -> Exploration。
EHSRPlayerControlMode AHSRPlayerController::ResolveControlModeForCurrentWorld() const
{
	return Cast<AHSRBattleGameMode>(GetWorld() ? GetWorld()->GetAuthGameMode() : nullptr)
		? EHSRPlayerControlMode::Battle
		: EHSRPlayerControlMode::Exploration;
}

// 切换探索角色到指定队伍槽位：校验队伍/档案/角色类后生成新 Pawn 并 Possess。
// 任何一步失败都会回滚（重新 Possess 旧 Pawn），保证切换原子性。
bool AHSRPlayerController::SwitchExplorationCharacter(int32 PartySlot)
{
	// 只有探索控制模式才允许切换。
	if (CurrentControlMode != EHSRPlayerControlMode::Exploration || !GetWorld())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED Mode=%d World=%d"), static_cast<int32>(CurrentControlMode), GetWorld() ? 1 : 0);
		return false;
	}
	UGameInstance* GI = GetGameInstance();
	UHSRPartySubsystem* Party = GI ? GI->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSRCharacterProfileSubsystem* Profiles = GI ? GI->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	FHSRPartySnapshot PartySnapshot;
	// 目标槽位必须存在且有已提交角色。
	if (!Party || !Profiles || !Party->GetSnapshot(PartySnapshot) || !PartySnapshot.Slots.IsValidIndex(PartySlot)
		|| PartySnapshot.Slots[PartySlot].IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED PartyMissingOrSlotEmpty Slot=%d Party=%d Profiles=%d Slots=%d"),
			PartySlot, Party ? 1 : 0, Profiles ? 1 : 0, Party ? PartySnapshot.Slots.Num() : -1);
		return false;
	}
	// 从档案取角色定义，并解析出可生成的角色类。
	const FName CharacterId = PartySnapshot.Slots[PartySlot].CharacterId;
	const UHSRCharacterDefinition* Definition = nullptr;
	if (!Profiles->GetDefinition(CharacterId, Definition) || !Definition || Definition->CharacterClass.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED DefinitionMissing Char=%s"), *CharacterId.ToString());
		return false;
	}
	UClass* CharacterClass = Definition->CharacterClass.LoadSynchronous();
	if (!CharacterClass || !CharacterClass->IsChildOf<APawn>())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED ClassInvalid Char=%s Class=%s"), *CharacterId.ToString(), CharacterClass ? *CharacterClass->GetName() : TEXT("None"));
		return false;
	}
	// 在旧 Pawn 的位置生成新角色。
	APawn* PreviousPawn = GetPawn();
	const FTransform SpawnTransform = PreviousPawn ? PreviousPawn->GetActorTransform() : FTransform::Identity;
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(CharacterClass, SpawnTransform, SpawnParameters);
	if (!NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED SpawnFailed Char=%s Slot=%d"), *CharacterId.ToString(), PartySlot);
		return false;
	}
	// Possess 新角色并校验。
	Possess(NewPawn);
	if (GetPawn() != NewPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED PossessFailed Char=%s Slot=%d"), *CharacterId.ToString(), PartySlot);
		NewPawn->Destroy();
		return false;
	}
	// 同步队伍活动槽位；失败则回滚到旧 Pawn。
	if (Party->SetActiveSlot(PartySlot) != EHSRPartyResult::Success)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Exploration switch REJECTED SetActiveSlotFailed Slot=%d"), PartySlot);
		if (PreviousPawn)
		{
			Possess(PreviousPawn);
		}
		NewPawn->Destroy();
		return false;
	}
	// 销毁旧 Pawn（如果不同）。
	if (PreviousPawn && PreviousPawn != NewPawn)
	{
		PreviousPawn->Destroy();
	}
	UE_LOG(LogTemp, Log, TEXT("HSR Exploration character switched Slot=%d CharacterId=%s Pawn=%s"), PartySlot, *CharacterId.ToString(), *NewPawn->GetName());
	return true;
}

// 数字键 1-4：切换到对应队伍槽位的探索角色。
void AHSRPlayerController::HandlePartySlot1()
{
	SwitchExplorationCharacter(0);
}

void AHSRPlayerController::HandlePartySlot2()
{
	SwitchExplorationCharacter(1);
}

void AHSRPlayerController::HandlePartySlot3()
{
	SwitchExplorationCharacter(2);
}

void AHSRPlayerController::HandlePartySlot4()
{
	SwitchExplorationCharacter(3);
}

// Possess 新 Pawn：校验类型、按所在世界重新解析控制模式、必要时补探索输入上下文并刷新 HUD 交互观察。
void AHSRPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::OnPossess - InPawn is nullptr"));
		return;
	}

	AHSRExplorationCharacter* ExplorationChar = Cast<AHSRExplorationCharacter>(InPawn);
	if (!ExplorationChar)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::OnPossess - Possessed Pawn is not AHSRExplorationCharacter: %s"), *InPawn->GetName());
	}

	// 仅用于日志：确认探索角色的动作都在探索映射上下文中（排查输入失效问题）。
	if (ExplorationChar && ExplorationMappingContext)
	{
		for (const UInputAction* BoundAction : { ExplorationChar->GetMoveAction(), ExplorationChar->GetLookAction(),
			ExplorationChar->GetJumpAction(), ExplorationChar->GetInteractAction() })
		{
			const bool bFoundInContext = ExplorationMappingContext->GetMappings().ContainsByPredicate(
				[BoundAction](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Action == BoundAction; });
			UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::OnPossess - BoundAction=%s FoundInContext=%s"),
				BoundAction ? *BoundAction->GetPathName() : TEXT("None"),
				bFoundInContext ? TEXT("true") : TEXT("false"));
		}
	}

	// Possess 可能发生在战斗世界的 BeginPlay 之后，因此重新解析控制模式而非信任之前设置的值。
	// 否则探索上下文会被错误地加回战斗模式之上，导致相机再次环绕。
	const EHSRPlayerControlMode ResolvedMode = ResolveControlModeForCurrentWorld();
	if (ResolvedMode != CurrentControlMode)
	{
		SetControlMode(ResolvedMode);
	}

	// 首次添加由 SetupInputComponent 负责（与 UE 5.6 模板保持一致）。
	if (bInputSystemReady && CurrentControlMode == EHSRPlayerControlMode::Exploration)
	{
		AddExplorationContext();
	}

	// 若 HUD 已存在，刷新交互观察者以响应新 Possess 的 Pawn。
	if (AHSRHUD* HSRHUD = Cast<AHSRHUD>(GetHUD()))
	{
		HSRHUD->RefreshInteractionObserver();
	}

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::OnPossess - Controller=%s Pawn=%s"),
		*GetName(), *InPawn->GetName());
}

void AHSRPlayerController::OnUnPossess()
{
	// 先移除探索输入上下文，避免 Pawn 解绑后残留输入。
	RemoveExplorationContext();

	// 在 Super 清空 Pawn 引用之前清理 HUD 交互观察实例。
	if (AHSRHUD* HSRHUD = Cast<AHSRHUD>(GetHUD()))
	{
		HSRHUD->ClearInteractionObserverInstance();
	}

	Super::OnUnPossess();
}

// 按控制模式构建输入策略：决定输入焦点意图与鼠标光标是否可见。
void AHSRPlayerController::BuildPolicyForControlMode(EHSRPlayerControlMode Mode, FHSRInputModePolicy& OutPolicy)
{
	switch (Mode)
	{
	case EHSRPlayerControlMode::UIOnly:
		// 菜单与结算面板：Pawn 不接收任何输入，只留 UI 输入并显示光标。
		OutPolicy.InputIntent = EHSRUIInputIntent::UIOnly;
		OutPolicy.bShowMouseCursor = true;
		break;
	case EHSRPlayerControlMode::Battle:
		// 战斗命令面板需要点击，但战斗世界仍要接收游戏输入（技能、镜头取景），
		// 因此用 GameAndUI 而非 UIOnly。Pawn 保持 Tick；视角/移动输入由
		// ApplyUIInputPolicy 单独抑制（因为战斗世界 Possess 的正是探索 Pawn）。
		OutPolicy.InputIntent = EHSRUIInputIntent::GameAndUI;
		OutPolicy.bShowMouseCursor = true;
		break;
	case EHSRPlayerControlMode::Exploration:
	default:
		OutPolicy.InputIntent = EHSRUIInputIntent::GameOnly;
		OutPolicy.bShowMouseCursor = false;
		break;
	}
}

// 设置控制模式：按模式构造输入策略并应用（输入焦点/光标/探索上下文增删）。
void AHSRPlayerController::SetControlMode(EHSRPlayerControlMode NewMode)
{
	FHSRInputModePolicy Policy;
	BuildPolicyForControlMode(NewMode, Policy);
	ApplyUIInputPolicy(Policy, NewMode);
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::SetControlMode - Applied mode %d"),
		static_cast<uint8>(CurrentControlMode));
}

// 应用 UI 输入策略：按意图设置输入焦点、光标可见性，并在探索模式下恢复/添加探索输入上下文。
// 非探索模式下还会抑制 Pawn 的视角/移动输入（见函数内注释）。
bool AHSRPlayerController::ApplyUIInputPolicy(const FHSRInputModePolicy& Policy, const EHSRPlayerControlMode SemanticMode)
{
	if (!IsLocalPlayerController())
	{
		return false;
	}
	// 幂等短路：模式与意图都没变时直接成功。
	if (bControlModeApplied && CurrentControlMode == SemanticMode
		&& AppliedInputIntent == Policy.InputIntent && bShowMouseCursor == Policy.bShowMouseCursor)
	{
		return true;
	}
	// 探索输入上下文只属于探索模式：每次切换都移除它（而不只是第一次），
	// 这样战斗命令面板打开时 Mouse2D 无法到达 Pawn，独立于上面的短路逻辑。
	RemoveExplorationContext();

	switch (Policy.InputIntent)
	{
	case EHSRUIInputIntent::GameOnly:
		SetInputMode(FInputModeGameOnly());
		break;
	case EHSRUIInputIntent::UIOnly:
	{
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(Mode);
		break;
	}
	case EHSRUIInputIntent::GameAndUI:
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		break;
	}
	default:
		return false;
	}

	CurrentControlMode = SemanticMode;
	AppliedInputIntent = Policy.InputIntent;
	bShowMouseCursor = Policy.bShowMouseCursor;

	// 非探索模式抑制 Pawn 的视角/移动。战斗世界 Possess 的是同一个探索 Pawn，
	// 其 CameraBoom 用了 bUsePawnControlRotation，因此仅改输入焦点不够：
	// GameAndUI 仍会把 Mouse2D 送给 Look 动作，相机会在光标下继续环绕。
	// 在这里（而非 Pawn 内）做门控，保证规则只有一个归属者——Pawn 无法知道输入为何被抑制。
	//
	// SetIgnoreLookInput/SetIgnoreMoveInput 是引用计数而非布尔值：
	// 调用两次设置需要两次匹配的释放。先重置再设置，避免重复切换模式
	// 残留永久抑制，导致探索模式无法恢复。
	const bool bBlockPawnInput = SemanticMode != EHSRPlayerControlMode::Exploration;
	ResetIgnoreInputFlags();
	if (bBlockPawnInput)
	{
		SetIgnoreLookInput(true);
		SetIgnoreMoveInput(true);
	}

	if (SemanticMode == EHSRPlayerControlMode::Exploration && bInputSystemReady)
	{
		AddExplorationContext();
	}
	bControlModeApplied = true;
	return true;
}

// 请求打开暂停界面（经 UI 管理器走统一栈逻辑）。
void AHSRPlayerController::RequestOpenPauseScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenPauseScreen();
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 RequestOpen Result=%d Stack=%d HasPause=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

// 请求关闭前端到根界面（不经过对话覆盖层判断）。
void AHSRPlayerController::RequestCloseFrontendToRoot()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			Manager->CloseFrontendToRoot();
		}
	}
}

// 暂停键处理：有对话覆盖层先关对话，否则栈底开 PauseHub、其余栈状态执行返回。
void AHSRPlayerController::HandlePauseBack()
{
	ULocalPlayer* LP = GetLocalPlayer();
	UHSRUIManagerSubsystem* Manager = LP ? LP->GetSubsystem<UHSRUIManagerSubsystem>() : nullptr;
	if (!Manager)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI P17 PauseBack ignored LocalPlayer=%s Manager=None"), LP ? TEXT("valid") : TEXT("None"));
		return;
	}
	if (Manager->HasOpenDialogueOverlay())
	{
		const EHSRUIScreenResult Result = Manager->CloseDialogueOverlay();
		UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue Back Result=%d HasOverlay=%s"),
			static_cast<int32>(Result), Manager->HasOpenDialogueOverlay() ? TEXT("true") : TEXT("false"));
		return;
	}

	// 栈内只剩 1 层（根/暂停层）时打开 PauseHub，否则逐层返回。
	const int32 StackBefore = Manager->GetLogicalScreenCount();
	const EHSRUIScreenResult Result = StackBefore <= 1
		? Manager->OpenFrontendModule(EHSRFrontendModule::PauseHub) : Manager->RequestBack();
	UE_LOG(LogTemp, Log, TEXT("HSRUI P17 PauseBack handled StackBefore=%d Result=%d StackAfter=%d HasPause=%s"),
		StackBefore, static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
		Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
}

// 打开背包前端模块。
void AHSRPlayerController::HandleInventory()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			M->OpenFrontendModule(EHSRFrontendModule::Inventory);
		}
	}
}

// 打开队伍前端模块。
void AHSRPlayerController::HandleParty()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			M->OpenFrontendModule(EHSRFrontendModule::Party);
		}
	}
}

// 打开地图前端模块。
void AHSRPlayerController::HandleMap()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			M->OpenFrontendModule(EHSRFrontendModule::Map);
		}
	}
}

// 打开挑战前端模块。
void AHSRPlayerController::HandleChallenge()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (auto* M = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			M->OpenFrontendModule(EHSRFrontendModule::Challenge);
		}
	}
}

// 关闭到根界面：有对话覆盖层时先关对话，否则关到根。
void AHSRPlayerController::HandleCloseToRoot()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			if (Manager->HasOpenDialogueOverlay())
			{
				const EHSRUIScreenResult Result = Manager->CloseDialogueOverlay();
				UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Dialogue CloseToRoot Result=%d HasOverlay=%s"),
					static_cast<int32>(Result), Manager->HasOpenDialogueOverlay() ? TEXT("true") : TEXT("false"));
				return;
			}
		}
	}
	RequestCloseFrontendToRoot();
}

// 添加前端导航输入上下文（供 UI 面板导航使用，优先级 10）。
void AHSRPlayerController::AddFrontendNavigationContext()
{
	if (!FrontendNavigationMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend context is not configured"));
		return;
	}
	ULocalPlayer* LP = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSRUI Frontend context unavailable LocalPlayer=%s Subsystem=%s"),
			LP ? TEXT("valid") : TEXT("None"), Subsystem ? TEXT("valid") : TEXT("None"));
		return;
	}

	// 若上下文已存在则视为"已验证"，否则按"恢复"逻辑重新添加。
	const bool bContextPresent = Subsystem->HasMappingContext(FrontendNavigationMappingContext);
	if (ShouldRestoreFrontendNavigationContext(bFrontendNavigationContextAdded, bContextPresent))
	{
		FModifyContextOptions Options;
		Options.bForceImmediately = true;
		Subsystem->AddMappingContext(FrontendNavigationMappingContext, 10, Options);
	}
	bFrontendNavigationContextAdded = true;
	UE_LOG(LogTemp, Log, TEXT("HSRUI Frontend context %s Context=%s Present=%s"),
		bContextPresent ? TEXT("verified") : TEXT("restored"), *FrontendNavigationMappingContext->GetPathName(),
		Subsystem->HasMappingContext(FrontendNavigationMappingContext) ? TEXT("true") : TEXT("false"));
}

// 请求返回上一屏（经 UI 管理器栈）。
void AHSRPlayerController::RequestBackScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->RequestBack();
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 RequestBack Result=%d Stack=%d HasPause=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenPauseScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

// 请求打开角色详情屏幕（经 UI 管理器栈）。
void AHSRPlayerController::RequestOpenCharacterDetailScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenFrontendModule(EHSRFrontendModule::Character);
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 CharacterDetail RequestOpen Result=%d Stack=%d HasDetail=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenCharacterDetailScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

// 请求打开背包屏幕（经 UI 管理器栈）。
void AHSRPlayerController::RequestOpenInventoryScreen()
{
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UHSRUIManagerSubsystem* Manager = LP->GetSubsystem<UHSRUIManagerSubsystem>())
		{
			const EHSRUIScreenResult Result = Manager->OpenFrontendModule(EHSRFrontendModule::Inventory);
			UE_LOG(LogTemp, Log, TEXT("HSRUI P17 Inventory RequestOpen Result=%d Stack=%d HasInventory=%s"),
				static_cast<int32>(Result), Manager->GetLogicalScreenCount(),
				Manager->HasOpenInventoryScreen() ? TEXT("true") : TEXT("false"));
		}
	}
}

// 把探索输入上下文加入增强输入子系统（优先级 0），并打印每个动作的映射以便排查。
void AHSRPlayerController::AddExplorationContext()
{
	if (bExplorationContextAdded)
	{
		return;
	}

	if (!ExplorationMappingContext)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - ExplorationMappingContext is not set"));
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - No LocalPlayer"));
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("AHSRPlayerController::AddExplorationContext - No EnhancedInputLocalPlayerSubsystem"));
		return;
	}

	FModifyContextOptions Options;
	Options.bForceImmediately = true;
	Subsystem->AddMappingContext(ExplorationMappingContext, 0, Options);
	bExplorationContextAdded = true;
	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - Added %s"),
		*ExplorationMappingContext->GetName());

	UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - HasContext=%s"),
		Subsystem->HasMappingContext(ExplorationMappingContext) ? TEXT("true") : TEXT("false"));

	for (const FEnhancedActionKeyMapping& Mapping : ExplorationMappingContext->GetMappings())
	{
		UE_LOG(LogTemp, Log, TEXT("AHSRPlayerController::AddExplorationContext - Mapping Action=%s Key=%s"),
			Mapping.Action ? *Mapping.Action->GetPathName() : TEXT("None"),
			*Mapping.Key.ToString());
	}
}

// 从增强输入子系统移除探索输入上下文（幂等：未添加则直接返回）。
void AHSRPlayerController::RemoveExplorationContext()
{
	if (!bExplorationContextAdded)
	{
		return;
	}

	if (!ExplorationMappingContext)
	{
		bExplorationContextAdded = false;
		return;
	}

	ULocalPlayer* LP = GetLocalPlayer();
	if (!LP)
	{
		bExplorationContextAdded = false;
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!Subsystem)
	{
		bExplorationContextAdded = false;
		return;
	}

	Subsystem->RemoveMappingContext(ExplorationMappingContext);
	bExplorationContextAdded = false;
}
