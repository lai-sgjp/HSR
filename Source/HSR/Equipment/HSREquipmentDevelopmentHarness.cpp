#include "HSREquipmentDevelopmentHarness.h"

#include "HSREquipmentSubsystem.h"
#include "../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "../UI/HSREquipmentDetailViewModel.h"
#include "../UI/HSREquipmentDetailWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// 匿名命名空间：Phase 12 装备开发的编辑器控制台命令与共享辅助函数。
// 这套工具用「固定角色 + 固定实例 GUID」构造一份可复现的装备负载，便于在 PIE 里
// 反复演练 装备/替换/卸下/保存/读档 的完整链路。
namespace
{
	const FName CharacterName(TEXT("Character.A"));
	// 固定实例 GUID：保证每次演练使用的是同一批装备实例，存档往返后可精确比对。
	const FGuid WeaponInstance(0x12004001, 0, 0, 1);
	const FGuid HeadInstance(0x12004002, 0, 0, 1);
	const FGuid HandsInstance(0x12004003, 0, 0, 1);
	const FString SaveSlot(TEXT("HSR_P12_Development"));
	// 当前显示中的详情 Widget（ShowDetail/HideDetail 共用）。
	TWeakObjectPtr<UHSREquipmentDetailWidget> DetailWidget;
	// 记录每个装备子系统最后一次由本 Harness 写入的负载版本号，用于生成下一个版本号。
	TMap<TObjectKey<UHSREquipmentSubsystem>, int32> LastHarnessRevisions;

