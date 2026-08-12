#include "HSRGameModeBase.h"

#include "../Character/HSRCharacterBase.h"
#include "../Data/Definitions/HSRCharacterCatalog.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSRMapCatalog.h"
#include "../Data/Definitions/HSRMapDefinition.h"
#include "../Data/Definitions/HSRTeleportDefinition.h"
#include "../Map/HSRMapSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Player/HSRPlayerController.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "Curves/CurveFloat.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

AHSRGameModeBase::AHSRGameModeBase()
{
	// BP_HSRGameMode already sets this in the asset, so exploration works today. Pinning the
	// same default in C++ costs nothing and stops a newly authored GameMode Blueprint from
	// silently inheriting the engine controller -- exactly how the battle GameMode lost its
	// input handling.
	// （中文说明）资源里已经配好了 PlayerController 类，这里在 C++ 里再固定一次默认值：
	// 防止新创建的 GameMode 蓝图在没接线的情况下静默回落到引擎默认 Controller，
	// 曾经战斗 GameMode 就是这样丢掉了输入处理。
	PlayerControllerClass = AHSRPlayerController::StaticClass();
}

void AHSRGameModeBase::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);
	// 玩家重生后（有 Pawn 了）才做地图/角色引导，因为引导需要拿到实际 Pawn。
	if (NewPlayer && NewPlayer->GetPawn())
	{
#if WITH_DEV_AUTOMATION_TESTS
		// 自动化测试注入的 Controller 优先于第一玩家控制器。
		AutomationController = NewPlayer;
#endif
		BootstrapMapDefinitions();
		BootstrapCharacterIdentity(CharacterBootstrapMode);
	}
}

