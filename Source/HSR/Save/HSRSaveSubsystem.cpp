#include "HSRSaveSubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"
#include "HSRSaveGame.h"
#include "HSRSaveVersion.h"
#include "Kismet/GameplayStatics.h"
#include "../Battle/HSRBattleTransitionSubsystem.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../Battle/HSRBattleGameMode.h"
#include "../Battle/HSRBattleCoordinator.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "AbilitySystemComponent.h"

// 匿名命名空间：编辑器专用的控制台命令（开发测试用，不进生产构建）。
// 这些命令在 PIE 环境里直接操作存档/战斗子系统，用来验证
// 「存档往返后战斗侧的成长上下文与血量为预期」等跨系统一致性。
namespace
{
	// 从当前 PIE 世界链里找到正在运行的那个 UWorld。
	UWorld* GetPIEWorld()
	{
		UWorld* W = nullptr;
		if (GEngine)
		{
			for (const FWorldContext& C : GEngine->GetWorldContexts())
			{
				if (C.World() && C.World()->IsPlayInEditor())
				{
					W = C.World();
					break;
				}
			}
		}
		return W;
	}

	// RunHSRSavePIEAudit：P11-005 存档/战斗往返审计。
	// 流程：取基线快照 -> 写临时槽 -> 改角色经验 -> 再存 -> 读回基线 ->
	// 从临时槽读档 -> 校验战斗参与者里 Player 的血量/成长句柄与刷新计数。
	void RunHSRSavePIEAudit()
	{
		UWorld* W = GetPIEWorld();
		AHSRBattleGameMode* GM = W ? Cast<AHSRBattleGameMode>(W->GetAuthGameMode()) : nullptr;
		UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
		UHSRSaveSubsystem* S = GI ? GI->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
		UHSRCharacterProfileSubsystem* P = GI ? GI->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		UHSRBattleCoordinator* BC = GM ? GM->GetCoordinator() : nullptr;
		if (!S || !P || !BC)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.SaveTest FAILED MissingPIEChain"));
			return;
		}