	// 从当前 PIE 世界链里找到 GameInstance（用于取各子系统）。
	UGameInstance* FindPIEGameInstance()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->IsPlayInEditor())
			{
				return World->GetGameInstance();
			}
		}
		return nullptr;
	}

	// RegisterDefinitions：把本 Harness 依赖的装备定义注册进装备子系统。
	// 武器定义是临时构造的；头/手遗器定义从生产资源加载。已注册过的定义允许重复
	// （DuplicateDefinitionId 视作成功），保证命令可重复执行。
	bool RegisterDefinitions(UHSREquipmentSubsystem* Equipment, FName& OutHeadId, FName& OutHandsId, FName& OutSetId)
	{
		if (!Equipment)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL MissingSubsystem"));
			return false;
		}

		UHSREquipmentDefinition* Weapon = NewObject<UHSREquipmentDefinition>(GetTransientPackage());
		Weapon->DefinitionId = TEXT("Equipment.P12.FixedWeapon");
		Weapon->Slot = EHSREquipmentSlot::Weapon;
		Weapon->EnhancementCap = 15;
		const EHSREquipmentOperationResult WeaponResult = Equipment->RegisterDefinition(*Weapon);
		if (WeaponResult != EHSREquipmentOperationResult::Success
			&& WeaponResult != EHSREquipmentOperationResult::DuplicateDefinitionId)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL WeaponResult=%d"),
				static_cast<int32>(WeaponResult));
			return false;
		}

		// 从生产资源加载头/手遗器，并要求它们属于同一套装（这样才能演练套装计数）。
		UHSRRelicDefinition* Head = LoadObject<UHSRRelicDefinition>(nullptr, TEXT("/Game/Data/Relics/DA_Relic_Head.DA_Relic_Head"));
		UHSRRelicDefinition* Hands = LoadObject<UHSRRelicDefinition>(nullptr, TEXT("/Game/Data/Relics/DA_Relic_Hands.DA_Relic_Hands"));
		if (!Head || !Hands || Head->DefinitionId.IsNone() || Hands->DefinitionId.IsNone()
			|| Head->SetId.IsNone() || Head->SetId != Hands->SetId)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL Head=%s HeadId=%s HeadSet=%s Hands=%s HandsId=%s HandsSet=%s"),
				Head ? TEXT("Loaded") : TEXT("Missing"), Head ? *Head->DefinitionId.ToString() : TEXT("None"),
				Head ? *Head->SetId.ToString() : TEXT("None"), Hands ? TEXT("Loaded") : TEXT("Missing"),
				Hands ? *Hands->DefinitionId.ToString() : TEXT("None"), Hands ? *Hands->SetId.ToString() : TEXT("None"));
			return false;
		}

		const EHSREquipmentOperationResult HeadResult = Equipment->RegisterDefinition(*Head);
		const EHSREquipmentOperationResult HandsResult = Equipment->RegisterDefinition(*Hands);
		if ((HeadResult != EHSREquipmentOperationResult::Success && HeadResult != EHSREquipmentOperationResult::DuplicateDefinitionId)
			|| (HandsResult != EHSREquipmentOperationResult::Success && HandsResult != EHSREquipmentOperationResult::DuplicateDefinitionId))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness RegisterDefinitions FAIL HeadResult=%d HandsResult=%d"),
				static_cast<int32>(HeadResult), static_cast<int32>(HandsResult));
			return false;
		}

		OutHeadId = Head->DefinitionId;
		OutHandsId = Hands->DefinitionId;
		OutSetId = Head->SetId;
		return true;
	}

	// Commit：把一份目标恢复状态（Desired）提交到装备子系统（GameInstance 版本）。
	// 先 ProjectRestore 预演，再 CommitRestore 真正落地，最后 NotifyRestored 广播；
	// 若提供了 Desired，则把它的版本号记录到 LastHarnessRevisions 供后续递增。
	bool Commit(UGameInstance* GameInstance, const FHSREquipmentRestoreState* Desired)
	{
		UHSREquipmentSubsystem* Equipment = GameInstance ? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
		if (!Equipment)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness Commit FAIL MissingEquipmentSubsystem"));
			return false;
		}

		FHSREquipmentRestoreMap Candidate;
		const FGuid CharacterId = HSRCharacterGuidFromProfileName(CharacterName);
		if (Desired)
		{
			Candidate.Add(CharacterId, *Desired);
		}
		if (!Equipment->ProjectRestore(Candidate))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness Commit FAIL ProjectRestore CandidateCharacters=%d Desired=%d"),
				Candidate.Num(), Desired ? 1 : 0);
			return false;
		}
		Equipment->CommitRestore(Candidate);
		Equipment->NotifyRestored({ CharacterId });
		if (Desired)
		{
			LastHarnessRevisions.Add(Equipment, Desired->Revision);
		}
		return true;
	}

	// Commit：同上，但直接接收装备子系统指针（供测试路径复用）。
	bool Commit(UHSREquipmentSubsystem* Equipment, const FHSREquipmentRestoreState* Desired)
	{
		if (!Equipment)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness CommitForTest FAIL MissingEquipmentSubsystem"));
			return false;
		}

		FHSREquipmentRestoreMap Candidate;
		const FGuid CharacterId = HSRCharacterGuidFromProfileName(CharacterName);
		if (Desired)
		{
			Candidate.Add(CharacterId, *Desired);
		}
		if (!Equipment->ProjectRestore(Candidate))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness CommitForTest FAIL ProjectRestore CandidateCharacters=%d Desired=%d"),
				Candidate.Num(), Desired ? 1 : 0);
			return false;
		}
		Equipment->CommitRestore(Candidate);
		Equipment->NotifyRestored({ CharacterId });
		if (Desired)
		{
			LastHarnessRevisions.Add(Equipment, Desired->Revision);
		}
		return true;
	}

	// BuildFixedState：构造固定的装备负载。bIncludeHands 控制是否包含第二件遗器（手部），
	// 从而在「2 件套 -> 1 件套」之间切换套装计数。
	// 版本号取「现有负载版本、本 Harness 上次写入版本 + 1、1」三者的最大值，确保单调递增。
	FHSREquipmentRestoreState BuildFixedState(UHSREquipmentSubsystem* Equipment, bool bIncludeHands)
	{
		FName HeadId, HandsId, SetId;
		FHSREquipmentRestoreState State;
		if (!RegisterDefinitions(Equipment, HeadId, HandsId, SetId))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness BuildFixedState FAIL RegisterDefinitions"));
			return State;
		}

		// 武器：固定词条 +20 攻击。
		FHSREquipmentInstance Weapon;
		Weapon.InstanceId = WeaponInstance;
		Weapon.DefinitionId = TEXT("Equipment.P12.FixedWeapon");
		Weapon.Kind = EHSREquipmentKind::Equipment;
		Weapon.EnhancementLevel = 3;
		Weapon.Modifiers.Add({ EHSREquipmentStat::Attack, 20.0f });
		State.Loadout.Equipment.Add(EHSREquipmentSlot::Weapon, Weapon);

		// 头部遗器：固定词条 +12 防御。
		FHSREquipmentInstance Head;
		Head.InstanceId = HeadInstance;
		Head.DefinitionId = HeadId;
		Head.Kind = EHSREquipmentKind::Relic;
		Head.EnhancementLevel = 3;
		Head.Modifiers.Add({ EHSREquipmentStat::Defense, 12.0f });
		State.Loadout.Relics.Add(EHSRRelicSlot::Head, Head);

		// 手部遗器（可选）：固定词条 +4 速度；同时把套装计数设为 2 或 1。
		if (bIncludeHands)
		{
			FHSREquipmentInstance Hands;
			Hands.InstanceId = HandsInstance;
			Hands.DefinitionId = HandsId;
			Hands.Kind = EHSREquipmentKind::Relic;
			Hands.EnhancementLevel = 3;
			Hands.Modifiers.Add({ EHSREquipmentStat::Speed, 4.0f });
			State.Loadout.Relics.Add(EHSRRelicSlot::Hands, Hands);
		}
		State.RelicSetCounts.Add(SetId, bIncludeHands ? 2 : 1);

		// 版本号递增。
		FHSREquipmentLoadout Existing;
		int32 ExistingRevision = 0;
		Equipment->GetLoadout(HSRCharacterGuidFromProfileName(CharacterName), Existing, ExistingRevision);
		State.Revision = FMath::Max(FMath::Max(ExistingRevision, LastHarnessRevisions.FindRef(Equipment)) + 1, 1);
		return State;
	}

	// LogResult：统一输出命令执行结果。
	void LogResult(const TCHAR* Command, bool bSuccess)
	{
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("HSR.EquipmentHarness Command=%s Result=SUCCESS"), Command);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness Command=%s Result=FAIL"), Command);
		}
	}

	// EnsureLumenShards：确保背包里有足够的「流明碎片」材料（用于 P17 强化演练）。
	// 缺失定义则临时注册；不足则补到 Desired（取 MaxStack 与 999 的较小值）。
	void EnsureLumenShards(UGameInstance* GI)
	{
		UHSRInventorySubsystem* Inventory = GI ? GI->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
		if (!Inventory)
		{
			return;
		}

		static const FName MaterialId(TEXT("Item.Material.LumenShard"));
		if (!Inventory->HasDefinition(MaterialId))
		{
			UHSRItemDefinition* Material = NewObject<UHSRItemDefinition>(GetTransientPackage());
			Material->ItemId = MaterialId;
			Material->StorageKind = EHSRItemStorageKind::Stackable;
			Material->MaxStack = 99;
			const EHSRInventoryOperationResult DefResult = Inventory->RegisterDefinition(*Material);
			if (DefResult != EHSRInventoryOperationResult::Success
				&& DefResult != EHSRInventoryOperationResult::DuplicateDefinitionId)
			{
				return;
			}
		}

		EHSRItemStorageKind StorageKind = EHSRItemStorageKind::Stackable;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(MaterialId, StorageKind, MaxStack))
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentHarness EnsureLumenShards FAIL MissingDefinition"));
			return;
		}

		FHSRInventorySnapshot Snap;
		Inventory->GetSnapshot(Snap);
		const int32 Have = Snap.GetStackQuantity(MaterialId);
		const int32 Desired = FMath::Min(999, MaxStack);
		if (Have >= Desired)
		{
			UE_LOG(LogTemp, Log, TEXT("HSR.EquipmentHarness EnsureLumenShards Have=%d MaxStack=%d"), Have, MaxStack);
			return;
		}

		const EHSRInventoryOperationResult AddResult = Inventory->AddStack(MaterialId, Desired - Have);
		if (AddResult != EHSRInventoryOperationResult::Success)
		{
			UE_LOG(LogTemp, Error,
				TEXT("HSR.EquipmentHarness EnsureLumenShards FAIL AddStack Result=%d Have=%d Desired=%d MaxStack=%d"),
				static_cast<int32>(AddResult), Have, Desired, MaxStack);
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("HSR.EquipmentHarness EnsureLumenShards Granted=%d Total=%d MaxStack=%d"),
			Desired - Have, Desired, MaxStack);
	}

	// 各命令的入口函数（都是「取 GameInstance -> 调 Harness 方法 -> 输出结果」的薄壳）。
	void RunSetup()
	{
		UGameInstance* GI = FindPIEGameInstance();
		const bool bOk = FHSREquipmentDevelopmentHarness::SetupFixedLoadout(GI);
		if (bOk)
		{
			EnsureLumenShards(GI);
		}
		LogResult(TEXT("Setup"), bOk);
	}

	void RunRemove()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("RemoveSecondRelic"), FHSREquipmentDevelopmentHarness::RemoveSecondRelic(GI));
	}

	void RunRestore()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("RestoreSecondRelic"), FHSREquipmentDevelopmentHarness::RestoreSecondRelic(GI));
	}

	void RunClear()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("Clear"), FHSREquipmentDevelopmentHarness::ClearLoadout(GI));
	}

	void RunSave()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("Save"), FHSREquipmentDevelopmentHarness::Save(GI));
	}

	void RunLoad()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("Load"), FHSREquipmentDevelopmentHarness::Load(GI));
	}

	void RunCleanup()
	{
		LogResult(TEXT("Cleanup"), FHSREquipmentDevelopmentHarness::CleanupSave());
	}

	void RunP17Audit()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("P17MovementAudit"), FHSREquipmentDevelopmentHarness::RunP17MovementAudit(GI));
	}

	void RunP17RelicFixture()
	{
		UGameInstance* GI = FindPIEGameInstance();
		LogResult(TEXT("P17RelicFixture"), FHSREquipmentDevelopmentHarness::RunP17RelicFixture(GI));
	}

	// RunHide：隐藏并销毁详情 Widget。
	void RunHide()
	{
		if (DetailWidget.IsValid())
		{
			DetailWidget->RemoveFromParent();
			DetailWidget.Reset();
		}
		LogResult(TEXT("HideDetail"), true);
	}

	// RunShow：创建只读的装备详情 Widget 并挂到视口。先隐藏旧的，再创建新的，
	// 用 ViewModel 绑定到目标角色的装备子系统。
	void RunShow()
	{
		UGameInstance* GI = FindPIEGameInstance();
		UWorld* World = GI ? GI->GetWorld() : nullptr;
		UHSREquipmentSubsystem* Equipment = GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
		TSubclassOf<UHSREquipmentDetailWidget> WidgetClass = LoadClass<UHSREquipmentDetailWidget>(nullptr,
			TEXT("/Game/UI/WBP_EquipmentDetail_P12.WBP_EquipmentDetail_P12_C"));
		if (!World || !Equipment || !WidgetClass)
		{
			LogResult(TEXT("ShowDetail"), false);
			return;
		}

		RunHide();
		UHSREquipmentDetailWidget* Widget = CreateWidget<UHSREquipmentDetailWidget>(World, WidgetClass);
		if (!Widget)
		{
			LogResult(TEXT("ShowDetail"), false);
			return;
		}
		UHSREquipmentDetailViewModel* ViewModel = NewObject<UHSREquipmentDetailViewModel>(Widget);
		if (!ViewModel)
		{
			LogResult(TEXT("ShowDetail"), false);
			return;
		}
		Widget->AddToViewport(100);
		Widget->SetViewModel(ViewModel);
		ViewModel->Initialize(Equipment, HSRCharacterGuidFromProfileName(CharacterName));
		DetailWidget = Widget;
		LogResult(TEXT("ShowDetail"), true);
	}

	// 注册全部控制台命令。
	FAutoConsoleCommand SetupCommand(TEXT("HSR.Equipment.Setup"),
		TEXT("Build the fixed Phase 12 loadout."), FConsoleCommandDelegate::CreateStatic(&RunSetup));
	FAutoConsoleCommand RemoveCommand(TEXT("HSR.Equipment.RemoveSecondRelic"),
		TEXT("Transition the fixed set from 2 to 1."), FConsoleCommandDelegate::CreateStatic(&RunRemove));
	FAutoConsoleCommand RestoreCommand(TEXT("HSR.Equipment.RestoreSecondRelic"),
		TEXT("Transition the fixed set from 1 to 2."), FConsoleCommandDelegate::CreateStatic(&RunRestore));
	FAutoConsoleCommand ClearCommand(TEXT("HSR.Equipment.Clear"),
		TEXT("Clear the fixed loadout."), FConsoleCommandDelegate::CreateStatic(&RunClear));
	FAutoConsoleCommand ShowCommand(TEXT("HSR.Equipment.ShowDetail"),
		TEXT("Create and show the read-only Phase 12 detail widget."), FConsoleCommandDelegate::CreateStatic(&RunShow));
	FAutoConsoleCommand HideCommand(TEXT("HSR.Equipment.HideDetail"),
		TEXT("Destroy the Phase 12 detail widget."), FConsoleCommandDelegate::CreateStatic(&RunHide));
	FAutoConsoleCommand SaveCommand(TEXT("HSR.Equipment.Save"),
		TEXT("Save the Phase 12 development slot."), FConsoleCommandDelegate::CreateStatic(&RunSave));
	FAutoConsoleCommand LoadCommand(TEXT("HSR.Equipment.Load"),
		TEXT("Load the Phase 12 development slot."), FConsoleCommandDelegate::CreateStatic(&RunLoad));
	FAutoConsoleCommand CleanupCommand(TEXT("HSR.Equipment.Cleanup"),
		TEXT("Delete the Phase 12 development slot."), FConsoleCommandDelegate::CreateStatic(&RunCleanup));
	FAutoConsoleCommand P17AuditCommand(TEXT("HSR.Equipment.P17Audit"),
		TEXT("Run the P17 equip, replace, and unequip transaction audit."), FConsoleCommandDelegate::CreateStatic(&RunP17Audit));
	FAutoConsoleCommand P17RelicFixtureCommand(TEXT("HSR.Equipment.P17RelicFixture"),
		TEXT("Run the P17 relic candidate and material enhancement authority audit."), FConsoleCommandDelegate::CreateStatic(&RunP17RelicFixture));
}