// 地图引导：把地图目录里的所有地图/传送点/区域登记进地图子系统，并设置初始位置。
// 只在启动阶段执行一次；任一注册失败都会记录结果码并提前返回。
EHSRMapBootstrapResult AHSRGameModeBase::BootstrapMapDefinitions()
{
	if (!MapCatalog)
	{
		LastMapBootstrapResult = EHSRMapBootstrapResult::MissingCatalog;
		return LastMapBootstrapResult;
	}

	UGameInstance* GameInstance = GetGameInstance();
	UHSRMapSubsystem* Maps = GameInstance ? GameInstance->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	if (!Maps)
	{
		LastMapBootstrapResult = EHSRMapBootstrapResult::MissingCatalog;
		return LastMapBootstrapResult;
	}

	for (const TObjectPtr<UHSRMapDefinition>& MapEntry : MapCatalog->Maps)
	{
		if (!MapEntry)
		{
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
		const EHSRMapOperationResult RegisterResult = Maps->RegisterMapAsset(MapEntry);
		// NoOp 视为可接受：说明之前已注册过相同定义。
		if (RegisterResult != EHSRMapOperationResult::Success && RegisterResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap map '%s' register failed result=%d"),
				*MapEntry->MapId.ToString(),
				static_cast<int32>(RegisterResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
	}

	for (const TObjectPtr<UHSRTeleportDefinition>& TeleportEntry : MapCatalog->Teleports)
	{
		if (!TeleportEntry)
		{
			LastMapBootstrapResult = EHSRMapBootstrapResult::TeleportRegistrationFailed;
			return LastMapBootstrapResult;
		}
		const EHSRMapOperationResult RegisterResult = Maps->RegisterTeleportAsset(TeleportEntry);
		if (RegisterResult != EHSRMapOperationResult::Success && RegisterResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap teleport '%s' register failed result=%d"),
				*TeleportEntry->TeleportId.ToString(),
				static_cast<int32>(RegisterResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::TeleportRegistrationFailed;
			return LastMapBootstrapResult;
		}
	}

	// Unlock every catalog region so all authored teleports are reachable from their source maps.
	// （中文说明）解锁目录里的所有区域，让每个作者的传送点都从各自源地图可达。
	for (const TObjectPtr<UHSRMapDefinition>& MapEntry : MapCatalog->Maps)
	{
		if (!MapEntry)
		{
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
		const EHSRMapOperationResult UnlockResult = Maps->UnlockRegion(MapEntry->RegionId);
		if (UnlockResult != EHSRMapOperationResult::Success && UnlockResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap region '%s' unlock failed result=%d"),
				*MapEntry->RegionId.ToString(),
				static_cast<int32>(UnlockResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::MapRegistrationFailed;
			return LastMapBootstrapResult;
		}
	}

	// 配置了初始地图时，把“当前所在地”设为初始地图。
	if (!InitialMapId.IsNone())
	{
		const EHSRMapOperationResult LocationResult = Maps->SetCurrentLocation(InitialMapId);
		if (LocationResult != EHSRMapOperationResult::Success && LocationResult != EHSRMapOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Warning, TEXT("P18 MapBootstrap initial location '%s' failed result=%d"),
				*InitialMapId.ToString(),
				static_cast<int32>(LocationResult));
			LastMapBootstrapResult = EHSRMapBootstrapResult::InitialLocationFailed;
			return LastMapBootstrapResult;
		}
	}

	LastMapBootstrapResult = EHSRMapBootstrapResult::Success;
	UE_LOG(LogTemp, Log, TEXT("P18 MapBootstrap registered %d maps %d teleports regions unlocked initial=%s"),
		MapCatalog->Maps.Num(),
		MapCatalog->Teleports.Num(),
		*InitialMapId.ToString());
	return LastMapBootstrapResult;
}

// 角色身份引导：把目录里的角色定义登记进档案子系统，并给当前 Pawn 投影一个选定的角色。
// 处理“首次进入（无队伍成员）时用初始角色建队”和“校验已登记目录与当前目录一致”两种场景。
EHSRCharacterBootstrapResult AHSRGameModeBase::BootstrapCharacterIdentity(const EHSRCharacterBootstrapMode Mode)
{
	// 必须先拿到可用的 Pawn：引导最终要把角色投影到它身上。
	AController* Controller = ResolveBootstrapController();
	AHSRCharacterBase* Character = Controller ? Cast<AHSRCharacterBase>(Controller->GetPawn()) : nullptr;
	if (!Character)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PawnProjectionFailed);
	}
	if (!CharacterCatalog)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::MissingCatalog);
	}

	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	// 档案与队伍子系统缺一不可，否则无法登记角色定义或读取队伍。
	if (!Profiles || !Party)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PartyUnavailable);
	}

	// 逐个校验目录条目：必须有有效定义、有角色 ID、经验曲线可加载且不重复。
	TSet<FName> CatalogIds;
	bool bContainsInitialCharacter = false;
	int32 RegisteredDefinitionCount = 0;
	for (const TSubclassOf<UHSRCharacterDefinition>& Entry : CharacterCatalog->Characters)
	{
		const UHSRCharacterDefinition* Definition = Entry ? Entry->GetDefaultObject<UHSRCharacterDefinition>() : nullptr;
		if (!Definition || Definition->CharacterId.IsNone() || CatalogIds.Contains(Definition->CharacterId)
			|| Definition->CumulativeExperienceCurve.IsNull()
			|| !Definition->CumulativeExperienceCurve.LoadSynchronous())
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::CatalogConflict);
		}

		CatalogIds.Add(Definition->CharacterId);
		bContainsInitialCharacter |= Definition->CharacterId == InitialCharacterId;
		// 若该角色此前已被登记过，必须与当前目录指向同一个定义对象，否则目录自相矛盾。
		const UHSRCharacterDefinition* RegisteredDefinition = nullptr;
		if (Profiles->GetDefinition(Definition->CharacterId, RegisteredDefinition))
		{
			if (RegisteredDefinition != Definition)
			{
				return FinishBootstrap(EHSRCharacterBootstrapResult::CatalogConflict);
			}
			++RegisteredDefinitionCount;
		}
	}

	// 初始角色必须存在于目录里。
	if (InitialCharacterId.IsNone() || !bContainsInitialCharacter)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::InvalidInitialCharacter);
	}
	// 要么目录里所有角色都已登记，要么一个都没登记（避免只登记一半的怪状态）。
	if (RegisteredDefinitionCount != 0 && RegisteredDefinitionCount != CatalogIds.Num())
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::CatalogConflict);
	}
	// 整个目录还没登记过时，一次性登记全部。
	if (RegisteredDefinitionCount == 0
		&& Profiles->RegisterLoadedCatalog(CharacterCatalog) != EHSRCharacterProfileResult::Success)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::ProfileRegistrationFailed);
	}

	FHSRPartySnapshot PartySnapshot;
	if (!Party->GetSnapshot(PartySnapshot) || PartySnapshot.Slots.IsEmpty())
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PartyUnavailable);
	}

	// 若 0 号槽位空着：先看是否已有别的槽位提交过成员（有则说明选择未落定），
	// 再在“未要求使用已提交运行时”的前提下用初始角色补位建队。
	bool bSeededParty = false;
	if (PartySnapshot.Slots[0].IsEmpty())
	{
		const bool bHasCommittedPartyMember = PartySnapshot.Slots.ContainsByPredicate(
			[](const FHSRPartySlot& Slot) { return !Slot.IsEmpty(); });
		if (bHasCommittedPartyMember)
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::NoCommittedSelection);
		}
		if (Mode == EHSRCharacterBootstrapMode::UseCommittedRuntime)
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::NoCommittedSelection);
		}
		if (Party->AddCharacter(InitialCharacterId, 0) != EHSRPartyResult::Success)
		{
			return FinishBootstrap(EHSRCharacterBootstrapResult::PartyUnavailable);
		}
		bSeededParty = true;
		Party->GetSnapshot(PartySnapshot);
	}

	// 取 0 号槽位的角色作为“选定角色”，拿到它的档案快照与定义。
	const FName SelectedCharacterId = PartySnapshot.Slots[0].CharacterId;
	FHSRCharacterProfileSnapshot SelectedProfile;
	const UHSRCharacterDefinition* SelectedDefinition = nullptr;
	if (SelectedCharacterId.IsNone() || !Profiles->GetProfileSnapshot(SelectedCharacterId, SelectedProfile)
		|| !Profiles->GetDefinition(SelectedCharacterId, SelectedDefinition) || !SelectedDefinition)
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::NoCommittedSelection);
	}
	if (!Character->SetProjectedCharacterId(SelectedCharacterId))
	{
		return FinishBootstrap(EHSRCharacterBootstrapResult::PawnProjectionFailed);
	}

	// 首次建队返回 Success，沿用已有队伍返回 NoOp。
	return FinishBootstrap(bSeededParty ? EHSRCharacterBootstrapResult::Success : EHSRCharacterBootstrapResult::NoOp,
		SelectedCharacterId);
}

