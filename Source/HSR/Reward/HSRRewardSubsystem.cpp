#include "HSRRewardSubsystem.h"

#include "HSRRewardResolver.h"
#include "../Data/Definitions/HSRDropTableDefinition.h"
#include "../Data/Definitions/HSRItemDefinition.h"
#include "../Data/Definitions/HSRRewardDefinition.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "Misc/Crc.h"
#include "HSRSettlementTypes.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// AreDropEntriesEqual：逐项比较两份掉落表条目（顺序敏感），用于判断
	// 「已注册的表」与「本次要注册的表」是否一致。
	bool AreDropEntriesEqual(const TArray<FHSRDropTableEntry>& A, const TArray<FHSRDropTableEntry>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].ItemId != B[Index].ItemId
				|| A[Index].MinQuantity != B[Index].MinQuantity
				|| A[Index].MaxQuantity != B[Index].MaxQuantity
				|| A[Index].Weight != B[Index].Weight)
			{
				return false;
			}
		}
		return true;
	}

	// AreRewardEntriesEqual：逐项比较两份奖励固定物品列表。
	bool AreRewardEntriesEqual(const TArray<FHSRRewardItemEntry>& A, const TArray<FHSRRewardItemEntry>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].ItemId != B[Index].ItemId || A[Index].Quantity != B[Index].Quantity)
			{
				return false;
			}
		}
		return true;
	}
}