// 公开的 Harness 方法：供编辑器命令与自动化测试共同调用。
// 每个方法都是「取子系统 -> 构造固定状态 -> Commit」的薄封装，保证 PIE 与测试
// 两条路径使用完全相同的装备数据。

bool FHSREquipmentDevelopmentHarness::SetupFixedLoadout(UGameInstance* GI)
{
	UHSREquipmentSubsystem* E = GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	if (!E)
	{
		return false;
	}
	const FHSREquipmentRestoreState State = BuildFixedState(E, true);
	return !State.Loadout.Relics.IsEmpty() && Commit(GI, &State);
}

bool FHSREquipmentDevelopmentHarness::RemoveSecondRelic(UGameInstance* GI)
{
	UHSREquipmentSubsystem* E = GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	if (!E)
	{
		return false;
	}
	// 不带手部 => 套装从 2 件切到 1 件。
	const FHSREquipmentRestoreState State = BuildFixedState(E, false);
	return !State.Loadout.Relics.IsEmpty() && Commit(GI, &State);
}

bool FHSREquipmentDevelopmentHarness::RestoreSecondRelic(UGameInstance* GI)
{
	return SetupFixedLoadout(GI);
}

// ClearLoadout：清空目标角色的装备负载（Commit nullptr 表示空负载），
// 并把版本号推进到下一个合法值。
bool FHSREquipmentDevelopmentHarness::ClearLoadout(UGameInstance* GI)
{
	UHSREquipmentSubsystem* E = GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	if (!E)
	{
		return false;
	}
	FHSREquipmentLoadout Existing;
	int32 Revision = 0;
	E->GetLoadout(HSRCharacterGuidFromProfileName(CharacterName), Existing, Revision);
	const int32 ClearRevision = FMath::Max(FMath::Max(Revision, LastHarnessRevisions.FindRef(E)) + 1, 1);
	if (!Commit(GI, nullptr))
	{
		return false;
	}
	LastHarnessRevisions.Add(E, ClearRevision);
	return true;
}