// 引导收尾：记录结果码，成功时保存已解析的角色 ID，并打日志。
EHSRCharacterBootstrapResult AHSRGameModeBase::FinishBootstrap(const EHSRCharacterBootstrapResult Result,
	const FName CharacterId)
{
	LastCharacterBootstrapResult = Result;
	const bool bSucceeded = Result == EHSRCharacterBootstrapResult::Success
		|| Result == EHSRCharacterBootstrapResult::NoOp;
	if (bSucceeded)
	{
		ResolvedCharacterId = CharacterId;
	}
	if (bSucceeded)
	{
		UE_LOG(LogTemp, Log, TEXT("P17-PATCH-03B Bootstrap Result=%d CharacterId=%s"),
			static_cast<int32>(Result),
			*CharacterId.ToString());
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("P17-PATCH-03B Bootstrap Result=%d CharacterId=%s"),
			static_cast<int32>(Result),
			*CharacterId.ToString());
	}
	return Result;
}

// 解析引导用的 Controller：自动化测试注入的优先，否则取世界里的第一玩家控制器。
AController* AHSRGameModeBase::ResolveBootstrapController() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (AutomationController.IsValid())
	{
		return AutomationController.Get();
	}
#endif
	return GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试专用：注入角色引导所需的目录、初始角色与 Controller。
void AHSRGameModeBase::ConfigureCharacterBootstrapForAutomation(UHSRCharacterCatalog* InCatalog,
	const FName InInitialCharacterId, AController* InController)
{
	CharacterCatalog = InCatalog;
	InitialCharacterId = InInitialCharacterId;
	AutomationController = InController;
}

// 自动化测试专用：注入地图引导所需的目录与初始地图。
void AHSRGameModeBase::ConfigureMapBootstrapForAutomation(UHSRMapCatalog* InCatalog, const FName InInitialMapId)
{
	MapCatalog = InCatalog;
	InitialMapId = InInitialMapId;
}
#endif