		// 用随机槽位名避免污染正式存档。
		const FString Slot = FString::Printf(TEXT("HSR_SaveTest_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		FHSRSaveData Baseline;
		if (S->SaveSnapshot(Baseline) != EHSRSaveResult::Success || S->SaveToSlot(Slot) != EHSRSaveResult::Success)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.SaveTest FAILED Baseline"));
			return;
		}

		// 改变运行时状态（给角色 A 加经验）后存槽、再读回基线（验证快照恢复）。
		P->GrantExperience(TEXT("Character.A"), 100);
		S->SaveToSlot(Slot);
		S->LoadSnapshot(Baseline);

		// 记录读档前的成长句柄/刷新计数。
		const FString Old = BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));
		const int32 RefreshBefore = BC->GetProgressionRefreshCountForDevelopmentTest();

		// 真正从临时槽读档，观察成长句柄/刷新计数/玩家血量变化。
		const EHSRSaveResult First = S->LoadFromSlot(Slot);
		const FString New = BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));
		float HP = 0, MaxHP = 0;
		for (const FHSRBattleParticipant& BP : BC->GetParticipants())
		{
			if (BP.ParticipantId == TEXT("Player") && BP.AbilitySystemComponent.IsValid())
			{
				HP = BP.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
				MaxHP = BP.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute());
			}
		}
		const int32 RefreshAfter = BC->GetProgressionRefreshCountForDevelopmentTest();

		// 第二次读档应是无操作（NoOp）：句柄应保持稳定、刷新计数不再增加。
		const EHSRSaveResult Repeat = S->LoadFromSlot(Slot);
		const FString RepeatHandle = BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));

		UE_LOG(LogTemp, Log, TEXT("HSR.SaveTest TxRefresh=%d->%d First=%d Repeat=%d Old=%s New=%s RepeatHandle=%s Secondary=%d Matching=%d Fingerprint=%s Health=%.3f MaxHealth=%.3f RefreshResult=%d"),
			RefreshBefore, RefreshAfter, static_cast<int32>(First), static_cast<int32>(Repeat),
			*Old, *New, *RepeatHandle,
			BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")),
			BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")),
			*BC->GetProgressionFingerprintForDevelopmentTest(TEXT("Player")),
			HP, MaxHP, BC->GetLastProgressionRefreshResultForDevelopmentTest() ? 1 : 0);

		// 清理临时槽位。
		UGameplayStatics::DeleteGameInSlot(Slot, 0);
		UE_LOG(LogTemp, Log, TEXT("HSR.SaveTest COMPLETE Cleanup=%d NoOpHandleStable=%d"),
			UGameplayStatics::DoesSaveGameExist(Slot, 0) ? 0 : 1, New == RepeatHandle ? 1 : 0);
	}

	// RunHSRProgressionFailureTest：P11-006 成长应用/移除失败注入审计。
	// 验证当 Apply 或旧句柄移除被注入失败时，运行时能保持原句柄不脏写；
	// 失败解除后重试应成功，且句柄唯一、副句柄为 0。
	void RunHSRProgressionFailureTest()
	{
		UWorld* W = GetPIEWorld();
		AHSRBattleGameMode* GM = W ? Cast<AHSRBattleGameMode>(W->GetAuthGameMode()) : nullptr;
		UHSRBattleCoordinator* BC = GM ? GM->GetCoordinator() : nullptr;
		UHSRCharacterProfileSubsystem* P = W && W->GetGameInstance()
			? W->GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		if (!BC || !P)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.ProgressionFailureTest Result=FAIL Reason=MissingPIEChain"));
			return;
		}

		// 读取角色 A 的当前成长上下文与 Player 血量。
		FHSRCharacterProgressionContext Base;
		if (!P->GetProgressionContext(TEXT("Character.A"), Base))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.ProgressionFailureTest Result=FAIL Reason=MissingProfile"));
			return;
		}
		float Health = 0;
		for (const FHSRBattleParticipant& X : BC->GetParticipants())
		{
			if (X.ParticipantId == TEXT("Player") && X.AbilitySystemComponent.IsValid())
			{
				Health = X.AbilitySystemComponent->GetNumericAttribute(UHSRCoreAttributeSet::GetHealthAttribute());
			}
		}
		const FString Old = BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));

		// 第一次失败注入：Apply 阶段失败。期望句柄保持 Old、无副句柄。
		FHSRCharacterProgressionContext First = Base;
		++First.RuntimeRevision;
		First.ProgressionBonuses.MaxHealth += 10.0f;
		BC->SetProgressionApplyFailureForDevelopmentTest(true);
		const bool ApplyFail = BC->RefreshCharacterProgression(TEXT("Player"), First);
		BC->SetProgressionApplyFailureForDevelopmentTest(false);
		const bool ApplyPreserved = !ApplyFail && Old == BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"))
			&& BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")) == 1
			&& BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")) == 0;

		// 失败解除后重试应成功。
		const bool FirstRetry = BC->RefreshCharacterProgression(TEXT("Player"), First);
		const FString New = BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));

		// 第二次失败注入：旧句柄移除阶段失败。期望新句柄不落地、仍保持 New。
		FHSRCharacterProgressionContext Second = First;
		++Second.RuntimeRevision;
		Second.ProgressionBonuses.MaxHealth += 10.0f;
		BC->SetProgressionOldRemoveFailureForDevelopmentTest(true);
		const bool RemoveFail = BC->RefreshCharacterProgression(TEXT("Player"), Second);
		BC->SetProgressionOldRemoveFailureForDevelopmentTest(false);
		const bool RemovePreserved = !RemoveFail && New == BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"))
			&& BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")) == 1
			&& BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")) == 0;

		// 再次重试成功，最终句柄唯一且不同于 New。
		const bool SecondRetry = BC->RefreshCharacterProgression(TEXT("Player"), Second);
		const FString Final = BC->GetProgressionPrimaryHandleForDevelopmentTest(TEXT("Player"));
		const bool FinalUnique = SecondRetry && Final != New
			&& BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")) == 1
			&& BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")) == 0;

		const bool bPassed = ApplyPreserved && FirstRetry && RemovePreserved && FinalUnique;
		if (bPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("HSR.ProgressionFailureTest Result=PASS ApplyFail=%d ApplyPreserved=%d FirstRetry=%d RemoveFail=%d RemovePreserved=%d FinalUnique=%d Health=%.3f Old=%s New=%s Final=%s Matching=%d Secondary=%d Fingerprint=%s"),
				ApplyFail ? 1 : 0, ApplyPreserved ? 1 : 0, FirstRetry ? 1 : 0,
				RemoveFail ? 1 : 0, RemovePreserved ? 1 : 0, FinalUnique ? 1 : 0,
				Health, *Old, *New, *Final,
				BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")),
				BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")),
				*BC->GetProgressionFingerprintForDevelopmentTest(TEXT("Player")));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.ProgressionFailureTest Result=FAIL ApplyFail=%d ApplyPreserved=%d FirstRetry=%d RemoveFail=%d RemovePreserved=%d FinalUnique=%d Health=%.3f Old=%s New=%s Final=%s Matching=%d Secondary=%d Fingerprint=%s"),
				ApplyFail ? 1 : 0, ApplyPreserved ? 1 : 0, FirstRetry ? 1 : 0,
				RemoveFail ? 1 : 0, RemovePreserved ? 1 : 0, FinalUnique ? 1 : 0,
				Health, *Old, *New, *Final,
				BC->GetProgressionActiveHandleCountForDevelopmentTest(TEXT("Player")),
				BC->GetProgressionSecondaryCountForDevelopmentTest(TEXT("Player")),
				*BC->GetProgressionFingerprintForDevelopmentTest(TEXT("Player")));
		}
	}

	// GetCloseoutSave：取当前 PIE 里的存档子系统，供各 Closeout 命令共用。
	UHSRSaveSubsystem* GetCloseoutSave()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& C : GEngine->GetWorldContexts())
		{
			if (C.World() && C.World()->IsPlayInEditor())
			{
				return C.World()->GetGameInstance()
					? C.World()->GetGameInstance()->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
			}
		}
		return nullptr;
	}

	// RunCloseoutSave：把当前运行时快照写入 P11 收尾槽。
	void RunCloseoutSave()
	{
		UHSRSaveSubsystem* S = GetCloseoutSave();
		if (!S)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.CloseoutSave Result=FAIL Reason=MissingPIESave"));
			return;
		}
		const EHSRSaveResult R = S->SaveToSlot(TEXT("HSR_P11_Closeout"));
		const FHSRSaveData& D = S->GetSnapshot();
		UE_LOG(LogTemp, Log, TEXT("HSR.CloseoutSave Result=%d ProfileCount=%d PartyRevision=%lld"),
			static_cast<int32>(R), D.Profiles.Num(), D.PartyRevision);
	}

	// RunCloseoutLoad：从 P11 收尾槽读档，并打印恢复后的事务版本/角色版本/队伍版本。
	void RunCloseoutLoad()
	{
		UHSRSaveSubsystem* S = GetCloseoutSave();
		if (!S)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.CloseoutLoad Result=FAIL Reason=MissingPIESave"));
			return;
		}
		const EHSRSaveResult R = S->LoadFromSlot(TEXT("HSR_P11_Closeout"));
		FHSRCharacterProfileSnapshot A;
		UHSRCharacterProfileSubsystem* P = S->GetGameInstance()
			? S->GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		UHSRPartySubsystem* Party = S->GetGameInstance()
			? S->GetGameInstance()->GetSubsystem<UHSRPartySubsystem>() : nullptr;
		FHSRPartySnapshot PS;
		if (Party)
		{
			Party->GetSnapshot(PS);
		}
		UE_LOG(LogTemp, Log, TEXT("HSR.CloseoutLoad Result=%d RestoreTx=%lld CharacterARevision=%lld PartyRevision=%lld"),
			static_cast<int32>(R), S->GetRestoreTransactionRevisionForDevelopmentTest(),
			P && P->GetProfileSnapshot(TEXT("Character.A"), A) ? A.RuntimeRevision : -1, PS.Revision);
	}

	// RunCloseoutCleanup：删除 P11 收尾槽，并验证确实已删除。
	void RunCloseoutCleanup()
	{
		const bool bExisted = UGameplayStatics::DoesSaveGameExist(TEXT("HSR_P11_Closeout"), 0);
		if (bExisted)
		{
			UGameplayStatics::DeleteGameInSlot(TEXT("HSR_P11_Closeout"), 0);
		}
		const bool bGone = !UGameplayStatics::DoesSaveGameExist(TEXT("HSR_P11_Closeout"), 0);
		if (bGone)
		{
			UE_LOG(LogTemp, Log, TEXT("HSR.CloseoutCleanup Result=SUCCESS Existed=%d Absent=1"), bExisted ? 1 : 0);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.CloseoutCleanup Result=FAIL Existed=%d Absent=0"), bExisted ? 1 : 0);
		}
	}

	// LogP13State：打印 P13 收尾槽的存档状态（schema/背包/奖励各维度）。
	void LogP13State(const TCHAR* Operation, EHSRSaveResult Result, UHSRSaveSubsystem* Save)
	{
		const FHSRSaveData* Data = Save ? &Save->GetSnapshot() : nullptr;
		UE_LOG(LogTemp, Log, TEXT("P13-004 %s Result=%d Schema=%d Stacks=%d Unique=%d Claims=%d InventoryRevision=%lld RewardRevision=%lld"),
			Operation, static_cast<int32>(Result),
			Data ? Data->SchemaVersion : -1,
			Data ? Data->Inventory.Stacks.Num() : -1,
			Data ? Data->Inventory.UniqueItems.Num() : -1,
			Data ? Data->Rewards.Receipts.Num() : -1,
			Data ? Data->Inventory.Revision : -1,
			Data ? Data->Rewards.Revision : -1);
	}

	void RunP13Save()
	{
		UHSRSaveSubsystem* S = GetCloseoutSave();
		const EHSRSaveResult R = S ? S->SaveToSlot(TEXT("HSR_P13_Closeout")) : EHSRSaveResult::InvalidData;
		LogP13State(TEXT("Save"), R, S);
	}

	// RunP13Clear：通过「快照 -> 清空背包/奖励域 -> 恢复」来清掉 P13 运行时数据。
	void RunP13Clear()
	{
		UHSRSaveSubsystem* S = GetCloseoutSave();
		if (!S)
		{
			LogP13State(TEXT("Clear"), EHSRSaveResult::InvalidData, S);
			return;
		}
		FHSRSaveData Data;
		const EHSRSaveResult Capture = S->SaveSnapshot(Data);
		if (Capture == EHSRSaveResult::Success)
		{
			Data.Inventory = FHSRInventorySaveData();
			Data.Rewards = FHSRRewardSaveData();
		}
		const EHSRSaveResult R = Capture == EHSRSaveResult::Success ? S->LoadSnapshot(Data) : Capture;
		LogP13State(TEXT("Clear"), R, S);
	}

	void RunP13Load()
	{
		UHSRSaveSubsystem* S = GetCloseoutSave();
		const EHSRSaveResult R = S ? S->LoadFromSlot(TEXT("HSR_P13_Closeout")) : EHSRSaveResult::InvalidData;
		LogP13State(TEXT("Load"), R, S);
	}

	void RunP13Cleanup()
	{
		const bool bDeleted = !UGameplayStatics::DoesSaveGameExist(TEXT("HSR_P13_Closeout"), 0)
			|| UGameplayStatics::DeleteGameInSlot(TEXT("HSR_P13_Closeout"), 0);
		UE_LOG(LogTemp, Log, TEXT("P13-004 Cleanup Result=%s"), bDeleted ? TEXT("SUCCESS") : TEXT("FAILED"));
	}

	// 注册上述命令为控制台命令（HSR.SaveTest 等）。
	FAutoConsoleCommand Cmd(TEXT("HSR.SaveTest"),
		TEXT("Runs the P11-005 Save/Battle PIE audit and cleans its temporary slot."),
		FConsoleCommandDelegate::CreateStatic(&RunHSRSavePIEAudit));
	FAutoConsoleCommand FailureCmd(TEXT("HSR.ProgressionFailureTest"),
		TEXT("Runs P11-006 progression apply/remove failure audit in PIE."),
		FConsoleCommandDelegate::CreateStatic(&RunHSRProgressionFailureTest));
	FAutoConsoleCommand CloseoutSaveCmd(TEXT("HSR.CloseoutSave"),
		TEXT("Writes the P11 closeout save slot."),
		FConsoleCommandDelegate::CreateStatic(&RunCloseoutSave));
	FAutoConsoleCommand CloseoutLoadCmd(TEXT("HSR.CloseoutLoad"),
		TEXT("Loads the P11 closeout save slot."),
		FConsoleCommandDelegate::CreateStatic(&RunCloseoutLoad));
	FAutoConsoleCommand CloseoutCleanupCmd(TEXT("HSR.CloseoutCleanup"),
		TEXT("Deletes the P11 closeout save slot."),
		FConsoleCommandDelegate::CreateStatic(&RunCloseoutCleanup));
	FAutoConsoleCommand P13SaveCmd(TEXT("HSR.P13Save"),
		TEXT("Saves the Phase 13 closeout slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP13Save));
	FAutoConsoleCommand P13ClearCmd(TEXT("HSR.P13Clear"),
		TEXT("Clears Phase 13 Inventory and Reward runtime through Save v3 restore."),
		FConsoleCommandDelegate::CreateStatic(&RunP13Clear));
	FAutoConsoleCommand P13LoadCmd(TEXT("HSR.P13Load"),
		TEXT("Loads the Phase 13 closeout slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP13Load));
	FAutoConsoleCommand P13CleanupCmd(TEXT("HSR.P13Cleanup"),
		TEXT("Deletes the Phase 13 closeout slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP13Cleanup));
}
#endif

#if WITH_EDITOR
namespace
{
	// P15 地图收尾槽位名。
	constexpr const TCHAR* P15MapSlot = TEXT("HSR_P15_Map_Closeout");

	// 获取当前 PIE 世界的存档子系统（与上面 GetCloseoutSave 类似的模式）。
	UHSRSaveSubsystem* GetP15SaveSubsystem()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->IsPlayInEditor())
			{
				return World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
			}
		}
		return nullptr;
	}

	// 打印 P15 地图收尾存档的关键状态（地图位置/区域/传送点/标记/版本号）。
	void LogP15MapSave(const TCHAR* Operation, const EHSRSaveResult Result, const UHSRSaveSubsystem* Save)
	{
		const FHSRSaveData* Data = Save ? &Save->GetSnapshot() : nullptr;
		UE_LOG(LogTemp, Log, TEXT("P15 MapSave %s Result=%d Schema=%d Map=%s Arrival=%s Location=%s Regions=%d Teleports=%d Flags=%d Revision=%lld RestoreTx=%lld"),
			Operation, static_cast<int32>(Result), Data ? Data->SchemaVersion : -1,
			Data ? *Data->Map.CurrentLocation.MapId.ToString() : TEXT("None"),
			Data ? *Data->Map.CurrentLocation.ArrivalId.ToString() : TEXT("None"),
			Data ? *Data->Map.CurrentLocation.WorldTransform.GetLocation().ToString() : TEXT("None"),
			Data ? Data->Map.UnlockedRegionIds.Num() : -1, Data ? Data->Map.UnlockedTeleportIds.Num() : -1,
			Data ? Data->Map.ExplorationFlags.Num() : -1, Data ? Data->Map.Revision : -1,
			Save ? Save->GetRestoreTransactionRevisionForDevelopmentTest() : -1);
	}

	void RunP15MapSave()
	{
		UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
		const EHSRSaveResult Result = Save ? Save->SaveToSlot(P15MapSlot) : EHSRSaveResult::InvalidData;
		LogP15MapSave(TEXT("Save"), Result, Save);
	}

	// RunP15MapSetFlag：设置一个编辑用探索标记，验证标记能被写入快照。
	void RunP15MapSetFlag()
	{
		UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
		UHSRMapSubsystem* Maps = Save && Save->GetGameInstance()
			? Save->GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
		const EHSRMapOperationResult Result = Maps
			? Maps->SetExplorationFlag(TEXT("Exploration.P15.EditorGate"))
			: EHSRMapOperationResult::InvalidWorld;
		UE_LOG(LogTemp, Log, TEXT("P15 MapSave SetFlag Result=%d Flag=Exploration.P15.EditorGate"),
			static_cast<int32>(Result));
	}

	// RunP15MapClear：通过「快照 -> 重置地图域 -> 恢复」清掉运行时地图状态（不碰磁盘槽）。
	void RunP15MapClear()
	{
		UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
		FHSRSaveData Data;
		EHSRSaveResult Result = Save ? Save->SaveSnapshot(Data) : EHSRSaveResult::InvalidData;
		if (Result == EHSRSaveResult::Success)
		{
			Data.Map = FHSRMapSaveData();
			Result = Save->LoadSnapshot(Data);
		}
		LogP15MapSave(TEXT("ClearRuntime"), Result, Save);
	}

	void RunP15MapLoad()
	{
		UHSRSaveSubsystem* Save = GetP15SaveSubsystem();
		const EHSRSaveResult Result = Save ? Save->LoadFromSlot(P15MapSlot) : EHSRSaveResult::InvalidData;
		LogP15MapSave(TEXT("Load"), Result, Save);
	}

	void RunP15MapCleanup()
	{
		const bool bDeleted = !UGameplayStatics::DoesSaveGameExist(P15MapSlot, 0)
			|| UGameplayStatics::DeleteGameInSlot(P15MapSlot, 0);
		UE_LOG(LogTemp, Log, TEXT("P15 MapSave Cleanup Result=%s"), bDeleted ? TEXT("SUCCESS") : TEXT("FAILED"));
	}

	FAutoConsoleCommand P15MapSaveCommand(TEXT("HSR.P15MapSave"), TEXT("Saves the Phase 15 map closeout slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP15MapSave));
	FAutoConsoleCommand P15MapSetFlagCommand(TEXT("HSR.P15MapSetFlag"), TEXT("Sets the Phase 15 Editor Gate exploration flag."),
		FConsoleCommandDelegate::CreateStatic(&RunP15MapSetFlag));
	FAutoConsoleCommand P15MapClearCommand(TEXT("HSR.P15MapClear"), TEXT("Clears runtime map state without touching the disk slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP15MapClear));
	FAutoConsoleCommand P15MapLoadCommand(TEXT("HSR.P15MapLoad"), TEXT("Loads the Phase 15 map closeout slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP15MapLoad));
	FAutoConsoleCommand P15MapCleanupCommand(TEXT("HSR.P15MapCleanup"), TEXT("Deletes the Phase 15 map closeout slot."),
		FConsoleCommandDelegate::CreateStatic(&RunP15MapCleanup));
}
#endif

// Initialize：子系统启动时把所有需要快照/恢复的领域子系统缓存为弱指针，并订阅
// 地图恢复旅行完成/失败事件。同时加载「物品 -> 装备映射目录」，用于存档校验时
// 核对背包唯一物品与装备定义之间的映射关系（见 Validate 里的 inventory-equipment-mapping）。
void UHSRSaveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	Party = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	Equipment = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	Inventory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	Reward = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRRewardSubsystem>() : nullptr;
	Quest = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRQuestSubsystem>() : nullptr;
	Map = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRMapSubsystem>() : nullptr;
	ChallengeProgression = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRChallengeProgressionSubsystem>() : nullptr;

	// 订阅地图恢复旅行事件：恢复读档可能涉及跨地图旅行（进入存档时的地图），
	// 真正落地要等旅行完成（HandleRestoreArrival）或失败（HandleRestoreTravelFailure）。
	if (Map.IsValid())
	{
		Map->OnArrivalCommitted().AddUObject(this, &UHSRSaveSubsystem::HandleRestoreArrival);
		Map->OnRestoreTravelFailed().AddUObject(this, &UHSRSaveSubsystem::HandleRestoreTravelFailure);
	}

	// 加载物品->装备映射目录；缺失时打错误日志（会阻断 inventory-equipment 校验）。
	MappingCatalog = LoadObject<UHSRItemEquipmentMappingCatalog>(nullptr,
		TEXT("/Game/Data/Items/DA_ItemEquipmentMappingCatalog_P17.DA_ItemEquipmentMappingCatalog_P17"));
	if (!MappingCatalog)
	{
		UE_LOG(LogTemp, Error, TEXT("HSR Save missing production ItemEquipmentMappingCatalog"));
	}
}

// Deinitialize：反向解除订阅并清理挂起的恢复候选。
void UHSRSaveSubsystem::Deinitialize()
{
	if (Map.IsValid())
	{
		Map->OnArrivalCommitted().RemoveAll(this);
	}
	if (Map.IsValid())
	{
		Map->OnRestoreTravelFailed().RemoveAll(this);
	}
	PendingRestoreCandidate.Reset();
	Super::Deinitialize();
}

// HandleRestoreArrival：地图恢复旅行真正到达目标地图后触发。
// 校验三件事：确有挂起候选、地图子系统有效、到达的地图 ID 与候选一致、请求 ID 与
// 挂起的请求 ID 一致——都满足才把候选真正 LoadSnapshot 落地。
void UHSRSaveSubsystem::HandleRestoreArrival(const FHSRMapArrivalCommitInfo& Info)
{
	if (!PendingRestoreCandidate.IsSet() || !Map.IsValid()
		|| Info.MapId != PendingRestoreCandidate->Map.CurrentLocation.MapId
		|| PendingRestoreRequestId != Map->GetLastCommittedRequestId())
	{
		return;
	}

	bCompletingRestoreTravel = true;
	const EHSRSaveResult Result = LoadSnapshot(PendingRestoreCandidate.GetValue());
	bCompletingRestoreTravel = false;

	PendingRestoreCandidate.Reset();
	PendingRestoreRequestId.Invalidate();
	LastLoadResult.Result = Result;
	LastLoadResult.bRuntimeChanged = Result == EHSRSaveResult::Success;
	LoadCompleted.Broadcast(LastLoadResult);
}

// HandleRestoreTravelFailure：地图恢复旅行失败时触发，直接以 LoadFailed 收尾，
// 不落地任何候选。
void UHSRSaveSubsystem::HandleRestoreTravelFailure(const FGuid& RequestId)
{
	if (!PendingRestoreCandidate.IsSet() || PendingRestoreRequestId != RequestId)
	{
		return;
	}
	PendingRestoreCandidate.Reset();
	PendingRestoreRequestId.Invalidate();
	LastLoadResult.Result = EHSRSaveResult::LoadFailed;
	LastLoadResult.bRuntimeChanged = false;
	LoadCompleted.Broadcast(LastLoadResult);
}

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
// InitializeForDevelopmentTest：开发/自动化测试用的注入式初始化——允许把测试自建的
// 子系统（Equipment/Inventory/Reward/Quest/Map 等）塞进来替换真实子系统，
// 让存档逻辑可以在不依赖完整 GameInstance 的情况下被独立测试。
void UHSRSaveSubsystem::InitializeForDevelopmentTest(UHSRCharacterProfileSubsystem* InProfiles, UHSRPartySubsystem* InParty, UHSREquipmentSubsystem* InEquipment, UHSRInventorySubsystem* InInventory, UHSRRewardSubsystem* InReward, UHSRQuestSubsystem* InQuest, UHSRMapSubsystem* InMap, UHSRItemEquipmentMappingCatalog* InMappingCatalog)
{
	Profiles = InProfiles;
	Party = InParty;
	UGameInstance* OwnerGameInstance = GetGameInstance() ? GetGameInstance() : Cast<UGameInstance>(GetOuter());
	DevelopmentEquipment = InEquipment ? InEquipment : NewObject<UHSREquipmentSubsystem>(OwnerGameInstance);
	Equipment = DevelopmentEquipment;
	DevelopmentInventory = InInventory ? InInventory : NewObject<UHSRInventorySubsystem>(OwnerGameInstance);
	DevelopmentReward = InReward ? InReward : NewObject<UHSRRewardSubsystem>(OwnerGameInstance);
	DevelopmentQuest = InQuest ? InQuest : NewObject<UHSRQuestSubsystem>(OwnerGameInstance);
	DevelopmentMap = InMap ? InMap : NewObject<UHSRMapSubsystem>(OwnerGameInstance);
	DevelopmentChallengeProgression = OwnerGameInstance
		? OwnerGameInstance->GetSubsystem<UHSRChallengeProgressionSubsystem>() : nullptr;
	if (!DevelopmentChallengeProgression && OwnerGameInstance)
	{
		DevelopmentChallengeProgression = NewObject<UHSRChallengeProgressionSubsystem>(OwnerGameInstance);
	}
	Inventory = DevelopmentInventory;
	Reward = DevelopmentReward;
	Quest = DevelopmentQuest;
	Map = DevelopmentMap;
	ChallengeProgression = DevelopmentChallengeProgression;
	MappingCatalog = InMappingCatalog;
	DevelopmentReward->InitializeForDevelopmentTest(DevelopmentInventory);
	DevelopmentQuest->InitializeForDevelopmentTest(DevelopmentReward);
}
#endif

namespace HSRSaveSchemaGates
{
	// 一个地方回答「哪个域在哪个 schema 版本起生效」。Validate、CanPrepareSnapshot、
	// LoadSnapshot 都读这套谓词，因此新增 schema 时只需改这里的常量/谓词，而不是
	// 改三处各自的三元表达式链。数值来自 HSRSaveVersion 的迁移历史：
	//   schema 3 起有背包+奖励；4 起有任务；5 起有地图；7 起用装备 registry（弃用扁平数组）；
	//   8 起有挑战进度。
	constexpr int32 InventoryAndRewardsSince = 3;
	constexpr int32 QuestsSince = 4;
	constexpr int32 MapSince = 5;
	constexpr int32 EquipmentRegistrySince = 7;
	constexpr int32 ChallengeProgressionSince = 8;

	bool HasInventoryAndRewards(int32 SchemaVersion) { return SchemaVersion >= InventoryAndRewardsSince; }
	bool HasQuests(int32 SchemaVersion) { return SchemaVersion >= QuestsSince; }
	bool HasMap(int32 SchemaVersion) { return SchemaVersion >= MapSince; }
	bool UsesEquipmentRegistry(int32 SchemaVersion) { return SchemaVersion >= EquipmentRegistrySince; }
	bool HasChallengeProgression(int32 SchemaVersion) { return SchemaVersion >= ChallengeProgressionSince; }

	/** 旧式扁平装备数组只存在于 schema 2 与 registry 切换（7）之间的版本。 */
	bool HasLegacyEquipmentArray(int32 SchemaVersion)
	{
		return SchemaVersion > 1 && !UsesEquipmentRegistry(SchemaVersion);
	}

	// 各领域的「有效读取器」：当前 schema 不包含该域时，返回一个静态空对象，
	// 而不是让调用方去读它自己遗留的脏字段。这样旧档的缺失域一律按空处理。
	const FHSRInventorySaveData& EffectiveInventory(const FHSRSaveData& Data)
	{
		static const FHSRInventorySaveData Empty;
		return HasInventoryAndRewards(Data.SchemaVersion) ? Data.Inventory : Empty;
	}

	const FHSRRewardSaveData& EffectiveRewards(const FHSRSaveData& Data)
	{
		static const FHSRRewardSaveData Empty;
		return HasInventoryAndRewards(Data.SchemaVersion) ? Data.Rewards : Empty;
	}

	const FHSRQuestSaveData& EffectiveQuests(const FHSRSaveData& Data)
	{
		static const FHSRQuestSaveData Empty;
		return HasQuests(Data.SchemaVersion) ? Data.Quests : Empty;
	}

	const FHSRMapSaveData& EffectiveMap(const FHSRSaveData& Data)
	{
		static const FHSRMapSaveData Empty;
		return HasMap(Data.SchemaVersion) ? Data.Map : Empty;
	}

	const FHSRChallengeProgressionSaveData& EffectiveChallengeProgression(const FHSRSaveData& Data)
	{
		static const FHSRChallengeProgressionSaveData Empty;
		return HasChallengeProgression(Data.SchemaVersion) ? Data.ChallengeProgression : Empty;
	}

	const TArray<FHSREquipmentSaveDto>& EffectiveLegacyEquipment(const FHSRSaveData& Data)
	{
		static const TArray<FHSREquipmentSaveDto> Empty;
		return HasLegacyEquipmentArray(Data.SchemaVersion) ? Data.Equipment : Empty;
	}

	/** 把「给定 schema 未携带的域」清空，用于采纳一份已迁移/恢复的数据后对齐。 */
	void ClearDomainsAbsentAtSchema(FHSRSaveData& Target, int32 SourceSchemaVersion)
	{
		if (!HasInventoryAndRewards(SourceSchemaVersion))
		{
			Target.Inventory = FHSRInventorySaveData();
			Target.Rewards = FHSRRewardSaveData();
		}
		if (!HasQuests(SourceSchemaVersion))
		{
			Target.Quests = FHSRQuestSaveData();
		}
		if (!HasMap(SourceSchemaVersion))
		{
			Target.Map = FHSRMapSaveData();
		}
		if (!HasChallengeProgression(SourceSchemaVersion))
		{
			Target.ChallengeProgression = FHSRChallengeProgressionSaveData();
		}
	}
}

// Validate：对一份存档数据做全量跨域校验。这是保存/加载前的统一守卫——任何域不合法
// 都会被拒绝（返回 false）。校验分几层：
//   1) 基础：schema 在合法区间、队伍版本号非负；
//   2) 队伍宽度：允许「声明的 schema 宽度」或「运行时宽度」二者之一（见下方注释）；
//   3) 角色档案：ID 非空、等级/经验/突破合法、无重复角色、角色 GUID 不冲突、技能等级合法；
//   4) 队伍槽位：无重复角色、槽位里的角色必须已注册；
//   5) 各域「不该存在却存在」的检查（旧 schema 不应携带新域载荷）；
//   6) 装备：registry/placement 一致性（实例唯一、槽位不重复、归属角色匹配 GUID）；
//   7) 背包唯一物品与装备映射目录、定义存在性；
//   8) 跨子系统定义存在性（Profile/Equipment/Inventory/Reward/Quest/Map/挑战）。
bool UHSRSaveSubsystem::Validate(const FHSRSaveData& C) const
{
#if WITH_DEV_AUTOMATION_TESTS
	// 开发测试下用带日志的拒绝函数，方便定位被拒原因。
	const auto Reject = [](const TCHAR* Reason) -> bool
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Save Validate REJECTED reason=%s"), Reason);
		return false;
	};
#else
	// 发布构建下拒绝函数不带日志，仅丢弃原因（避免无谓的日志开销）。
	const auto Reject = [](const TCHAR* Reason) -> bool
	{
		(void)Reason;
		return false;
	};
#endif
	if (C.SchemaVersion < 1 || C.SchemaVersion > HSRSaveVersion::CurrentSchema || C.PartyRevision < 0)
	{
		return Reject(TEXT("schema-or-party-revision"));
	}

	// 两种队伍宽度都合法：直接从磁盘解出的 blob 携带其声明 schema 时代的宽度；
	// 而来自运行时快照的数据即使调用方盖了旧 schema，也总是当前容量（LoadSnapshot
	// 不做迁移，只有 DecodeEnvelope 会迁移）。所以两种都放行。
	const int32 DeclaredWidth = static_cast<int32>(HSRSaveVersion::PartySlotCountForSchema(C.SchemaVersion));
	const int32 RuntimeWidth = static_cast<int32>(HSRSaveVersion::PartySlotCount);
	if (C.PartySlots.Num() != DeclaredWidth && C.PartySlots.Num() != RuntimeWidth)
	{
		return Reject(TEXT("party-width"));
	}

	TSet<FName> Seen;
	TMap<FGuid, FName> GuidOwners;
	for (const FHSRSaveProfileDto& P : C.Profiles)
	{
		const auto& S = P.State;
		if (S.CharacterId.IsNone() || S.Level < 1 || S.Experience < 0 || S.Ascension < 0)
		{
			return Reject(TEXT("profile-state"));
		}
		if (Seen.Contains(S.CharacterId))
		{
			return Reject(TEXT("profile-duplicate"));
		}
		Seen.Add(S.CharacterId);

		// 角色 ID 派生出的 GUID 必须全局唯一——装备摆放/背包实例用这个 GUID 归属角色。
		const FGuid Guid = HSRCharacterGuidFromProfileName(S.CharacterId);
		if (const FName* Owner = GuidOwners.Find(Guid))
		{
			if (*Owner != S.CharacterId)
			{
				return Reject(TEXT("profile-guid-collision"));
			}
		}
		else
		{
			GuidOwners.Add(Guid, S.CharacterId);
		}

		for (const auto& K : S.SkillLevels)
		{
			if (K.Key.IsNone() || K.Value < 0)
			{
				return Reject(TEXT("profile-skill"));
			}
		}
	}

	TSet<FName> PartySeen;
	for (const FHSRPartySlot& Slot : C.PartySlots)
	{
		if (Slot.CharacterId.IsNone())
		{
			continue;
		}
		if (PartySeen.Contains(Slot.CharacterId))
		{
			return Reject(TEXT("party-duplicate"));
		}
		PartySeen.Add(Slot.CharacterId);
		if (!Seen.Contains(Slot.CharacterId))
		{
			return Reject(TEXT("party-unknown-character"));
		}
	}

	// 一个 blob 不得携带其声明 schema 尚未出现的域的载荷。
	if (!HSRSaveSchemaGates::HasLegacyEquipmentArray(C.SchemaVersion) && !C.Equipment.IsEmpty())
	{
		return Reject(TEXT("legacy-equipment-at-schema"));
	}
	if (!HSRSaveSchemaGates::HasInventoryAndRewards(C.SchemaVersion)
		&& (!C.Inventory.Stacks.IsEmpty() || !C.Inventory.UniqueItems.IsEmpty() || C.Inventory.Revision != 0
			|| !C.Rewards.Receipts.IsEmpty() || C.Rewards.Revision != 0))
	{
		return Reject(TEXT("inventory-rewards-at-schema"));
	}
	if (!HSRSaveSchemaGates::HasQuests(C.SchemaVersion) && (!C.Quests.States.IsEmpty() || C.Quests.Revision != 0))
	{
		return Reject(TEXT("quests-at-schema"));
	}
	if (!HSRSaveSchemaGates::HasMap(C.SchemaVersion)
		&& (!C.Map.CurrentLocation.MapId.IsNone() || !C.Map.CurrentLocation.ArrivalId.IsNone()
			|| !C.Map.UnlockedRegionIds.IsEmpty() || !C.Map.UnlockedTeleportIds.IsEmpty()
			|| !C.Map.ExplorationFlags.IsEmpty() || C.Map.Revision != 0))
	{
		return Reject(TEXT("map-at-schema"));
	}

	// 装备域校验（schema >= 7 走 registry/placement，否则走扁平数组）。
	TSet<FGuid> OwnedInstances;
	TMap<FGuid, const FHSREquipmentRegistryDto*> RegistryInstances;
	TSet<FGuid> PlacedInstances;
	if (C.SchemaVersion >= 7)
	{
		for (const auto& D : C.EquipmentRegistry)
		{
			if (!D.InstanceId.IsValid() || OwnedInstances.Contains(D.InstanceId))
			{
				return Reject(TEXT("equipment-registry-instance"));
			}
			OwnedInstances.Add(D.InstanceId);
			RegistryInstances.Add(D.InstanceId, &D);
		}
		TSet<FString> Slots;
		for (const auto& D : C.EquipmentPlacements)
		{
			const FName* Owner = GuidOwners.Find(D.CharacterId);
			const FString Slot = FString::Printf(TEXT("%s:%d:%d"), *D.CharacterId.ToString(), D.Kind, D.Slot);
			if (!Owner || HSRCharacterGuidFromProfileName(*Owner) != D.CharacterId
				|| !OwnedInstances.Contains(D.InstanceId)
				|| PlacedInstances.Contains(D.InstanceId) || Slots.Contains(Slot))
			{
				return Reject(TEXT("equipment-placement"));
			}
			PlacedInstances.Add(D.InstanceId);
			Slots.Add(Slot);
		}
	}
	else
	{
		for (const FHSREquipmentSaveDto& D : C.Equipment)
		{
			const FName* Owner = GuidOwners.Find(D.CharacterId);
			if (!Owner || HSRCharacterGuidFromProfileName(*Owner) != D.CharacterId
				|| !D.InstanceId.IsValid() || OwnedInstances.Contains(D.InstanceId))
			{
				return Reject(TEXT("legacy-equipment-row"));
			}
			OwnedInstances.Add(D.InstanceId);
		}
	}

	// 背包唯一物品：实例 ID 必须唯一，且不能与已装备实例重复；若该物品可映射成装备，
	// 其定义必须与映射目录一致（inventory-equipment-mapping）。
	TSet<FGuid> InventoryInstances;
	for (const FHSRItemInstance& I : C.Inventory.UniqueItems)
	{
		if (!I.InstanceId.IsValid() || InventoryInstances.Contains(I.InstanceId)
			|| (C.SchemaVersion >= 7 ? PlacedInstances.Contains(I.InstanceId) : OwnedInstances.Contains(I.InstanceId)))
		{
			return Reject(TEXT("inventory-instance"));
		}
		InventoryInstances.Add(I.InstanceId);
		if (const FHSREquipmentRegistryDto* const* Registry = RegistryInstances.Find(I.InstanceId))
		{
			FHSRItemEquipmentMappingEntry Mapping;
			if (!MappingCatalog || !MappingCatalog->Resolve(I.DefinitionId, Mapping)
				|| Mapping.EquipmentDefinitionId != (*Registry)->DefinitionId
				|| static_cast<int32>(Mapping.Kind) != (*Registry)->Kind
				|| !Equipment.IsValid()
				|| !Equipment->IsDefinitionCompatible(Mapping.EquipmentDefinitionId, Mapping.Kind, Mapping.Slot))
			{
				return Reject(TEXT("inventory-equipment-mapping"));
			}
		}
	}

	// 只读的跨域预检：必须在任何域的 PrepareRestore 或装备投影之前完成。
	if (!Profiles.IsValid() || !Equipment.IsValid() || !Inventory.IsValid()
		|| !Reward.IsValid() || !Quest.IsValid() || !Map.IsValid())
	{
		return Reject(TEXT("subsystem-missing"));
	}

	for (const FHSRSaveProfileDto& P : C.Profiles)
	{
		if (!Profiles->HasDefinition(P.State.CharacterId))
		{
			return Reject(TEXT("profile-definition"));
		}
	}
	if (C.SchemaVersion >= 7)
	{
		for (const auto& D : C.EquipmentRegistry)
		{
			if (!Equipment->HasDefinition(D.DefinitionId))
			{
				return Reject(TEXT("equipment-definition"));
			}
		}
	}
	else
	{
		for (const FHSREquipmentSaveDto& D : C.Equipment)
		{
			if (!Equipment->HasDefinition(D.DefinitionId))
			{
				return Reject(TEXT("legacy-equipment-definition"));
			}
		}
	}
	for (const FHSRItemStackSnapshot& S : C.Inventory.Stacks)
	{
		if (!Inventory->HasDefinition(S.ItemId))
		{
			return Reject(TEXT("stack-definition"));
		}
	}
	for (const FHSRItemInstance& I : C.Inventory.UniqueItems)
	{
		if (!Inventory->HasDefinition(I.DefinitionId))
		{
			return Reject(TEXT("unique-definition"));
		}
	}
	for (const FHSRRewardReceipt& R : C.Rewards.Receipts)
	{
		if (!Reward->HasDefinition(R.Request.RewardDefinitionId))
		{
			return Reject(TEXT("reward-definition"));
		}
		for (const FHSRInventoryGrant& G : R.Grants)
		{
			if (!Inventory->HasDefinition(G.ItemId))
			{
				return Reject(TEXT("reward-grant-definition"));
			}
		}
	}
	for (const FHSRQuestRuntimeState& Q : C.Quests.States)
	{
		if (!Quest->HasDefinition(Q.QuestId))
		{
			return Reject(TEXT("quest-definition"));
		}
	}
	if (!C.Map.CurrentLocation.MapId.IsNone() && !Map->HasMapDefinition(C.Map.CurrentLocation.MapId))
	{
		return Reject(TEXT("map-definition"));
	}
	for (const FName& R : C.Map.UnlockedRegionIds)
	{
		if (!Map->HasRegionDefinition(R))
		{
			return Reject(TEXT("region-definition"));
		}
	}
	for (const FName& T : C.Map.UnlockedTeleportIds)
	{
		if (!Map->HasTeleportDefinition(T))
		{
			return Reject(TEXT("teleport-definition"));
		}
	}
	if (!UHSRChallengeProgressionSubsystem::ValidateSaveData(C.ChallengeProgression))
	{
		return Reject(TEXT("challenge-progression"));
	}
	return true;
}

// CanPrepareSnapshot：判断一份候选存档能否被完整「预演恢复」——即所有域的
// PrepareRestore 都通过。这比 Validate 更进一步：Validate 只查静态一致性，
// 这里实际调用各子系统的 PrepareRestore 干跑一遍（不写运行时）。
bool UHSRSaveSubsystem::CanPrepareSnapshot(const FHSRSaveData& Candidate) const
{
	if (Candidate.SchemaVersion < 1 || Candidate.SchemaVersion > HSRSaveVersion::CurrentSchema
		|| !Profiles.IsValid() || !Party.IsValid() || !Equipment.IsValid() || !Inventory.IsValid()
		|| !Reward.IsValid() || !Quest.IsValid() || !Map.IsValid() || !ChallengeProgression.IsValid()
		|| !Validate(Candidate))
	{
#if WITH_DEV_AUTOMATION_TESTS
		UE_LOG(LogTemp, Warning, TEXT("HSR Save prepare rejected by prerequisites/validation"));
#endif
		return false;
	}

	// 把候选档案转成各子系统期望的输入形态。
	TArray<FHSRCharacterProfileSnapshot> SavedProfiles;
	for (const FHSRSaveProfileDto& D : Candidate.Profiles)
	{
		FHSRCharacterProfileSnapshot P;
		P.RuntimeState = D.State;
		P.RuntimeRevision = D.RuntimeRevision;
		SavedProfiles.Add(MoveTemp(P));
	}

	TMap<FName, FHSRCharacterProfileSnapshot> ProfileCandidate;
	FHSRPartySnapshot PartySaved;
	PartySaved.Slots = Candidate.PartySlots;
	PartySaved.Revision = Candidate.PartyRevision;
	FHSRPartySnapshot PartyCandidate;
	FHSREquipmentRestoreMap EquipmentCandidate;
	FHSREquipmentRegistryRestoreState RegistryCandidate;
	FHSRInventoryRestoreState InventoryCandidate;
	FHSRRewardRestoreState RewardCandidate;
	FHSRQuestRestoreState QuestCandidate;
	FHSRMapRuntimeSnapshot MapCandidate;
	FHSRChallengeProgressionSaveData ChallengeProgressionCandidate;

	const bool bProfiles = Profiles->PrepareRestore(SavedProfiles, ProfileCandidate);
	const bool bParty = Party->PrepareRestore(PartySaved, PartyCandidate);
	const bool bEquipment = HSRSaveSchemaGates::UsesEquipmentRegistry(Candidate.SchemaVersion)
		? Equipment->PrepareRestore(Candidate.EquipmentRegistry, Candidate.EquipmentPlacements, RegistryCandidate)
		: Equipment->PrepareRestore(HSRSaveSchemaGates::EffectiveLegacyEquipment(Candidate), EquipmentCandidate);
	const bool bInventory = Inventory->PrepareRestore(HSRSaveSchemaGates::EffectiveInventory(Candidate), InventoryCandidate);
	const bool bReward = Reward->PrepareRestore(HSRSaveSchemaGates::EffectiveRewards(Candidate), RewardCandidate);
	const bool bQuest = Quest->PrepareRestore(HSRSaveSchemaGates::EffectiveQuests(Candidate), QuestCandidate);
	const bool bMap = Map->PrepareRestore(HSRSaveSchemaGates::EffectiveMap(Candidate), MapCandidate);
	const bool bChallengeProgression = ChallengeProgression->PrepareRestore(
		HSRSaveSchemaGates::EffectiveChallengeProgression(Candidate), ChallengeProgressionCandidate);

#if WITH_DEV_AUTOMATION_TESTS
	if (!(bProfiles && bParty && bEquipment && bInventory && bReward && bQuest && bMap && bChallengeProgression))
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR Save prepare rejected Profiles=%d Party=%d Equipment=%d Inventory=%d Reward=%d Quest=%d Map=%d Challenge=%d"),
			bProfiles, bParty, bEquipment, bInventory, bReward, bQuest, bMap, bChallengeProgression);
	}
#endif
	return bProfiles && bParty && bEquipment && bInventory && bReward && bQuest && bMap && bChallengeProgression;
}

// GetSlotSummary：只读地汇总一个槽位的状态，供 UI（存档/读档界面）显示。
// 它不修改任何运行时状态，只报告：
//   - 主槽/备份槽/暂存槽是否存在；
//   - 主槽是否可信（能否解出合法的 SaveId）；
//   - 主槽与备份槽的解码/校验结果；
//   - 当主槽损坏但备份血缘有效时，标记为 Recoverable；
//   - 否则标记 Ready / Empty / Unavailable，并给出对应的失败原因码。
bool UHSRSaveSubsystem::GetSlotSummary(const FString& SlotName, const int32 UserIndex, FHSRSaveSlotSummary& OutSummary) const
{
	OutSummary = FHSRSaveSlotSummary();
	OutSummary.SlotName = SlotName;

	// 槽位名合法性：非法名（含保留前缀）直接判定不可用。
	if (!HSRSaveVersion::IsValidSlot(SlotName, UserIndex) || SlotName.Contains(TEXT(".__hsr_")))
	{
		OutSummary.State = EHSRSaveSlotState::Unavailable;
		OutSummary.Result = EHSRSaveResult::InvalidArgument;
		return false;
	}

	// 三个物理槽位：主槽、备份槽（保存时被顶替的上一版）、暂存槽（写入中的中间产物）。
	const FString BackupSlot = SlotName + TEXT(".__hsr_backup_v1");
	const FString StagingSlot = SlotName + TEXT(".__hsr_staging_v1");
	OutSummary.bPrimaryPresent = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
	OutSummary.bBackupPresent = UGameplayStatics::DoesSaveGameExist(BackupSlot, UserIndex);
	const bool bStagingPresent = UGameplayStatics::DoesSaveGameExist(StagingSlot, UserIndex);

	// 三个槽都不存在 => 空槽。
	if (!OutSummary.bPrimaryPresent && !OutSummary.bBackupPresent && !bStagingPresent)
	{
		OutSummary.State = EHSRSaveSlotState::Empty;
		OutSummary.Result = EHSRSaveResult::SlotNotFound;
		return true;
	}

	FHSRSaveData PrimaryData;
	FHSRSaveData BackupData;
	FHSRSaveEnvelopeHeader PrimaryHeader;
	FHSRSaveEnvelopeHeader BackupHeader;
	EHSRSaveDecodeResult PrimaryDecode = EHSRSaveDecodeResult::TooShort;
	EHSRSaveDecodeResult BackupDecode = EHSRSaveDecodeResult::TooShort;
	EHSRSaveResult PrimaryFailure = EHSRSaveResult::InvalidEnvelope;
	EHSRSaveResult BackupFailure = EHSRSaveResult::InvalidEnvelope;
	bool bPrimaryCanonical = false;
	bool bBackupCanonical = false;
	bool bPrimaryValid = false;
	bool bBackupValid = false;

	// 把解码错误码映射成对外暴露的存档结果码。
	auto DecodeFailureToSaveResult = [](const EHSRSaveDecodeResult Reason)
	{
		switch (Reason)
		{
		case EHSRSaveDecodeResult::ChecksumMismatch:
			return EHSRSaveResult::IntegrityFailed;
		case EHSRSaveDecodeResult::FutureSchema:
		case EHSRSaveDecodeResult::TooOld:
		case EHSRSaveDecodeResult::UnsupportedFormat:
			return EHSRSaveResult::UnsupportedSchema;
		case EHSRSaveDecodeResult::SlotMismatch:
			return EHSRSaveResult::SlotIdentityMismatch;
		default:
			return EHSRSaveResult::InvalidEnvelope;
		}
	};

	// 主槽：能解出信封就解码+校验；若是 BadMagic（非本格式），尝试旧的 USaveGame
	// 兼容路径（仅 schema <= 5 或 >= 8 的旧对象序列化数据可接受）。
	if (OutSummary.bPrimaryPresent)
	{
		TArray<uint8> PrimaryBytes;
		if (UGameplayStatics::LoadDataFromSlot(PrimaryBytes, SlotName, UserIndex))
		{
			PrimaryDecode = HSRSaveVersion::DecodeEnvelope(PrimaryBytes, SlotName, UserIndex, PrimaryData, &PrimaryHeader);
			OutSummary.bPrimaryTrusted = PrimaryHeader.SaveId.IsValid();
			if (PrimaryDecode == EHSRSaveDecodeResult::Success)
			{
				bPrimaryCanonical = true;
				bPrimaryValid = Validate(PrimaryData);
				PrimaryFailure = bPrimaryValid ? EHSRSaveResult::Success : EHSRSaveResult::InvalidData;
			}
			else if (PrimaryDecode == EHSRSaveDecodeResult::BadMagic)
			{
				USaveGame* LegacyObject = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
				const UHSRSaveGame* Legacy = Cast<UHSRSaveGame>(LegacyObject);
				const bool bSupportedLegacy = Legacy && (Legacy->Data.SchemaVersion <= 5 || Legacy->Data.SchemaVersion >= 8);
				if (bSupportedLegacy)
				{
					PrimaryData = Legacy->Data;
					bPrimaryValid = Validate(PrimaryData);
					PrimaryFailure = bPrimaryValid ? EHSRSaveResult::Success : EHSRSaveResult::InvalidData;
				}
				else
				{
					PrimaryFailure = !LegacyObject ? EHSRSaveResult::LoadFailed
						: !Legacy ? EHSRSaveResult::ClassMismatch : EHSRSaveResult::UnsupportedSchema;
				}
			}
			else
			{
				PrimaryFailure = DecodeFailureToSaveResult(PrimaryDecode);
			}
		}
		else
		{
			PrimaryFailure = EHSRSaveResult::LoadFailed;
		}
	}

	// 备份槽：同样解码+校验（不尝试旧兼容路径——备份只可能是本格式写入的）。
	if (OutSummary.bBackupPresent)
	{
		TArray<uint8> BackupBytes;
		if (UGameplayStatics::LoadDataFromSlot(BackupBytes, BackupSlot, UserIndex))
		{
			BackupDecode = HSRSaveVersion::DecodeEnvelope(BackupBytes, SlotName, UserIndex, BackupData, &BackupHeader);
			if (BackupDecode == EHSRSaveDecodeResult::Success)
			{
				bBackupCanonical = true;
				bBackupValid = Validate(BackupData);
				BackupFailure = bBackupValid ? EHSRSaveResult::Success : EHSRSaveResult::InvalidData;
			}
			else
			{
				BackupFailure = DecodeFailureToSaveResult(BackupDecode);
			}
		}
		else
		{
			BackupFailure = EHSRSaveResult::LoadFailed;
		}
	}

	// 备份血缘判定：只有当主槽可信（能解出 SaveId）时，才要求备份与主槽同 SaveId 且
	// 代数更小——这是「备份是主槽的上一版」的凭证。
	const bool bPrimarySelected = bPrimaryValid;
	bool bBackupLineageValid = bBackupValid;
	if (bBackupLineageValid && OutSummary.bPrimaryTrusted)
	{
		bBackupLineageValid = bPrimaryCanonical
			&& BackupHeader.SaveId == PrimaryHeader.SaveId
			&& BackupHeader.Generation < PrimaryHeader.Generation;
	}

	auto CountPartyMembers = [](const TArray<FHSRPartySlot>& Slots)
	{
		int32 Count = 0;
		for (const FHSRPartySlot& Slot : Slots)
		{
			if (!Slot.CharacterId.IsNone())
			{
				++Count;
			}
		}
		return Count;
	};
	// ProjectSummary：把一份数据与头信息投影成对外摘要（代、时间戳、地图、队伍人数等）。
	auto ProjectSummary = [&](const FHSRSaveData& Data, const FHSRSaveEnvelopeHeader* Header, const bool bRecovered)
	{
		OutSummary.Result = EHSRSaveResult::Success;
		OutSummary.Generation = Header ? static_cast<int64>(Header->Generation) : 0;
		OutSummary.UtcUnixMilliseconds = Header ? Header->UtcUnixMilliseconds : 0;
		OutSummary.MapId = Data.Map.CurrentLocation.MapId;
		OutSummary.PartyMemberCount = CountPartyMembers(Data.PartySlots);
		OutSummary.CompletedChallengeCount = Data.ChallengeProgression.CompletedEncounterIds.Num();
		OutSummary.bRecoveredFromBackup = bRecovered;
	};

	// 主槽不可用但备份血缘有效 => 可恢复（从备份恢复）。
	if (!bPrimarySelected && bBackupLineageValid)
	{
		OutSummary.State = EHSRSaveSlotState::Recoverable;
		ProjectSummary(BackupData, bBackupCanonical ? &BackupHeader : nullptr, true);
		return true;
	}
	// 主槽可用 => Ready。
	if (bPrimarySelected)
	{
		OutSummary.State = EHSRSaveSlotState::Ready;
		ProjectSummary(PrimaryData, bPrimaryCanonical ? &PrimaryHeader : nullptr, false);
		return true;
	}

	// 两者都不可用 => Unavailable，附上主/备各自的失败原因。
	OutSummary.State = EHSRSaveSlotState::Unavailable;
	OutSummary.Result = OutSummary.bPrimaryPresent ? PrimaryFailure : BackupFailure;
	if (OutSummary.Result == EHSRSaveResult::Success)
	{
		// 有槽位但内容无效：若暂存槽存在说明上次保存中断，否则视为无效信封。
		OutSummary.Result = bStagingPresent ? EHSRSaveResult::SaveFailed : EHSRSaveResult::InvalidEnvelope;
	}
	return true;
}

// SaveSnapshot：把当前运行时各领域子系统的状态捕获成一份 FHSRSaveData 快照。
// 数据流：Profiles/Party/Equipment/Inventory/Reward/Quest/Map/挑战 各自导出 ->
// 汇总到 Captured -> 整体 Validate 通过后才写入 Current 并返回给调用方。
EHSRSaveResult UHSRSaveSubsystem::SaveSnapshot(FHSRSaveData& Out)
{
	if (!Party.IsValid() || !Profiles.IsValid() || !Equipment.IsValid() || !Inventory.IsValid()
		|| !Reward.IsValid() || !Quest.IsValid() || !Map.IsValid() || !ChallengeProgression.IsValid())
	{
		return EHSRSaveResult::InvalidData;
	}

	FHSRSaveData Captured;
	Captured.SchemaVersion = HSRSaveVersion::CurrentSchema;

	// 角色档案：把快照逐个转成存档 DTO。
	TArray<FHSRCharacterProfileSnapshot> P;
	Profiles->ExportProfiles(P);
	for (const auto& Entry : P)
	{
		FHSRSaveProfileDto D;
		D.State = Entry.RuntimeState;
		D.RuntimeRevision = Entry.RuntimeRevision;
		Captured.Profiles.Add(MoveTemp(D));
	}

	// 队伍。
	FHSRPartySnapshot PS;
	Party->GetSnapshot(PS);
	Captured.PartySlots = PS.Slots;
	Captured.PartyRevision = PS.Revision;

	// 装备（registry + placement）、背包、奖励、任务、地图、挑战进度。
	Equipment->ExportSaveData(Captured.EquipmentRegistry, Captured.EquipmentPlacements);
	Inventory->ExportSaveData(Captured.Inventory);
	Reward->ExportSaveData(Captured.Rewards);
	Quest->ExportSaveData(Captured.Quests);
	Map->ExportSaveData(Captured.Map);
	ChallengeProgression->ExportSaveData(Captured.ChallengeProgression);

	// 导出结果必须整体通过校验，否则这份快照不该被使用。
	if (!Validate(Captured))
	{
		return EHSRSaveResult::InvalidData;
	}

	Current = Captured;
	Out = Captured;
	return EHSRSaveResult::Success;
}

// LoadSnapshot：把一份候选存档完整恢复到运行时。
// 流程（分阶段）：
//   1) 前置：schema 边界（明确拒绝 schema 6——它是迁移中间态）、各子系统有效、Validate 通过；
//   2) 预演：各子系统的 PrepareRestore 干跑，产出候选状态；
//   3) 装备投影：装备子系统 ProjectRestore（需要把 registry/placement 合成为 Loadout）；
//   4) 变更检测：计算哪些角色/队伍/装备/背包/奖励/任务/地图/挑战 相对当前发生了变化；
//   5) 地图恢复旅行：若存档位置不在当前地图，发起跨图旅行并挂起（见 HandleRestoreArrival）；
//   6) 提交：各子系统 CommitRestore，更新 Current、补 schema 缺失域、广播变更。
EHSRSaveResult UHSRSaveSubsystem::LoadSnapshot(const FHSRSaveData& Candidate)
{
	if (Candidate.SchemaVersion < 1 || Candidate.SchemaVersion > HSRSaveVersion::CurrentSchema
		|| Candidate.SchemaVersion == 6)
	{
		return EHSRSaveResult::UnsupportedSchema;
	}
	if (!Profiles.IsValid() || !Party.IsValid() || !Equipment.IsValid() || !Inventory.IsValid()
		|| !Reward.IsValid() || !Quest.IsValid() || !Map.IsValid() || !ChallengeProgression.IsValid()
		|| !Validate(Candidate))
	{
		return EHSRSaveResult::InvalidData;
	}

	// 阶段 2：把候选档案转成各子系统期望的输入形态。
	TArray<FHSRCharacterProfileSnapshot> SavedProfiles;
	for (const auto& D : Candidate.Profiles)
	{
		FHSRCharacterProfileSnapshot P;
		P.RuntimeState = D.State;
		P.RuntimeRevision = D.RuntimeRevision;
		SavedProfiles.Add(MoveTemp(P));
	}

	TMap<FName, FHSRCharacterProfileSnapshot> ProfileCandidate;
	FHSRPartySnapshot PartySaved;
	PartySaved.Slots = Candidate.PartySlots;
	PartySaved.Revision = Candidate.PartyRevision;
	FHSRPartySnapshot PartyCandidate;

	TMap<FGuid, FHSREquipmentRestoreState> EquipmentCandidate;
	FHSREquipmentRegistryRestoreState RegistryCandidate;
	FHSRInventoryRestoreState InventoryCandidate;
	FHSRRewardRestoreState RewardCandidate;
	FHSRQuestRestoreState QuestCandidate;
	FHSRMapRuntimeSnapshot MapCandidate;
	FHSRChallengeProgressionSaveData ChallengeProgressionCandidate;

	// 装备预演：schema >= 7 用 registry/placement 形式；旧 schema 用扁平数组形式。
	const bool bEquipmentPrepared = HSRSaveSchemaGates::UsesEquipmentRegistry(Candidate.SchemaVersion)
		? Equipment->PrepareRestore(Candidate.EquipmentRegistry, Candidate.EquipmentPlacements, RegistryCandidate)
		: Equipment->PrepareRestore(HSRSaveSchemaGates::EffectiveLegacyEquipment(Candidate), EquipmentCandidate);
	const bool bUsesRegistry = HSRSaveSchemaGates::UsesEquipmentRegistry(Candidate.SchemaVersion);
	if (bUsesRegistry && bEquipmentPrepared)
	{
		EquipmentCandidate = RegistryCandidate.Loadouts;
	}

	const bool bChallengeProgressionPrepared = ChallengeProgression->PrepareRestore(
		HSRSaveSchemaGates::EffectiveChallengeProgression(Candidate), ChallengeProgressionCandidate);

	// 其余领域预演：任何一个失败即整体拒绝（不写任何运行时）。
	if (!Profiles->PrepareRestore(SavedProfiles, ProfileCandidate)
		|| !Party->PrepareRestore(PartySaved, PartyCandidate)
		|| !bEquipmentPrepared
		|| !Inventory->PrepareRestore(HSRSaveSchemaGates::EffectiveInventory(Candidate), InventoryCandidate)
		|| !Reward->PrepareRestore(HSRSaveSchemaGates::EffectiveRewards(Candidate), RewardCandidate)
		|| !Quest->PrepareRestore(HSRSaveSchemaGates::EffectiveQuests(Candidate), QuestCandidate)
		|| !Map->PrepareRestore(HSRSaveSchemaGates::EffectiveMap(Candidate), MapCandidate)
		|| !bChallengeProgressionPrepared)
	{
		return EHSRSaveResult::InvalidData;
	}

	// 阶段 3：装备投影——把候选的 Loadout 正式合成为可安装的形态。
	const FHSREquipmentRestoreMap& ProjectedEquipment = bUsesRegistry ? RegistryCandidate.Loadouts : EquipmentCandidate;
	if (!Equipment->ProjectRestore(ProjectedEquipment))
	{
		return EHSRSaveResult::InvalidData;
	}

	// 阶段 4：变更检测。角色档案逐项比较状态字段；队伍比较版本与槽位；装备比较
	// 各角色的装备行（含词条）；其余领域用各子系统自己的 IsRestoreDifferent。
	TArray<FName> ChangedIds;
	for (const auto& It : ProfileCandidate)
	{
		FHSRCharacterProfileSnapshot Old;
		if (!Profiles->GetProfileSnapshot(It.Key, Old)
			|| Old.RuntimeRevision != It.Value.RuntimeRevision
			|| Old.RuntimeState.Level != It.Value.RuntimeState.Level
			|| Old.RuntimeState.Experience != It.Value.RuntimeState.Experience
			|| Old.RuntimeState.Ascension != It.Value.RuntimeState.Ascension
			|| !Old.RuntimeState.SkillLevels.OrderIndependentCompareEqual(It.Value.RuntimeState.SkillLevels))
		{
			ChangedIds.Add(It.Key);
		}
	}
	ChangedIds.Sort(FNameLexicalLess());

	FHSRPartySnapshot OldParty;
	Party->GetSnapshot(OldParty);
	bool PartyChanged = OldParty.Revision != PartyCandidate.Revision;
	for (int32 I = 0; !PartyChanged && I < OldParty.Slots.Num(); ++I)
	{
		PartyChanged = OldParty.Slots[I].CharacterId != PartyCandidate.Slots[I].CharacterId;
	}

	// 把候选装备行还原成扁平 DTO 列表（registry/placement 已在此前展开），用于和现有装备比对。
	TArray<FHSREquipmentSaveDto> CandidateEquipmentRows = Candidate.Equipment;
	if (Candidate.SchemaVersion >= 7)
	{
		for (const auto& Placement : Candidate.EquipmentPlacements)
		{
			for (const auto& Registry : Candidate.EquipmentRegistry)
			{
				if (Registry.InstanceId == Placement.InstanceId)
				{
					FHSREquipmentSaveDto D;
					D.DefinitionId = Registry.DefinitionId;
					D.InstanceId = Registry.InstanceId;
					D.CharacterId = Placement.CharacterId;
					D.Kind = Placement.Kind;
					D.Slot = Placement.Slot;
					D.EnhancementLevel = Registry.EnhancementLevel;
					D.Modifiers = Registry.Modifiers;
					D.SetId = Registry.SetId;
					D.AuthorityRevision = Placement.AuthorityRevision;
					CandidateEquipmentRows.Add(MoveTemp(D));
					break;
				}
			}
		}
	}

	// 逐角色比对装备：先收集「现有角色被整体删除」的（候选里没有该角色），
	// 再对每个候选角色按行（InstanceId 排序后）逐一比对各字段。
	TSet<FGuid> EquipmentChanged;
	TArray<FHSREquipmentSaveDto> ExistingEquipment;
	Equipment->ExportSaveData(ExistingEquipment);
	TSet<FGuid> ExistingCharacters;
	for (const FHSREquipmentSaveDto& D : ExistingEquipment)
	{
		ExistingCharacters.Add(D.CharacterId);
	}
	for (const FGuid& Id : ExistingCharacters)
	{
		if (!EquipmentCandidate.Contains(Id))
		{
			EquipmentChanged.Add(Id);
		}
	}
	for (const auto& P : EquipmentCandidate)
	{
		TArray<FHSREquipmentSaveDto> OldRows;
		for (const FHSREquipmentSaveDto& D : ExistingEquipment)
		{
			if (D.CharacterId == P.Key)
			{
				OldRows.Add(D);
			}
		}
		TArray<FHSREquipmentSaveDto> NewRows;
		for (const FHSREquipmentSaveDto& D : CandidateEquipmentRows)
		{
			if (D.CharacterId == P.Key)
			{
				NewRows.Add(D);
			}
		}
		if (OldRows.Num() != NewRows.Num())
		{
			EquipmentChanged.Add(P.Key);
			continue;
		}
		OldRows.Sort([](const auto& A, const auto& B) { return A.InstanceId < B.InstanceId; });
		NewRows.Sort([](const auto& A, const auto& B) { return A.InstanceId < B.InstanceId; });
		for (int32 I = 0; I < OldRows.Num(); ++I)
		{
			bool bDifferent = OldRows[I].InstanceId != NewRows[I].InstanceId
				|| OldRows[I].DefinitionId != NewRows[I].DefinitionId
				|| OldRows[I].Kind != NewRows[I].Kind
				|| OldRows[I].Slot != NewRows[I].Slot
				|| OldRows[I].EnhancementLevel != NewRows[I].EnhancementLevel
				|| OldRows[I].AuthorityRevision != NewRows[I].AuthorityRevision
				|| OldRows[I].SetId != NewRows[I].SetId
				|| OldRows[I].Modifiers.Num() != NewRows[I].Modifiers.Num();
			for (int32 M = 0; !bDifferent && M < OldRows[I].Modifiers.Num(); ++M)
			{
				bDifferent = OldRows[I].Modifiers[M].Stat != NewRows[I].Modifiers[M].Stat
					|| OldRows[I].Modifiers[M].Value != NewRows[I].Modifiers[M].Value;
			}
			if (bDifferent)
			{
				EquipmentChanged.Add(P.Key);
				break;
			}
		}
	}

	// 阶段 5：地图恢复旅行。存档位置可能不在当前地图，需要先旅行到目标地图。
	const EHSRMapOperationResult RestoreTravelResult = Map->RequestRestoreTravel(MapCandidate);
	if (RestoreTravelResult != EHSRMapOperationResult::Success && RestoreTravelResult != EHSRMapOperationResult::NoOp)
	{
		return EHSRSaveResult::InvalidData;
	}
	// 旅行成功且当前不是「旅行完成回调」中 => 挂起候选，等 HandleRestoreArrival 落地。
	if (RestoreTravelResult == EHSRMapOperationResult::Success && !bCompletingRestoreTravel)
	{
		FHSRTeleportRequest PendingRequest;
		if (!Map->GetPendingRequest(PendingRequest) || PendingRequest.TeleportId != TEXT("Save.Restore"))
		{
			return EHSRSaveResult::InvalidData;
		}
		PendingRestoreCandidate = Candidate;
		PendingRestoreRequestId = PendingRequest.RequestId;
		return EHSRSaveResult::Success;
	}
	// NoOp（无需旅行）=> 直接应用存档位置。
	if (RestoreTravelResult == EHSRMapOperationResult::NoOp)
	{
		const EHSRMapOperationResult PlacementResult = Map->ApplyRestoreLocation(MapCandidate);
		if (PlacementResult != EHSRMapOperationResult::Success && PlacementResult != EHSRMapOperationResult::NoOp)
		{
			return EHSRSaveResult::InvalidData;
		}
	}

	// 其余领域的变更检测。
	const bool bInventoryChanged = Inventory->IsRestoreDifferent(InventoryCandidate);
	const bool bRewardsChanged = Reward->IsRestoreDifferent(RewardCandidate);
	const bool bQuestsChanged = Quest->IsRestoreDifferent(QuestCandidate);
	const bool bMapChanged = Map->IsRestoreDifferent(MapCandidate);
	const bool bChallengeProgressionChanged = ChallengeProgression->IsRestoreDifferent(ChallengeProgressionCandidate);

	// 阶段 6：提交。角色/队伍静默提交（变更广播由 NotifyRestored 统一做）；
	// 其余领域按是否变更决定是否广播。
	Profiles->CommitRestoreSilent(MoveTemp(ProfileCandidate));
	Party->CommitRestoreSilent(MoveTemp(PartyCandidate));
	if (bUsesRegistry)
	{
		Equipment->CommitRestore(RegistryCandidate);
	}
	else
	{
		Equipment->CommitRestore(EquipmentCandidate);
	}
	Inventory->CommitRestore(MoveTemp(InventoryCandidate), false);
	Reward->CommitRestore(MoveTemp(RewardCandidate), false);
	Quest->CommitRestore(MoveTemp(QuestCandidate), false);
	Map->CommitRestore(MoveTemp(MapCandidate), false);
	ChallengeProgression->CommitRestore(MoveTemp(ChallengeProgressionCandidate), bChallengeProgressionChanged);

	// 更新 Current：盖上当前 schema、补齐队伍宽度、清掉旧 schema 缺失的域。
	Current = Candidate;
	Current.SchemaVersion = HSRSaveVersion::CurrentSchema;
	if (Current.PartySlots.Num() < static_cast<int32>(HSRSaveVersion::PartySlotCount))
	{
		Current.PartySlots.SetNum(HSRSaveVersion::PartySlotCount);
	}
	HSRSaveSchemaGates::ClearDomainsAbsentAtSchema(Current, Candidate.SchemaVersion);

	// 广播变更：仅通知实际变化的领域，减少 UI/战斗侧无谓刷新。
	Profiles->NotifyRestored(ChangedIds);
	if (PartyChanged)
	{
		Party->NotifyRestored();
	}
	Equipment->NotifyRestored(EquipmentChanged);
	if (bInventoryChanged)
	{
		Inventory->OnInventoryChanged().Broadcast(Current.Inventory.Revision);
	}
	if (bRewardsChanged)
	{
		Reward->OnRewardRestored().Broadcast(Current.Rewards.Revision);
	}
	if (bQuestsChanged)
	{
		Quest->OnQuestRestored().Broadcast(Current.Quests.Revision);
	}
	if (bMapChanged)
	{
		Map->OnMapStateChanged().Broadcast(Map->GetSnapshot());
	}
	if (!ChangedIds.IsEmpty() || PartyChanged || !EquipmentChanged.IsEmpty()
		|| bInventoryChanged || bRewardsChanged || bQuestsChanged || bMapChanged || bChallengeProgressionChanged)
	{
		FHSRRestoreCommitInfo Info;
		Info.ChangedCharacterIds = ChangedIds;
		Info.bPartyChanged = PartyChanged;
		Info.bInventoryChanged = bInventoryChanged;
		Info.bRewardsChanged = bRewardsChanged;
		Info.bQuestsChanged = bQuestsChanged;
		Info.bMapChanged = bMapChanged;
		Info.bChallengeProgressionChanged = bChallengeProgressionChanged;
		Info.TransactionRevision = ++RestoreTransactionRevision;
		RestoreCommitted.Broadcast(Info);
	}
	return EHSRSaveResult::Success;
}

// SaveToSlot：把当前运行时快照以「三段式事务」写入磁盘槽位：
//   1) 写入暂存槽（Staging）并回读校验（字节一致 + 信封头一致）；
//   2) 若主槽原本有效，把主槽旧数据备份到备份槽（Backup）并回读校验；
//   3) 把新数据写入主槽并回读校验，最后清理暂存槽。
// 任意一步失败都会走 Fail 回滚：把 Current 恢复为写入前的快照，并记录失败阶段。
// SaveId 首次保存时生成，之后沿用旧 SaveId 并递增 Generation，供备份血缘判定。
EHSRSaveResult UHSRSaveSubsystem::SaveToSlot(const FString& SlotName, int32 UserIndex)
{
	// 记录本趟写入的失败阶段；HEADER 在成功校验后才被填充。
	LastWriteFailureStage = EHSRSaveFailureStage::None;
	bLastWriteCleanupWarning = false;
	LastWriteHeader = FHSRSaveEnvelopeHeader();

	// 槽位名与保留前缀校验；操作重入保护。
	if (!HSRSaveVersion::IsValidSlot(SlotName, UserIndex) || SlotName.Contains(TEXT(".__hsr_")))
	{
		return EHSRSaveResult::InvalidArgument;
	}
	if (bOperationInProgress)
	{
		return EHSRSaveResult::InvalidArgument;
	}
	TGuardValue<bool> OperationGuard(bOperationInProgress, true);

	const FString Staging = SlotName + TEXT(".__hsr_staging_v1"), Backup = SlotName + TEXT(".__hsr_backup_v1");
	const FHSRSaveData Previous = Current;

#if WITH_DEV_AUTOMATION_TESTS
	// 自动化注入：在捕获阶段制造失败。
	if (bInjectCreateFailure || InjectedTransactionStage == EHSRSaveFailureStage::Capture)
	{
		LastWriteFailureStage = EHSRSaveFailureStage::Capture;
		return bInjectCreateFailure ? EHSRSaveResult::CreateFailed : EHSRSaveResult::InvalidData;
	}
#endif

	// 捕获当前运行时快照。
	FHSRSaveData Captured;
	if (SaveSnapshot(Captured) != EHSRSaveResult::Success)
	{
		LastWriteFailureStage = EHSRSaveFailureStage::Capture;
		return EHSRSaveResult::InvalidData;
	}
	Captured.SchemaVersion = HSRSaveVersion::CurrentSchema;

	// Fail：记录失败阶段、回滚 Current 为写入前快照，返回对应结果码。
	auto Fail = [&](EHSRSaveFailureStage Stage, EHSRSaveResult Result)
	{
		LastWriteFailureStage = Stage;
		Current = Previous;
		return Result;
	};
	// ReadValid：读物理槽 + 解码信封 + 全量校验，三者全通过才视为「有效旧数据」。
	auto ReadValid = [&](const FString& Physical, TArray<uint8>& Out, FHSRSaveData& Data, FHSRSaveEnvelopeHeader& Header)
	{
		return UGameplayStatics::LoadDataFromSlot(Out, Physical, UserIndex)
			&& HSRSaveVersion::DecodeEnvelope(Out, SlotName, UserIndex, Data, &Header) == EHSRSaveDecodeResult::Success
			&& Validate(Data);
	};

	// 确定 SaveId 与 Generation：优先沿用主槽（或备份槽）的历史身份，否则新生成。
	FGuid Id = FGuid::NewGuid();
	uint64 Gen = 1;
	TArray<uint8> Old;
	FHSRSaveData OldData;
	FHSRSaveEnvelopeHeader OldHeader;
	const bool bOldPrimaryValid = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)
		&& ReadValid(SlotName, Old, OldData, OldHeader);
	if (bOldPrimaryValid)
	{
		if (OldHeader.Generation == MAX_uint64)
		{
			return Fail(EHSRSaveFailureStage::Encode, EHSRSaveResult::InvalidEnvelope);
		}
		Id = OldHeader.SaveId;
		Gen = OldHeader.Generation + 1;
	}
	else
	{
		TArray<uint8> ExistingBackup;
		FHSRSaveData BackupData;
		FHSRSaveEnvelopeHeader BackupHeader;
		if (UGameplayStatics::DoesSaveGameExist(Backup, UserIndex)
			&& ReadValid(Backup, ExistingBackup, BackupData, BackupHeader))
		{
			if (BackupHeader.Generation == MAX_uint64)
			{
				return Fail(EHSRSaveFailureStage::Encode, EHSRSaveResult::InvalidEnvelope);
			}
			Id = BackupHeader.SaveId;
			Gen = BackupHeader.Generation + 1;
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (InjectedTransactionStage == EHSRSaveFailureStage::Encode)
	{
		return Fail(EHSRSaveFailureStage::Encode, EHSRSaveResult::InvalidData);
	}
#endif

	// 编码信封。
	TArray<uint8> Bytes;
	if (!HSRSaveVersion::EncodeEnvelope(Captured, SlotName, UserIndex, Id, Gen, Bytes))
	{
		return Fail(EHSRSaveFailureStage::Encode, EHSRSaveResult::InvalidData);
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (bInjectSaveFailure || InjectedTransactionStage == EHSRSaveFailureStage::StagingWrite)
	{
		return Fail(EHSRSaveFailureStage::StagingWrite, EHSRSaveResult::SaveFailed);
	}
#endif

	// 第 1 步：写暂存槽，并回读校验（字节、SaveId、Generation 全比对）。
	if (!UGameplayStatics::SaveDataToSlot(Bytes, Staging, UserIndex))
	{
		return Fail(EHSRSaveFailureStage::StagingWrite, EHSRSaveResult::SaveFailed);
	}
	TArray<uint8> Check;
	FHSRSaveData CheckData;
	FHSRSaveEnvelopeHeader CheckHeader;
#if WITH_DEV_AUTOMATION_TESTS
	if (InjectedTransactionStage == EHSRSaveFailureStage::StagingReadback)
	{
		return Fail(EHSRSaveFailureStage::StagingReadback, EHSRSaveResult::LoadFailed);
	}
#endif
	const bool bStagingLoaded = UGameplayStatics::LoadDataFromSlot(Check, Staging, UserIndex);
	const EHSRSaveDecodeResult StagingDecode = bStagingLoaded
		? HSRSaveVersion::DecodeEnvelope(Check, SlotName, UserIndex, CheckData, &CheckHeader)
		: EHSRSaveDecodeResult::TooShort;
	const bool bStagingValid = StagingDecode == EHSRSaveDecodeResult::Success && Validate(CheckData);
	if (!bStagingValid || Check != Bytes || CheckHeader.SaveId != Id || CheckHeader.Generation != Gen)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR save staging validation failed Loaded=%d Decode=%d Valid=%d Bytes=%d Id=%d Gen=%d"),
			bStagingLoaded ? 1 : 0, static_cast<int32>(StagingDecode), bStagingValid ? 1 : 0,
			Check == Bytes ? 1 : 0, CheckHeader.SaveId == Id ? 1 : 0, CheckHeader.Generation == Gen ? 1 : 0);
		return Fail(EHSRSaveFailureStage::StagingReadback, EHSRSaveResult::LoadFailed);
	}

	// 第 2 步：若主槽原本有效，把旧数据备份到备份槽并回读校验。
	if (bOldPrimaryValid)
	{
#if WITH_DEV_AUTOMATION_TESTS
		if (InjectedTransactionStage == EHSRSaveFailureStage::BackupWrite)
		{
			return Fail(EHSRSaveFailureStage::BackupWrite, EHSRSaveResult::SaveFailed);
		}
#endif
		if (!UGameplayStatics::SaveDataToSlot(Old, Backup, UserIndex))
		{
			return Fail(EHSRSaveFailureStage::BackupWrite, EHSRSaveResult::SaveFailed);
		}
#if WITH_DEV_AUTOMATION_TESTS
		if (InjectedTransactionStage == EHSRSaveFailureStage::BackupReadback)
		{
			return Fail(EHSRSaveFailureStage::BackupReadback, EHSRSaveResult::LoadFailed);
		}
#endif
		TArray<uint8> BackupCheck;
		FHSRSaveData BackupCheckData;
		FHSRSaveEnvelopeHeader BackupCheckHeader;
		if (!ReadValid(Backup, BackupCheck, BackupCheckData, BackupCheckHeader)
			|| BackupCheck != Old
			|| BackupCheckHeader.SaveId != OldHeader.SaveId
			|| BackupCheckHeader.Generation != OldHeader.Generation)
		{
			return Fail(EHSRSaveFailureStage::BackupReadback, EHSRSaveResult::LoadFailed);
		}
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (InjectedTransactionStage == EHSRSaveFailureStage::PrimaryWrite)
	{
		return Fail(EHSRSaveFailureStage::PrimaryWrite, EHSRSaveResult::SaveFailed);
	}
#endif

	// 第 3 步：把新数据写入主槽并回读校验。
	if (!UGameplayStatics::SaveDataToSlot(Bytes, SlotName, UserIndex))
	{
		return Fail(EHSRSaveFailureStage::PrimaryWrite, EHSRSaveResult::SaveFailed);
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (InjectedTransactionStage == EHSRSaveFailureStage::PrimaryReadback)
	{
		return Fail(EHSRSaveFailureStage::PrimaryReadback, EHSRSaveResult::LoadFailed);
	}
#endif
	if (!ReadValid(SlotName, Check, CheckData, CheckHeader)
		|| Check != Bytes || CheckHeader.SaveId != Id || CheckHeader.Generation != Gen)
	{
		return Fail(EHSRSaveFailureStage::PrimaryReadback, EHSRSaveResult::LoadFailed);
	}
	LastWriteHeader = CheckHeader;

#if WITH_DEV_AUTOMATION_TESTS
	if (InjectedTransactionStage == EHSRSaveFailureStage::Cleanup)
	{
		bLastWriteCleanupWarning = true;
		return EHSRSaveResult::Success;
	}
#endif

	// 清理暂存槽；删除失败只记警告，不影响保存成功结果。
	if (UGameplayStatics::DoesSaveGameExist(Staging, UserIndex) && !UGameplayStatics::DeleteGameInSlot(Staging, UserIndex))
	{
		bLastWriteCleanupWarning = true;
	}
	return EHSRSaveResult::Success;
}

// LoadFromSlot：从磁盘槽位读档。策略：
//   - 主槽优先：解码成功且 CanPrepareSnapshot 通过则采用；
//   - 主槽是 BadMagic 时尝试旧 USaveGame 兼容路径；
//   - 主槽不可用时尝试备份槽（校验血缘：同 SaveId 且代数更小）；
//   - 都不可用则按失败原因返回 SlotNotFound / InvalidEnvelope 等。
// 最终把选中的数据交给 LoadSnapshot 落地，并更新 LastLoadResult。
EHSRSaveResult UHSRSaveSubsystem::LoadFromSlot(const FString& SlotName, int32 UserIndex)
{
	LastLoadResult = FHSRSaveLoadResult();

	// 槽位名与保留前缀校验。
	if (!HSRSaveVersion::IsValidSlot(SlotName, UserIndex) || SlotName.Contains(TEXT(".__hsr_")))
	{
		LastLoadResult.Result = EHSRSaveResult::InvalidArgument;
		LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::InvalidArgument;
		return LastLoadResult.Result;
	}
	if (bOperationInProgress)
	{
		LastLoadResult.Result = EHSRSaveResult::InvalidArgument;
		LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::Busy;
		return LastLoadResult.Result;
	}

	// 加载期间禁止有挂起的旅行（地图或战斗返回），否则恢复与旅行会互相干扰。
	const UHSRBattleTransitionSubsystem* Battle = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UHSRBattleTransitionSubsystem>() : nullptr;
#if WITH_DEV_AUTOMATION_TESTS
	const bool bMapBlocked = bInjectMapTravelPending || (Map.IsValid() && Map->HasPendingTravel());
	const bool bBattleBlocked = bInjectBattleReturnPending || (Battle && Battle->HasReturnPending());
#else
	const bool bMapBlocked = Map.IsValid() && Map->HasPendingTravel();
	const bool bBattleBlocked = Battle && Battle->HasReturnPending();
#endif
	if (bMapBlocked || bBattleBlocked)
	{
		LastLoadResult.Result = EHSRSaveResult::InvalidData;
		LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::TravelPending;
		return LastLoadResult.Result;
	}

	TGuardValue<bool> OperationGuard(bOperationInProgress, true);

#if WITH_DEV_AUTOMATION_TESTS
	if (bInjectLoadFailure)
	{
		LastLoadResult.Result = EHSRSaveResult::LoadFailed;
		return LastLoadResult.Result;
	}
#endif

	const FString BackupSlot = SlotName + TEXT(".__hsr_backup_v1");
	FHSRSaveData Selected;
	FHSRSaveEnvelopeHeader PrimaryHeader, BackupHeader;
	bool bHaveSelected = false;
	bool bPrimaryTrusted = false;
	EHSRSaveResult PrimaryFailureResult = EHSRSaveResult::InvalidEnvelope;

	// 主槽读取。
	if (UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		TArray<uint8> PrimaryBytes;
		FHSRSaveData PrimaryData;
		if (UGameplayStatics::LoadDataFromSlot(PrimaryBytes, SlotName, UserIndex))
		{
			const EHSRSaveDecodeResult Reason = HSRSaveVersion::DecodeEnvelope(PrimaryBytes, SlotName, UserIndex, PrimaryData, &PrimaryHeader);
			LastLoadResult.PrimaryReason = static_cast<uint8>(Reason);
			bPrimaryTrusted = PrimaryHeader.SaveId.IsValid();
			LastLoadResult.bPrimaryHeaderTrusted = bPrimaryTrusted;
			if (Reason == EHSRSaveDecodeResult::Success)
			{
				// 解码成功：只要预演通过就采用主槽。
				if (CanPrepareSnapshot(PrimaryData))
				{
					Selected = MoveTemp(PrimaryData);
					bHaveSelected = true;
					LastLoadResult.Source = EHSRSaveLoadSource::Primary;
					LastLoadResult.SaveId = PrimaryHeader.SaveId;
					LastLoadResult.Generation = PrimaryHeader.Generation;
				}
				else
				{
					LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::PrepareFailed;
				}
			}
			else if (Reason == EHSRSaveDecodeResult::BadMagic)
			{
				// 非本格式：尝试旧 USaveGame 兼容路径（仅部分 schema 可接受）。
				USaveGame* LegacyObject = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
				const UHSRSaveGame* Legacy = Cast<UHSRSaveGame>(LegacyObject);
				const bool bSupportedLegacy = Legacy && (Legacy->Data.SchemaVersion <= 5 || Legacy->Data.SchemaVersion >= 8);
				if (bSupportedLegacy && CanPrepareSnapshot(Legacy->Data))
				{
					Selected = Legacy->Data;
					bHaveSelected = true;
					LastLoadResult.Source = EHSRSaveLoadSource::LegacyPrimary;
				}
				else
				{
					LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::LegacyInvalid;
					PrimaryFailureResult = !LegacyObject ? EHSRSaveResult::LoadFailed
						: !Legacy ? EHSRSaveResult::ClassMismatch
						: !bSupportedLegacy ? EHSRSaveResult::UnsupportedSchema : EHSRSaveResult::InvalidData;
				}
			}
			else
			{
				LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::DecodeFailure;
			}
		}
		else
		{
			LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::DecodeFailure;
		}
	}
	else
	{
		LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::Missing;
		LastLoadResult.PrimaryReason = static_cast<uint8>(EHSRSaveDecodeResult::TooShort);
	}

	// 主槽不可用 => 尝试备份槽。
	if (!bHaveSelected && UGameplayStatics::DoesSaveGameExist(BackupSlot, UserIndex))
	{
		TArray<uint8> BackupBytes;
		FHSRSaveData BackupData;
		const bool bRead = UGameplayStatics::LoadDataFromSlot(BackupBytes, BackupSlot, UserIndex);
		const EHSRSaveDecodeResult Reason = bRead
			? HSRSaveVersion::DecodeEnvelope(BackupBytes, SlotName, UserIndex, BackupData, &BackupHeader)
			: EHSRSaveDecodeResult::TooShort;
		LastLoadResult.BackupReason = static_cast<uint8>(Reason);

		if (Reason == EHSRSaveDecodeResult::Success)
		{
			// 血缘校验：主槽可信时，备份必须同 SaveId 且代数严格更小。
			bool bLineageValid = true;
			if (bPrimaryTrusted)
			{
				if (BackupHeader.SaveId != PrimaryHeader.SaveId)
				{
					bLineageValid = false;
					LastLoadResult.BackupStageReason = EHSRSaveLoadReason::LineageMismatch;
				}
				else if (BackupHeader.Generation >= PrimaryHeader.Generation)
				{
					bLineageValid = false;
					LastLoadResult.BackupStageReason = EHSRSaveLoadReason::InvalidGeneration;
				}
			}
			if (bLineageValid && CanPrepareSnapshot(BackupData))
			{
				Selected = MoveTemp(BackupData);
				bHaveSelected = true;
				LastLoadResult.Source = EHSRSaveLoadSource::Backup;
				LastLoadResult.SaveId = BackupHeader.SaveId;
				LastLoadResult.Generation = BackupHeader.Generation;
				LastLoadResult.bRecoveredFromBackup = true;
				LastLoadResult.bPrimaryUntrusted = !bPrimaryTrusted;
			}
			else if (bLineageValid)
			{
				LastLoadResult.BackupStageReason = EHSRSaveLoadReason::PrepareFailed;
			}
		}
		else
		{
			LastLoadResult.BackupStageReason = EHSRSaveLoadReason::DecodeFailure;
		}
	}
	else if (!bHaveSelected)
	{
		LastLoadResult.BackupStageReason = EHSRSaveLoadReason::Missing;
		LastLoadResult.BackupReason = static_cast<uint8>(EHSRSaveDecodeResult::TooShort);
	}

	// 主备都不可用：按失败原因归类返回。
	if (!bHaveSelected)
	{
		LastLoadResult.Result = LastLoadResult.PrimaryStageReason == EHSRSaveLoadReason::Missing
			&& LastLoadResult.BackupStageReason == EHSRSaveLoadReason::Missing
			? EHSRSaveResult::SlotNotFound
			: LastLoadResult.PrimaryStageReason == EHSRSaveLoadReason::LegacyInvalid
				? PrimaryFailureResult
				: EHSRSaveResult::InvalidEnvelope;
		return LastLoadResult.Result;
	}

	// 落地：调用 LoadSnapshot，并根据事务版本号是否变化判断运行时是否被改动。
	const int64 BeforeRevision = RestoreTransactionRevision;
	LastLoadResult.Result = LoadSnapshot(Selected);
	LastLoadResult.bRuntimeChanged = RestoreTransactionRevision != BeforeRevision;
	if (LastLoadResult.Result != EHSRSaveResult::Success && LastLoadResult.Source != EHSRSaveLoadSource::None)
	{
		if (LastLoadResult.Source == EHSRSaveLoadSource::Backup)
		{
			LastLoadResult.BackupStageReason = EHSRSaveLoadReason::ProjectionFailed;
		}
		else
		{
			LastLoadResult.PrimaryStageReason = EHSRSaveLoadReason::ProjectionFailed;
		}
	}
	return LastLoadResult.Result;
}