// Save：把当前运行时快照写入开发槽位。
bool FHSREquipmentDevelopmentHarness::Save(UGameInstance* GI)
{
	UHSRSaveSubsystem* S = GI ? GI->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
	return S && S->SaveToSlot(SaveSlot) == EHSRSaveResult::Success;
}

// Load：读档前先确保定义已注册（读档校验会检查定义存在性），再从槽位读档。
bool FHSREquipmentDevelopmentHarness::Load(UGameInstance* GI)
{
	UHSREquipmentSubsystem* E = GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	UHSRSaveSubsystem* S = GI ? GI->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
	FName HeadId, HandsId, SetId;
	return E && S && RegisterDefinitions(E, HeadId, HandsId, SetId)
		&& S->LoadFromSlot(SaveSlot) == EHSRSaveResult::Success;
}

// CleanupSave：删除开发槽位（不存在也算成功）。
bool FHSREquipmentDevelopmentHarness::CleanupSave()
{
	return !UGameplayStatics::DoesSaveGameExist(SaveSlot, 0) || UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
}

// 下面三个 ForTest 版本与上面的 GameInstance 版本逻辑一致，只是直接接收子系统指针，
// 供自动化测试在没有 PIE 的情况下调用。
bool FHSREquipmentDevelopmentHarness::SetupFixedLoadoutForTest(UHSREquipmentSubsystem* E)
{
	if (!E)
	{
		return false;
	}
	const FHSREquipmentRestoreState State = BuildFixedState(E, true);
	return !State.Loadout.Relics.IsEmpty() && Commit(E, &State);
}