// Initialize：启动时注册生产环境的奖励定义（P13 标准包与 VerticalSlice 演示包）。
// 关键动机：存档校验可能在奖励宝箱进入世界之前就执行（冷启动恢复），因此所有被
// 存档引用的奖励定义必须在 GameInstance 启动时注册，而不是依赖 Actor BeginPlay 的副作用。
void UHSRRewardSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UHSRInventorySubsystem>();
	Inventory = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	if (!Inventory.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("P13-004 ProductionDefinitionBootstrap=FAILED Reason=InventoryUnavailable"));
		return;
	}

	// 注册 P13 标准包的物品定义（流明碎片 + 存档令牌）。
	TArray<TObjectPtr<UHSRItemDefinition>> ItemDefinitions;
	ItemDefinitions.Add(LoadObject<UHSRItemDefinition>(nullptr, TEXT("/Game/Data/Items/DA_Item_LumenShard_P13.DA_Item_LumenShard_P13")));
	ItemDefinitions.Add(LoadObject<UHSRItemDefinition>(nullptr, TEXT("/Game/Data/Items/DA_Item_ArchiveToken_P13.DA_Item_ArchiveToken_P13")));

	// 掉落表还会掷出六件作者遗器；提前注册它们的物品定义，让 bundle 校验在
	// 任何宝箱/战斗发放之前就能识别这些物品。
	for (const TCHAR* RelicPath :
		{ TEXT("/Game/Data/Items/Relic/DA_Item_Relic_Head.DA_Item_Relic_Head"),
		  TEXT("/Game/Data/Items/Relic/DA_Item_Relic_Hands.DA_Item_Relic_Hands"),
		  TEXT("/Game/Data/Items/Relic/DA_Item_Relic_Body.DA_Item_Relic_Body"),
		  TEXT("/Game/Data/Items/Relic/DA_Item_Relic_Feet.DA_Item_Relic_Feet"),
		  TEXT("/Game/Data/Items/Relic/DA_Item_Relic_PlanarSphere.DA_Item_Relic_PlanarSphere"),
		  TEXT("/Game/Data/Items/Relic/DA_Item_Relic_LinkRope.DA_Item_Relic_LinkRope") })
	{
		if (UHSRItemDefinition* Relic = LoadObject<UHSRItemDefinition>(nullptr, RelicPath))
		{
			ItemDefinitions.Add(Relic);
		}
	}

	UHSRDropTableDefinition* DropTable = LoadObject<UHSRDropTableDefinition>(nullptr, TEXT("/Game/Data/Drops/DA_Drop_P13_Standard.DA_Drop_P13_Standard"));
	UHSRRewardDefinition* RewardDefinition = LoadObject<UHSRRewardDefinition>(nullptr, TEXT("/Game/Data/Rewards/DA_Reward_P13_Standard.DA_Reward_P13_Standard"));

	// 只有前两个物品、掉落表、奖励定义都加载成功才注册 bundle。
	if (!ItemDefinitions.IsEmpty() && ItemDefinitions[0] && ItemDefinitions[1] && DropTable && RewardDefinition)
	{
		const EHSRRewardOperationResult Result = RegisterBundle(ItemDefinitions, *DropTable, *RewardDefinition);
		if (Result != EHSRRewardOperationResult::Success && Result != EHSRRewardOperationResult::NoOp)
		{
			UE_LOG(LogTemp, Error, TEXT("P13-004 ProductionDefinitionBootstrap=FAILED Reason=RegisterBundle Result=%d"),
				static_cast<int32>(Result));
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("P13-004 ProductionDefinitionBootstrap=READY Result=%d Items=%d DropTable=%s Reward=%s"),
				static_cast<int32>(Result), ItemDefinitions.Num(),
				*DropTable->DropTableId.ToString(), *RewardDefinition->RewardDefinitionId.ToString());
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("P13-004 ProductionDefinitionBootstrap=FAILED Reason=AssetLoad Items=%d DropTable=%s Reward=%s"),
			ItemDefinitions.Num(),
			DropTable ? TEXT("OK") : TEXT("MISSING"),
			RewardDefinition ? TEXT("OK") : TEXT("MISSING"));
	}

	// 注册 VerticalSlice 演示物品定义，让演示遭遇的奖励 bundle（固定遗器 + 掉落材料）
	// 能通过校验。六个演示遗器是 Unique 物品，成长材料是 Stackable。
	const TCHAR* DemoItemPaths[] =
	{
		TEXT("/Game/Data/VerticalSlice/Items/Relics/DA_Item_HeavenLiveRoom_LinkRope.DA_Item_HeavenLiveRoom_LinkRope"),
		TEXT("/Game/Data/VerticalSlice/Items/Relics/DA_Item_HeavenLiveRoom_PlanarSphere.DA_Item_HeavenLiveRoom_PlanarSphere"),
		TEXT("/Game/Data/VerticalSlice/Items/Relics/DA_Item_ShiningMagicalGirl_Head.DA_Item_ShiningMagicalGirl_Head"),
		TEXT("/Game/Data/VerticalSlice/Items/Relics/DA_Item_ShiningMagicalGirl_Hands.DA_Item_ShiningMagicalGirl_Hands"),
		TEXT("/Game/Data/VerticalSlice/Items/Relics/DA_Item_ShiningMagicalGirl_Body.DA_Item_ShiningMagicalGirl_Body"),
		TEXT("/Game/Data/VerticalSlice/Items/Relics/DA_Item_ShiningMagicalGirl_Feet.DA_Item_ShiningMagicalGirl_Feet"),
		TEXT("/Game/Data/VerticalSlice/Items/Materials/DA_Item_DomainEchoGrowthMaterial.DA_Item_DomainEchoGrowthMaterial"),
	};
	int32 DemoItemsRegistered = 0;
	for (const TCHAR* DemoItemPath : DemoItemPaths)
	{
		if (UHSRItemDefinition* DemoItem = LoadObject<UHSRItemDefinition>(nullptr, DemoItemPath))
		{
			const EHSRInventoryOperationResult DemoResult = Inventory->RegisterDefinition(*DemoItem);
			if (DemoResult == EHSRInventoryOperationResult::Success || DemoResult == EHSRInventoryOperationResult::NoOp)
			{
				++DemoItemsRegistered;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("P18 DemoItemBootstrap=FAILED Reason=RegisterDefinition ItemId=%s Result=%d"),
					*DemoItem->ItemId.ToString(), static_cast<int32>(DemoResult));
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("P18 DemoItemBootstrap=READY Items=%d/%d"),
		DemoItemsRegistered, static_cast<int32>(UE_ARRAY_COUNT(DemoItemPaths)));

	// 提前注册演示奖励定义：收据在奖励提交的那一刻就会被持久化，但定义此前只在
	// 发放 Actor（宝箱 BeginPlay / 战斗过渡）的副作用里注册过。冷启动时存档层在任何
	// Actor 存在之前就做校验，于是引用未注册演示奖励的收据会让整份数据判为 InvalidData、
	// 每次存档/读档都失败。演示奖励不携带掉落表（DropRolls=0），其固定物品已由上面的
	// DemoItemBootstrap 注册，因此用普通 RegisterRewardDefinition 即可。
	const TCHAR* DemoRewardPaths[] =
	{
		TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_WangXiaYiTong.DA_Reward_WangXiaYiTong"),
		TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_Laigushi.DA_Reward_Laigushi"),
		TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_SupportSectionInspector.DA_Reward_SupportSectionInspector"),
		TEXT("/Game/Data/VerticalSlice/Rewards/DA_Reward_DomainEcho.DA_Reward_DomainEcho"),
	};
	int32 DemoRewardsRegistered = 0;
	for (const TCHAR* DemoRewardPath : DemoRewardPaths)
	{
		if (UHSRRewardDefinition* DemoReward = LoadObject<UHSRRewardDefinition>(nullptr, DemoRewardPath))
		{
			const EHSRRewardOperationResult DemoResult = RegisterRewardDefinition(*DemoReward);
			if (DemoResult == EHSRRewardOperationResult::Success || DemoResult == EHSRRewardOperationResult::NoOp)
			{
				++DemoRewardsRegistered;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("P18 DemoRewardBootstrap=FAILED Reason=RegisterRewardDefinition RewardId=%s Result=%d"),
					*DemoReward->RewardDefinitionId.ToString(), static_cast<int32>(DemoResult));
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("P18 DemoRewardBootstrap=READY Rewards=%d/%d"),
		DemoRewardsRegistered, static_cast<int32>(UE_ARRAY_COUNT(DemoRewardPaths)));
}

#if WITH_DEV_AUTOMATION_TESTS
// 自动化测试专用：注入背包子系统。
void UHSRRewardSubsystem::InitializeForAutomation(UHSRInventorySubsystem* InInventory)
{
	Inventory = InInventory;
}
#endif

#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
// 开发/测试专用：注入背包子系统。
void UHSRRewardSubsystem::InitializeForDevelopmentTest(UHSRInventorySubsystem* InInventory)
{
	Inventory = InInventory;
}
#endif

// RegisterDropTable：注册一张掉落表（先校验）。
EHSRRewardOperationResult UHSRRewardSubsystem::RegisterDropTable(const UHSRDropTableDefinition& Definition)
{
	const EHSRRewardOperationResult Validation = CanRegisterDropTable(Definition);
	if (Validation != EHSRRewardOperationResult::Success)
	{
		return Validation;
	}
	DropTables.Add(Definition.DropTableId, {Definition.DropTableId, Definition.Entries});
	return EHSRRewardOperationResult::Success;
}

// CanRegisterDropTable：校验掉落表。规则：ID 非空、条目数在界内且非空、每条掉落
// 的物品 ID 非空且不重复、数量区间合法、权重为正且总权重不溢出。已注册时内容一致
// 则 NoOp，否则 DuplicateDefinitionId。
EHSRRewardOperationResult UHSRRewardSubsystem::CanRegisterDropTable(const UHSRDropTableDefinition& Definition) const
{
	if (Definition.DropTableId.IsNone())
	{
		return EHSRRewardOperationResult::InvalidDefinitionId;
	}
	if (Definition.Entries.IsEmpty() || Definition.Entries.Num() > FHSRRewardResolver::MaxDefinitionEntries)
	{
		return EHSRRewardOperationResult::InvalidDefinition;
	}

	TSet<FName> SeenItems;
	int32 TotalWeight = 0;
	for (const FHSRDropTableEntry& Entry : Definition.Entries)
	{
		if (Entry.ItemId.IsNone() || SeenItems.Contains(Entry.ItemId)
			|| Entry.MinQuantity <= 0 || Entry.MaxQuantity < Entry.MinQuantity
			|| Entry.Weight <= 0 || Entry.Weight > MAX_int32 - TotalWeight)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		SeenItems.Add(Entry.ItemId);
		TotalWeight += Entry.Weight;
	}

	if (const FHSRDropTableRule* Existing = DropTables.Find(Definition.DropTableId))
	{
		return AreDropEntriesEqual(Existing->Entries, Definition.Entries)
			? EHSRRewardOperationResult::NoOp
			: EHSRRewardOperationResult::DuplicateDefinitionId;
	}
	return EHSRRewardOperationResult::Success;
}

// RegisterRewardDefinition：注册一条奖励定义（先校验）。
EHSRRewardOperationResult UHSRRewardSubsystem::RegisterRewardDefinition(const UHSRRewardDefinition& Definition)
{
	const EHSRRewardOperationResult Validation = CanRegisterRewardDefinition(Definition);
	if (Validation != EHSRRewardOperationResult::Success)
	{
		return Validation;
	}
	Rewards.Add(Definition.RewardDefinitionId, {Definition.RewardDefinitionId, Definition.FixedItems, Definition.DropTableId, Definition.DropRolls});
	return EHSRRewardOperationResult::Success;
}

// CanRegisterRewardDefinition：校验奖励定义。规则：ID 非空；固定物品条数与掉落次数
// 在界内；掉落表 ID 与掉落次数的存在性一致（有表必须能掷，没表必须不掷）；不能同时
// 既无固定物品又无掉落；固定物品去重且数量为正；引用的掉落表必须已注册（或等于本次
// 同步注册的 AdditionalDropTableId）。已注册时内容一致则 NoOp。
EHSRRewardOperationResult UHSRRewardSubsystem::CanRegisterRewardDefinition(const UHSRRewardDefinition& Definition, FName AdditionalDropTableId) const
{
	if (Definition.RewardDefinitionId.IsNone())
	{
		return EHSRRewardOperationResult::InvalidDefinitionId;
	}
	if (Definition.FixedItems.Num() > FHSRRewardResolver::MaxDefinitionEntries
		|| Definition.DropRolls < 0 || Definition.DropRolls > FHSRRewardResolver::MaxDropRolls
		|| (Definition.DropTableId.IsNone() != (Definition.DropRolls == 0))
		|| (Definition.FixedItems.IsEmpty() && Definition.DropRolls == 0))
	{
		return EHSRRewardOperationResult::InvalidDefinition;
	}

	TSet<FName> SeenFixedItems;
	for (const FHSRRewardItemEntry& Entry : Definition.FixedItems)
	{
		if (Entry.ItemId.IsNone() || SeenFixedItems.Contains(Entry.ItemId) || Entry.Quantity <= 0)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		SeenFixedItems.Add(Entry.ItemId);
	}
	if (!Definition.DropTableId.IsNone() && !DropTables.Contains(Definition.DropTableId) && Definition.DropTableId != AdditionalDropTableId)
	{
		return EHSRRewardOperationResult::UnknownDropTable;
	}
	if (const FHSRRewardDefinitionRule* Existing = Rewards.Find(Definition.RewardDefinitionId))
	{
		return Existing->DropTableId == Definition.DropTableId && Existing->DropRolls == Definition.DropRolls
			&& AreRewardEntriesEqual(Existing->FixedItems, Definition.FixedItems)
			? EHSRRewardOperationResult::NoOp
			: EHSRRewardOperationResult::DuplicateDefinitionId;
	}
	return EHSRRewardOperationResult::Success;
}

// CanRegisterBundle：校验一个「物品集合 + 掉落表 + 奖励定义」能否整体注册。
// 校验点：所有物品定义能注册（或已一致注册）；掉落表/固定物品里出现的每个物品都必须
// 已知（要么在本 bundle 的物品集合里，要么已在背包子系统注册）；掉落表与奖励定义各自
// 通过校验。任一「实际变化」发生则返回 Success，否则 NoOp。
EHSRRewardOperationResult UHSRRewardSubsystem::CanRegisterBundle(
	const TArray<TObjectPtr<UHSRItemDefinition>>& ItemDefinitions,
	const UHSRDropTableDefinition& DropTable,
	const UHSRRewardDefinition& RewardDefinition) const
{
	UHSRInventorySubsystem* InventorySubsystem = Inventory.Get();
	if (!InventorySubsystem)
	{
		return EHSRRewardOperationResult::InventoryRejected;
	}

	bool bAnyChange = false;
	TMap<FName, const UHSRItemDefinition*> BundleItems;
	for (const UHSRItemDefinition* ItemDefinition : ItemDefinitions)
	{
		if (!ItemDefinition)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		// bundle 内同名物品的属性必须一致。
		if (const UHSRItemDefinition* const* ExistingBundleItem = BundleItems.Find(ItemDefinition->ItemId))
		{
			if ((*ExistingBundleItem)->StorageKind != ItemDefinition->StorageKind
				|| (*ExistingBundleItem)->MaxStack != ItemDefinition->MaxStack)
			{
				return EHSRRewardOperationResult::InvalidDefinition;
			}
		}
		else
		{
			BundleItems.Add(ItemDefinition->ItemId, ItemDefinition);
		}
		const EHSRInventoryOperationResult Result = InventorySubsystem->CanRegisterDefinition(*ItemDefinition);
		if (Result != EHSRInventoryOperationResult::Success && Result != EHSRInventoryOperationResult::NoOp)
		{
			return EHSRRewardOperationResult::InvalidDefinition;
		}
		bAnyChange |= Result == EHSRInventoryOperationResult::Success;
	}

	// 物品已知性检查：掉落表与固定物品里引用的物品必须已注册或属于本 bundle。
	auto IsItemKnown = [InventorySubsystem, &BundleItems](FName ItemId)
	{
		EHSRItemStorageKind StorageKind;
		int32 MaxStack = 0;
		return BundleItems.Contains(ItemId) || InventorySubsystem->GetDefinitionInfo(ItemId, StorageKind, MaxStack);
	};
	for (const FHSRDropTableEntry& Entry : DropTable.Entries)
	{
		if (!IsItemKnown(Entry.ItemId))
		{
			UE_LOG(LogTemp, Error, TEXT("P13-003 RewardBundle Result=REJECTED Reason=UnknownDropItem ItemId=%s"), *Entry.ItemId.ToString());
			return EHSRRewardOperationResult::InvalidDefinition;
		}
	}
	for (const FHSRRewardItemEntry& Entry : RewardDefinition.FixedItems)
	{
		if (!IsItemKnown(Entry.ItemId))
		{
			UE_LOG(LogTemp, Error, TEXT("P13-003 RewardBundle Result=REJECTED Reason=UnknownFixedItem ItemId=%s"), *Entry.ItemId.ToString());
			return EHSRRewardOperationResult::InvalidDefinition;
		}
	}

	// 掉落表与奖励定义各自校验（允许 NoOp）。
	const EHSRRewardOperationResult DropValidation = CanRegisterDropTable(DropTable);
	if (DropValidation != EHSRRewardOperationResult::Success && DropValidation != EHSRRewardOperationResult::NoOp)
	{
		return DropValidation;
	}
	const EHSRRewardOperationResult RewardValidation = CanRegisterRewardDefinition(RewardDefinition, DropTable.DropTableId);
	if (RewardValidation != EHSRRewardOperationResult::Success && RewardValidation != EHSRRewardOperationResult::NoOp)
	{
		return RewardValidation;
	}

	bAnyChange |= DropValidation == EHSRRewardOperationResult::Success || RewardValidation == EHSRRewardOperationResult::Success;
	return bAnyChange ? EHSRRewardOperationResult::Success : EHSRRewardOperationResult::NoOp;
}

// RegisterBundle：整体注册一个奖励 bundle（先 CanRegisterBundle 校验，通过后分别注册
// 物品/掉落表/奖励定义）。
EHSRRewardOperationResult UHSRRewardSubsystem::RegisterBundle(
	const TArray<TObjectPtr<UHSRItemDefinition>>& ItemDefinitions,
	const UHSRDropTableDefinition& DropTable,
	const UHSRRewardDefinition& RewardDefinition)
{
	const EHSRRewardOperationResult Validation = CanRegisterBundle(ItemDefinitions, DropTable, RewardDefinition);
	if (Validation != EHSRRewardOperationResult::Success && Validation != EHSRRewardOperationResult::NoOp)
	{
		return Validation;
	}

	UHSRInventorySubsystem* InventorySubsystem = Inventory.Get();
	check(InventorySubsystem);
	for (const UHSRItemDefinition* ItemDefinition : ItemDefinitions)
	{
		InventorySubsystem->RegisterDefinition(*ItemDefinition);
	}
	RegisterDropTable(DropTable);
	RegisterRewardDefinition(RewardDefinition);
	return Validation;
}

// SubmitReward：提交一次奖励发放。幂等语义：同一 ClaimId 重复提交时，若请求一致则
// 返回既有收据（NoOp），否则 ClaimConflict。首次提交会解析定义 -> 生成 Grants ->
// 应用到背包 -> 记录收据并广播。
EHSRRewardOperationResult UHSRRewardSubsystem::SubmitReward(const FHSRRewardRequest& Request, FHSRRewardReceipt& OutReceipt)
{
	if (!Request.ClaimId.IsValid())
	{
		return EHSRRewardOperationResult::InvalidClaimId;
	}
	if (Request.RewardDefinitionId.IsNone())
	{
		return EHSRRewardOperationResult::InvalidDefinitionId;
	}

	// 幂等：ClaimId 已存在。
	if (const FHSRRewardReceipt* Existing = Receipts.Find(Request.ClaimId))
	{
		if (Existing->Request.RewardDefinitionId != Request.RewardDefinitionId || Existing->Request.Seed != Request.Seed)
		{
			return EHSRRewardOperationResult::ClaimConflict;
		}
		OutReceipt = *Existing;
		return EHSRRewardOperationResult::NoOp;
	}

	const FHSRRewardDefinitionRule* Reward = Rewards.Find(Request.RewardDefinitionId);
	if (!Reward)
	{
		return EHSRRewardOperationResult::UnknownRewardDefinition;
	}
	if (!Inventory.IsValid())
	{
		return EHSRRewardOperationResult::InventoryRejected;
	}

	// 解析定义成发放清单。
	TArray<FHSRInventoryGrant> Grants;
	if (!BuildGrants(Request, *Reward, Grants))
	{
		return EHSRRewardOperationResult::ResolveFailed;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (bInjectCommitFailure)
	{
		return EHSRRewardOperationResult::InjectedFailure;
	}
#endif

	// 应用发放（静默，不广播），成功后才记录收据并广播一次。
	int64 InventoryRevision = 0;
	if (Inventory->ApplyGrantsInternal(Grants, false, InventoryRevision) != EHSRInventoryOperationResult::Success)
	{
		return EHSRRewardOperationResult::InventoryRejected;
	}

	FHSRRewardReceipt Receipt;
	Receipt.Request = Request;
	Receipt.Grants = MoveTemp(Grants);
	Receipt.Revision = ++Revision;
	Receipts.Add(Request.ClaimId, Receipt);
	OutReceipt = Receipt;
	Inventory->BroadcastRevision(InventoryRevision);
	RewardCommitted.Broadcast(Receipt);
	return EHSRRewardOperationResult::Success;
}

// GetReceipt：按 ClaimId 查收据。
bool UHSRRewardSubsystem::GetReceipt(const FGuid& ClaimId, FHSRRewardReceipt& OutReceipt) const
{
	const FHSRRewardReceipt* Receipt = Receipts.Find(ClaimId);
	if (!Receipt)
	{
		return false;
	}
	OutReceipt = *Receipt;
	return true;
}

// GetReceipts：导出全部收据，按 ClaimId 排序（存档编码依赖稳定顺序）。
void UHSRRewardSubsystem::GetReceipts(TArray<FHSRRewardReceipt>& OutReceipts) const
{
	Receipts.GenerateValueArray(OutReceipts);
	OutReceipts.Sort([](const FHSRRewardReceipt& A, const FHSRRewardReceipt& B)
	{
		return A.Request.ClaimId < B.Request.ClaimId;
	});
}

// ExportSaveData：导出存档用的奖励数据。
void UHSRRewardSubsystem::ExportSaveData(FHSRRewardSaveData& OutData) const
{
	GetReceipts(OutData.Receipts);
	OutData.Revision = Revision;
}

// PrepareRestore：读档恢复干跑。校验：版本号非负；每张收据的 ClaimId/定义合法、定义
// 已注册、收据版本号在 1..Data.Revision 且不重复、发放的物品定义存在且存储类型与
// 发放形态匹配、唯一物品实例 ID 合法且不重复；收据数量必须等于版本号（收据版本号
// 从 1 连续编号），且最大收据版本号等于 Data.Revision。
bool UHSRRewardSubsystem::PrepareRestore(const FHSRRewardSaveData& Data, FHSRRewardRestoreState& OutCandidate) const
{
	if (Data.Revision < 0)
	{
		return false;
	}

	FHSRRewardRestoreState Candidate;
	Candidate.Revision = Data.Revision;
	int64 MaxReceiptRevision = 0;
	TSet<int64> SeenReceiptRevisions;
	TSet<FGuid> SeenInstances;
	for (const FHSRRewardReceipt& Receipt : Data.Receipts)
	{
		if (!Receipt.Request.ClaimId.IsValid() || Receipt.Request.RewardDefinitionId.IsNone()
			|| !Rewards.Contains(Receipt.Request.RewardDefinitionId)
			|| Receipt.Revision <= 0 || Receipt.Revision > Data.Revision
			|| SeenReceiptRevisions.Contains(Receipt.Revision)
			|| Candidate.Receipts.Contains(Receipt.Request.ClaimId) || Receipt.Grants.IsEmpty())
		{
			return false;
		}
		SeenReceiptRevisions.Add(Receipt.Revision);
		for (const FHSRInventoryGrant& Grant : Receipt.Grants)
		{
			EHSRItemStorageKind StorageKind;
			int32 MaxStack = 0;
			if (Grant.ItemId.IsNone() || Grant.Quantity <= 0 || !Inventory.IsValid()
				|| !Inventory->GetDefinitionInfo(Grant.ItemId, StorageKind, MaxStack)
				|| (StorageKind == EHSRItemStorageKind::Stackable && !Grant.InstanceIds.IsEmpty())
				|| (StorageKind == EHSRItemStorageKind::Unique && Grant.InstanceIds.Num() != Grant.Quantity))
			{
				return false;
			}
			for (const FGuid& InstanceId : Grant.InstanceIds)
			{
				if (!InstanceId.IsValid() || SeenInstances.Contains(InstanceId))
				{
					return false;
				}
				SeenInstances.Add(InstanceId);
			}
		}
		MaxReceiptRevision = FMath::Max(MaxReceiptRevision, Receipt.Revision);
		Candidate.Receipts.Add(Receipt.Request.ClaimId, Receipt);
	}
	if (Data.Revision != Data.Receipts.Num() || MaxReceiptRevision != Data.Revision)
	{
		return false;
	}
	OutCandidate = MoveTemp(Candidate);
	return true;
}

// IsRestoreDifferent：判断候选恢复状态与当前奖励运行时是否不同。
bool UHSRRewardSubsystem::IsRestoreDifferent(const FHSRRewardRestoreState& Candidate) const
{
	if (Revision != Candidate.Revision || Receipts.Num() != Candidate.Receipts.Num())
	{
		return true;
	}
	for (const TPair<FGuid, FHSRRewardReceipt>& Entry : Receipts)
	{
		const FHSRRewardReceipt* Other = Candidate.Receipts.Find(Entry.Key);
		if (!Other
			|| Other->Request.RewardDefinitionId != Entry.Value.Request.RewardDefinitionId
			|| Other->Request.Seed != Entry.Value.Request.Seed
			|| Other->Revision != Entry.Value.Revision
			|| Other->Grants.Num() != Entry.Value.Grants.Num())
		{
			return true;
		}
		for (int32 Index = 0; Index < Entry.Value.Grants.Num(); ++Index)
		{
			const FHSRInventoryGrant& A = Entry.Value.Grants[Index];
			const FHSRInventoryGrant& B = Other->Grants[Index];
			if (A.ItemId != B.ItemId || A.Quantity != B.Quantity || A.InstanceIds != B.InstanceIds)
			{
				return true;
			}
		}
	}
	return false;
}

// CommitRestore：提交恢复后的奖励状态。bNotify 控制是否广播 RewardRestored。
void UHSRRewardSubsystem::CommitRestore(FHSRRewardRestoreState&& Candidate, bool bNotify)
{
	Receipts = MoveTemp(Candidate.Receipts);
	Revision = Candidate.Revision;
	if (bNotify)
	{
		RewardRestored.Broadcast(Revision);
	}
}

// PrepareSettlementCandidate：结算事务的奖励预演。幂等：TransactionId 已在结算台账里
// 且请求一致则 NoOp；否则校验期望版本号、解析奖励定义、生成 Grants，并把收据与台账
// 条目写进候选（版本号 = Revision + 1）。
EHSRRewardOperationResult UHSRRewardSubsystem::PrepareSettlementCandidate(const FHSRSettlementRequest& Request,
	FHSRRewardSettlementCandidate& OutCandidate, FHSRSettlementReceipt& OutPreparedReceipt,
	TArray<FHSRInventoryGrant>& OutGrants, FHSRSettlementReceipt& OutExistingReceipt) const
{
	// 幂等：事务已在台账中。
	if (const FHSRSettlementReceipt* Existing = SettlementLedger.Find(Request.TransactionId))
	{
		if (Existing->RewardDefinitionId != Request.RewardDefinitionId
			|| Existing->PlayerCharacterId != Request.PlayerCharacterId
			|| Existing->RewardSeed != Request.RewardSeed
			|| Existing->Experience != Request.Experience
			|| Existing->ExpectedInventoryRevision != Request.ExpectedInventoryRevision
			|| Existing->ExpectedProfileRevision != Request.ExpectedProfileRevision
			|| Existing->ExpectedRewardRevision != Request.ExpectedRewardRevision)
		{
			return EHSRRewardOperationResult::ClaimConflict;
		}
		OutExistingReceipt = *Existing;
		return EHSRRewardOperationResult::NoOp;
	}

	if (Request.ExpectedRewardRevision != Revision)
	{
		return EHSRRewardOperationResult::RevisionConflict;
	}
	const FHSRRewardDefinitionRule* Rule = Rewards.Find(Request.RewardDefinitionId);
	if (!Rule)
	{
		return EHSRRewardOperationResult::UnknownRewardDefinition;
	}

	// 解析奖励定义成发放清单。
	FHSRRewardRequest RewardRequest;
	RewardRequest.ClaimId = Request.TransactionId;
	RewardRequest.RewardDefinitionId = Request.RewardDefinitionId;
	RewardRequest.Seed = Request.RewardSeed;
	if (!BuildGrants(RewardRequest, *Rule, OutGrants))
	{
		return EHSRRewardOperationResult::ResolveFailed;
	}

	// 构造候选：把「将被发布的奖励收据」与「结算台账条目」都放进候选。
	FHSRRewardSettlementCandidate Candidate;
	Candidate.TransactionId = Request.TransactionId;
	Candidate.Receipts = Receipts;
	Candidate.SettlementLedger = SettlementLedger;
	Candidate.NextRevision = Revision + 1;
	Candidate.PublishedRewardReceipt.Request = RewardRequest;
	Candidate.PublishedRewardReceipt.Grants = OutGrants;
	Candidate.PublishedRewardReceipt.Revision = Candidate.NextRevision;
	Candidate.Receipts.Add(Request.TransactionId, Candidate.PublishedRewardReceipt);

	FHSRSettlementReceipt Receipt;
	Receipt.TransactionId = Request.TransactionId;
	Receipt.RewardDefinitionId = Request.RewardDefinitionId;
	Receipt.PlayerCharacterId = Request.PlayerCharacterId;
	Receipt.RewardSeed = Request.RewardSeed;
	Receipt.Experience = Request.Experience;
	Receipt.ExpectedInventoryRevision = Request.ExpectedInventoryRevision;
	Receipt.ExpectedProfileRevision = Request.ExpectedProfileRevision;
	Receipt.ExpectedRewardRevision = Request.ExpectedRewardRevision;
	Receipt.RewardReceipt = Candidate.PublishedRewardReceipt;
	Receipt.RewardRevision = Candidate.NextRevision;
	Candidate.SettlementLedger.Add(Request.TransactionId, Receipt);

	OutPreparedReceipt = Receipt;
	OutCandidate = MoveTemp(Candidate);
	return EHSRRewardOperationResult::Success;
}

// InstallSettlementCandidateNoFail：结算提交阶段安装候选状态。
void UHSRRewardSubsystem::InstallSettlementCandidateNoFail(FHSRRewardSettlementCandidate&& Candidate)
{
	Receipts = MoveTemp(Candidate.Receipts);
	SettlementLedger = MoveTemp(Candidate.SettlementLedger);
}

// FinalizeSettlementRevisionNoFail：结算提交阶段推进版本号。
void UHSRRewardSubsystem::FinalizeSettlementRevisionNoFail(int64 PreparedRevision)
{
	Revision = PreparedRevision;
}

// PublishSettlementCommit：结算提交阶段广播奖励提交事件。
void UHSRRewardSubsystem::PublishSettlementCommit(const FHSRRewardReceipt& PreparedReceipt, int64 PreparedRevision)
{
	RewardCommitted.Broadcast(PreparedReceipt);
}

// MakeInstanceId：由「ClaimId + 物品 ID + 序号」派生唯一实例 ID。用物品 ID 的哈希
// 与序号异或进 ClaimId 的四个分量，既保证同一奖励内不同物品/序号的实例 ID 不同，
// 又保证同一奖励重放时能复现完全相同的实例 ID（存档/结算幂等性的基石）。
FGuid UHSRRewardSubsystem::MakeInstanceId(const FGuid& ClaimId, FName ItemId, int32 Ordinal)
{
	const uint32 ItemHash = FCrc::StrCrc32(*ItemId.ToString());
	FGuid Result(ClaimId.A ^ ItemHash, ClaimId.B ^ static_cast<uint32>(Ordinal + 1),
		ClaimId.C ^ (ItemHash << 1), ClaimId.D ^ static_cast<uint32>(Ordinal + 17));
	if (!Result.IsValid())
	{
		Result.D = 1;
	}
	return Result;
}

// BuildGrants：把解析出的物品清单转成背包发放（Grants）。唯一物品需要为每个实例
// 生成稳定的实例 ID（见 MakeInstanceId），并校验背包容量足够容纳这些唯一实例。
bool UHSRRewardSubsystem::BuildGrants(const FHSRRewardRequest& Request, const FHSRRewardDefinitionRule& Reward, TArray<FHSRInventoryGrant>& OutGrants) const
{
	const FHSRDropTableRule* DropTable = Reward.DropTableId.IsNone() ? nullptr : DropTables.Find(Reward.DropTableId);
	TArray<FHSRRewardItemEntry> ResolvedItems;
	if (!FHSRRewardResolver::Resolve(Reward, DropTable, Request.Seed, ResolvedItems))
	{
		return false;
	}

	OutGrants.Reset();
	FHSRInventorySnapshot InventorySnapshot;
	Inventory->GetSnapshot(InventorySnapshot);
	int32 UniqueOrdinal = 0;
	int32 TotalUniqueInstances = 0;
	for (const FHSRRewardItemEntry& Item : ResolvedItems)
	{
		EHSRItemStorageKind StorageKind;
		int32 MaxStack = 0;
		if (!Inventory->GetDefinitionInfo(Item.ItemId, StorageKind, MaxStack))
		{
			UE_LOG(LogTemp, Warning, TEXT("P13-003 RewardResolve Result=FAILED Reason=UnknownItem ItemId=%s ClaimId=%s"),
				*Item.ItemId.ToString(), *Request.ClaimId.ToString());
			return false;
		}

		FHSRInventoryGrant Grant;
		Grant.ItemId = Item.ItemId;
		Grant.Quantity = Item.Quantity;
		if (StorageKind == EHSRItemStorageKind::Unique)
		{
			// 唯一物品：数量必须能在背包容量内放下。
			if (Item.Quantity > InventorySnapshot.Capacity - TotalUniqueInstances)
			{
				return false;
			}
			TotalUniqueInstances += Item.Quantity;
			for (int32 Index = 0; Index < Item.Quantity; ++Index)
			{
				Grant.InstanceIds.Add(MakeInstanceId(Request.ClaimId, Item.ItemId, UniqueOrdinal++));
			}
		}
		OutGrants.Add(MoveTemp(Grant));
	}
	return true;
}