bool FHSREquipmentDevelopmentHarness::RemoveSecondRelicForTest(UHSREquipmentSubsystem* E)
{
	if (!E)
	{
		return false;
	}
	const FHSREquipmentRestoreState State = BuildFixedState(E, false);
	return !State.Loadout.Relics.IsEmpty() && Commit(E, &State);
}

bool FHSREquipmentDevelopmentHarness::ClearLoadoutForTest(UHSREquipmentSubsystem* E)
{
	if (!E)
	{
		return false;
	}
	FHSREquipmentLoadout Existing;
	int32 Revision = 0;
	E->GetLoadout(HSRCharacterGuidFromProfileName(CharacterName), Existing, Revision);
	const int32 ClearRevision = FMath::Max(FMath::Max(Revision, LastHarnessRevisions.FindRef(E)) + 1, 1);
	if (!Commit(E, nullptr))
	{
		return false;
	}
	LastHarnessRevisions.Add(E, ClearRevision);
	return true;
}

bool FHSREquipmentDevelopmentHarness::RunP17MovementAudit(UGameInstance* GI)
{
	return RunP17MovementAuditForTest(GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr,
		GI ? GI->GetSubsystem<UHSRInventorySubsystem>() : nullptr);
}

// RunP17MovementAuditForTest：P17 装备事务审计。
// 构造一个「控制台物品」唯一物品 + 对应的装备定义 + 映射目录，把两个实例加入背包与
// 装备注册表，然后依次执行 装备(Equip) -> 替换(Replace) -> 卸下(Unequip)，每步都
// 用最新的背包/装备版本号，验证事务性移动（movement）的完整链路。
bool FHSREquipmentDevelopmentHarness::RunP17MovementAuditForTest(UHSREquipmentSubsystem* E, UHSRInventorySubsystem* I)
{
	if (!E || !I)
	{
		return false;
	}

	// 注册物品/装备/映射定义。
	auto* Item = NewObject<UHSRItemDefinition>(GetTransientPackage());
	Item->ItemId = TEXT("Item.P17.Console");
	Item->StorageKind = EHSRItemStorageKind::Unique;
	Item->MaxStack = 1;
	const EHSRInventoryOperationResult ItemResult = I->RegisterDefinition(*Item);
	if (ItemResult != EHSRInventoryOperationResult::Success && ItemResult != EHSRInventoryOperationResult::DuplicateDefinitionId)
	{
		return false;
	}

	auto* Definition = NewObject<UHSREquipmentDefinition>(GetTransientPackage());
	Definition->DefinitionId = TEXT("Equipment.P17.Console");
	Definition->Slot = EHSREquipmentSlot::Body;
	Definition->EnhancementCap = 1;
	const EHSREquipmentOperationResult DefinitionResult = E->RegisterDefinition(*Definition);
	if (DefinitionResult != EHSREquipmentOperationResult::Success && DefinitionResult != EHSREquipmentOperationResult::DuplicateDefinitionId)
	{
		return false;
	}

	auto* Catalog = NewObject<UHSRItemEquipmentMappingCatalog>(GetTransientPackage());
	FHSRItemEquipmentMappingEntry Mapping;
	Mapping.ItemId = Item->ItemId;
	Mapping.EquipmentDefinitionId = Definition->DefinitionId;
	Mapping.Kind = EHSREquipmentKind::Equipment;
	Mapping.Slot = static_cast<int32>(EHSREquipmentSlot::Body);
	if (!Catalog->AddMapping(Mapping))
	{
		return false;
	}

	// 造两个唯一实例：同时加入背包（作为唯一物品）与装备注册表（作为可装备实例）。
	const FGuid CharacterId = HSRCharacterGuidFromProfileName(CharacterName);
	const FGuid First = FGuid::NewGuid(), Second = FGuid::NewGuid();
	for (const FGuid Id : { First, Second })
	{
		FHSRItemInstance Bag;
		Bag.InstanceId = Id;
		Bag.DefinitionId = Item->ItemId;
		if (I->AddUnique(Bag) != EHSRInventoryOperationResult::Success)
		{
			return false;
		}
		FHSREquipmentInstance Registry;
		Registry.InstanceId = Id;
		Registry.DefinitionId = Definition->DefinitionId;
		Registry.Kind = EHSREquipmentKind::Equipment;
		if (E->RegisterInstance(Registry) != EHSREquipmentOperationResult::Success)
		{
			return false;
		}
	}

	// Execute：构造一次装备移动请求，交给装备子系统的 ExecuteMovement 执行。
	auto Execute = [&](FGuid OperationId, FGuid InstanceId, EHSREquipmentMovementIntent Intent, int64 InventoryRevision, int64 EquipmentRevision)
	{
		FHSREquipmentMovementRequest Request;
		Request.OperationId = OperationId;
		Request.CharacterId = CharacterId;
		Request.InstanceId = InstanceId;
		Request.Intent = Intent;
		Request.Kind = EHSREquipmentKind::Equipment;
		Request.Slot = static_cast<int32>(EHSREquipmentSlot::Body);
		Request.ExpectedInventoryRevision = InventoryRevision;
		Request.ExpectedEquipmentRevision = EquipmentRevision;
		return E->ExecuteMovement(Request, *I, *Catalog);
	};

	// 依次执行三步事务，每步用上一步产出的最新版本号作为期望版本号。
	FHSRInventorySnapshot Inventory;
	FHSREquipmentLoadout InitialLoadout;
	int32 InitialEquipmentRevision = 0;
	E->GetLoadout(CharacterId, InitialLoadout, InitialEquipmentRevision);
	I->GetSnapshot(Inventory);
	const FHSREquipmentMovementResult Equip = Execute(FGuid::NewGuid(), First, EHSREquipmentMovementIntent::Equip, Inventory.Revision, InitialEquipmentRevision);
	if (Equip.Code != EHSREquipmentMovementResultCode::Success)
	{
		return false;
	}
	I->GetSnapshot(Inventory);
	const FHSREquipmentMovementResult Replace = Execute(FGuid::NewGuid(), Second, EHSREquipmentMovementIntent::Replace, Inventory.Revision, Equip.NewEquipmentRevision);
	if (Replace.Code != EHSREquipmentMovementResultCode::Success)
	{
		return false;
	}
	I->GetSnapshot(Inventory);
	const FHSREquipmentMovementResult Unequip = Execute(FGuid::NewGuid(), Second, EHSREquipmentMovementIntent::Unequip, Inventory.Revision, Replace.NewEquipmentRevision);

	// 校验：卸下成功，且最终负载的武器数量与初始一致（即换装后回到等量状态）。
	FHSREquipmentLoadout FinalLoadout;
	int32 FinalRevision = 0;
	E->GetLoadout(CharacterId, FinalLoadout, FinalRevision);
	return Unequip.Code == EHSREquipmentMovementResultCode::Success
		&& FinalLoadout.Equipment.Num() == InitialLoadout.Equipment.Num();
}

bool FHSREquipmentDevelopmentHarness::RunP17RelicFixture(UGameInstance* GI)
{
	return RunP17RelicFixtureForTest(GI ? GI->GetSubsystem<UHSREquipmentSubsystem>() : nullptr,
		GI ? GI->GetSubsystem<UHSRInventorySubsystem>() : nullptr);
}

// RunP17RelicFixtureForTest：P17 遗器强化（Enhancement）审计。
// 构造一个带词条的头部遗器实例、注册对应的强化规则（升级到 1 级需要 2 份材料），
// 然后连续执行两次相同的强化请求——第一次应真正提交，第二次因 OperationId 相同
// 应命中账本返回幂等重放（bReplay=true, bCommitted=false），并验证材料只扣一次、
// 强化等级只升一级。
bool FHSREquipmentDevelopmentHarness::RunP17RelicFixtureForTest(UHSREquipmentSubsystem* Equipment,
	UHSRInventorySubsystem* Inventory)
{
	if (!Equipment || !Inventory)
	{
		return false;
	}

	const FName MaterialId(TEXT("Item.P17.RelicMaterial"));
	const FName DefinitionId(TEXT("Relic.P17.ConsoleHead"));
	const FGuid CharacterId = HSRCharacterGuidFromProfileName(TEXT("Character.A"));
	const FGuid InstanceId(0x17007001, 0, 0, 1);

	// 注册材料定义并发 5 份。
	UHSRItemDefinition* Material = NewObject<UHSRItemDefinition>(GetTransientPackage());
	Material->ItemId = MaterialId;
	Material->StorageKind = EHSRItemStorageKind::Stackable;
	Material->MaxStack = 20;
	const EHSRInventoryOperationResult MaterialDefinitionResult = Inventory->RegisterDefinition(*Material);
	if (MaterialDefinitionResult != EHSRInventoryOperationResult::Success
		&& MaterialDefinitionResult != EHSRInventoryOperationResult::DuplicateDefinitionId)
	{
		return false;
	}
	if (Inventory->AddStack(MaterialId, 5) != EHSRInventoryOperationResult::Success)
	{
		return false;
	}

	// 注册头部遗器定义（带强化上限）。
	UHSRRelicDefinition* Definition = NewObject<UHSRRelicDefinition>(GetTransientPackage());
	Definition->DefinitionId = DefinitionId;
	Definition->SetId = TEXT("Set.P17.Console");
	Definition->Slot = EHSRRelicSlot::Head;
	Definition->EnhancementCap = 3;
	const EHSREquipmentOperationResult DefinitionResult = Equipment->RegisterDefinition(*Definition);
	if (DefinitionResult != EHSREquipmentOperationResult::Success
		&& DefinitionResult != EHSREquipmentOperationResult::DuplicateDefinitionId)
	{
		return false;
	}

	// 装备一件带基础词条的遗器。
	FHSREquipmentInstance Instance;
	Instance.InstanceId = InstanceId;
	Instance.DefinitionId = DefinitionId;
	Instance.Kind = EHSREquipmentKind::Relic;
	Instance.Modifiers.Add({ EHSREquipmentStat::Attack, 4.0f });
	const EHSREquipmentOperationResult InstanceResult = Equipment->Equip(CharacterId, Instance);
	if (InstanceResult != EHSREquipmentOperationResult::Success
		&& InstanceResult != EHSREquipmentOperationResult::NoOp)
	{
		return false;
	}

	// 构造强化规则：目标 1 级、消耗 2 份材料、命中后攻击词条变成 +8。
	UHSREquipmentEnhancementCatalog* Catalog = NewObject<UHSREquipmentEnhancementCatalog>(GetTransientPackage());
	FHSREquipmentEnhancementRule Rule;
	Rule.DefinitionId = DefinitionId;
	Rule.Kind = EHSREquipmentKind::Relic;
	Rule.TargetLevel = 1;
	Rule.MaterialItemId = MaterialId;
	Rule.MaterialCost = 2;
	Rule.TargetModifiers.Add({ EHSREquipmentStat::Attack, 8.0f });
	if (!Catalog->AddRule(Rule))
	{
		return false;
	}

	// 读取当前快照与版本号，构造强化请求（期望版本号与当前一致）。
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	FHSREquipmentLoadout Loadout;
	int32 EquipmentRevision = 0;
	if (!Equipment->GetLoadout(CharacterId, Loadout, EquipmentRevision))
	{
		return false;
	}
	FHSREquipmentEnhancementRequest Request;
	Request.OperationId = FGuid::NewGuid();
	Request.CharacterId = CharacterId;
	Request.InstanceId = InstanceId;
	Request.Kind = EHSREquipmentKind::Relic;
	Request.ExpectedInventoryRevision = InventorySnapshot.Revision;
	Request.ExpectedEquipmentRevision = EquipmentRevision;
	Request.ExpectedEnhancementLevel = 0;
	Request.TargetLevel = 1;

	// 执行两次相同请求：第二次应为幂等重放。
	const FHSREquipmentEnhancementResult Result = Equipment->ExecuteEnhancement(Request, *Inventory, *Catalog);
	const FHSREquipmentEnhancementResult Replay = Equipment->ExecuteEnhancement(Request, *Inventory, *Catalog);

	// 校验最终状态：强化到 1 级、材料从 5 扣到 3。
	FHSRInventorySnapshot AfterInventory;
	Inventory->GetSnapshot(AfterInventory);
	FHSREquipmentInstance AfterInstance;
	Equipment->FindRegisteredInstance(InstanceId, AfterInstance);
	const bool bSuccess = Result.Code == EHSREquipmentEnhancementResultCode::Success && Result.bCommitted
		&& Replay.Code == EHSREquipmentEnhancementResultCode::Success && Replay.bReplay && !Replay.bCommitted
		&& AfterInstance.EnhancementLevel == 1 && AfterInventory.Stacks.Num() == 1
		&& AfterInventory.Stacks[0].ItemId == MaterialId && AfterInventory.Stacks[0].Quantity == 3;
	UE_LOG(LogTemp, Log, TEXT("HSR.EquipmentHarness P17RelicFixture Result=%s Code=%d Replay=%s Level=%d Material=%d"),
		bSuccess ? TEXT("SUCCESS") : TEXT("FAIL"), static_cast<int32>(Result.Code), Replay.bReplay ? TEXT("true") : TEXT("false"),
		AfterInstance.EnhancementLevel, AfterInventory.Stacks.Num() == 1 ? AfterInventory.Stacks[0].Quantity : -1);
	return bSuccess;
}
