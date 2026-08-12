#include "HSREquipmentSubsystem.h"

#include "../Data/Definitions/HSREquipmentDefinition.h"
#include "../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../Data/Definitions/HSRRelicDefinition.h"
#include "../Data/Definitions/HSRRelicSetDefinition.h"
#include "../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "HSRRelicSetResolver.h"
#include "../Inventory/HSRInventorySubsystem.h"
#include "../Save/HSRSaveTypes.h"

#include <cmath>

// Initialize：启动时注册生产环境的遗器/套装定义。这样通过掉落或奖励进入背包的遗器
// 在穿戴时能解析到已知定义（ExecuteMovement 会用 Definitions.Find(DefinitionId) 校验）。
// 若缺了这一步，六个 DA_Relic_* 资源在运行时从未被加载，每一次遗器穿戴都会被
// MappingRejected 拒绝。
void UHSREquipmentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 注册正式遗器定义（P12 生产资源）。
	const TCHAR* RelicPaths[] = {
		TEXT("/Game/Data/Relics/DA_Relic_Head.DA_Relic_Head"),
		TEXT("/Game/Data/Relics/DA_Relic_Hands.DA_Relic_Hands"),
		TEXT("/Game/Data/Relics/DA_Relic_Body.DA_Relic_Body"),
		TEXT("/Game/Data/Relics/DA_Relic_Feet.DA_Relic_Feet"),
		TEXT("/Game/Data/Relics/DA_Relic_PlanarSphere.DA_Relic_PlanarSphere"),
		TEXT("/Game/Data/Relics/DA_Relic_LinkRope.DA_Relic_LinkRope"),
	};
	for (const TCHAR* Path : RelicPaths)
	{
		if (UHSRRelicDefinition* Relic = LoadObject<UHSRRelicDefinition>(nullptr, Path))
		{
			RegisterDefinition(*Relic);
		}
	}
	if (UHSRRelicSetDefinition* Set = LoadObject<UHSRRelicSetDefinition>(nullptr, TEXT("/Game/Data/RelicSets/DA_RelicSet_P12_A.DA_RelicSet_P12_A")))
	{
		RegisterSetDefinition(*Set);
	}

	// 注册 VerticalSlice 演示遗器定义。演示包里的遗器（由宝箱与演示遭遇发放）在穿戴时
	// 需要能解析到已知定义，否则 ExecuteMovement 会把每个 Demo.Relic.* 的穿戴都判为
	// MappingRejected。
	const TCHAR* DemoRelicPaths[] = {
		TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_HeavenLiveRoom_LinkRope.DA_Relic_HeavenLiveRoom_LinkRope"),
		TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_HeavenLiveRoom_PlanarSphere.DA_Relic_HeavenLiveRoom_PlanarSphere"),
		TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_ShiningMagicalGirl_Head.DA_Relic_ShiningMagicalGirl_Head"),
		TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_ShiningMagicalGirl_Hands.DA_Relic_ShiningMagicalGirl_Hands"),
		TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_ShiningMagicalGirl_Body.DA_Relic_ShiningMagicalGirl_Body"),
		TEXT("/Game/Data/VerticalSlice/Relics/Pieces/DA_Relic_ShiningMagicalGirl_Feet.DA_Relic_ShiningMagicalGirl_Feet"),
	};
	for (const TCHAR* Path : DemoRelicPaths)
	{
		if (UHSRRelicDefinition* Relic = LoadObject<UHSRRelicDefinition>(nullptr, Path))
		{
			RegisterDefinition(*Relic);
		}
	}
	if (UHSRRelicSetDefinition* MagicalGirl = LoadObject<UHSRRelicSetDefinition>(nullptr, TEXT("/Game/Data/VerticalSlice/Relics/Sets/DA_RelicSet_ShiningMagicalGirl.DA_RelicSet_ShiningMagicalGirl")))
	{
		RegisterSetDefinition(*MagicalGirl);
	}
	if (UHSRRelicSetDefinition* LiveRoom = LoadObject<UHSRRelicSetDefinition>(nullptr, TEXT("/Game/Data/VerticalSlice/Relics/Sets/DA_RelicSet_HeavenLiveRoom.DA_RelicSet_HeavenLiveRoom")))
	{
		RegisterSetDefinition(*LiveRoom);
	}
	UE_LOG(LogTemp, Log, TEXT("HSR.Equipment Bootstrap RelicDefinitions=%d HasHead=%d HasSet=%d"),
		Definitions.Num(), Definitions.Contains(TEXT("Relic.P12.Head")) ? 1 : 0,
		SetThresholds.Contains(TEXT("Set.P12.A")) ? 1 : 0);
}

// ExportSaveData：把当前装备状态导出为旧 schema（<7）的扁平装备 DTO 数组。
// 这是从「Loadout（按角色槽位索引） + InstanceRegistry（实例详情）」投影成扁平行：
// 每行含角色 GUID、实例详情、槽位种类、权威版本号。按角色/种类/槽位/实例 ID 排序，
// 保证存档字节可复现。
void UHSREquipmentSubsystem::ExportSaveData(TArray<FHSREquipmentSaveDto>& Out) const
{
	Out.Reset();
	for (const auto& L : Loadouts)
	{
		for (const auto& P : L.Value.Equipment)
		{
			const FHSREquipmentInstance* I = InstanceRegistry.Find(P.Value);
			if (!I)
			{
				continue;
			}
			FHSREquipmentSaveDto D;
			D.CharacterId = L.Key;
			D.DefinitionId = I->DefinitionId;
			D.InstanceId = I->InstanceId;
			D.Kind = 0;
			D.Slot = (int32)P.Key;
			D.EnhancementLevel = I->EnhancementLevel;
			D.Modifiers = I->Modifiers;
			D.AuthorityRevision = L.Value.Revision;
			Out.Add(D);
		}
		for (const auto& P : L.Value.Relics)
		{
			const FHSREquipmentInstance* I = InstanceRegistry.Find(P.Value);
			if (!I)
			{
				continue;
			}
			FHSREquipmentSaveDto D;
			D.CharacterId = L.Key;
			D.DefinitionId = I->DefinitionId;
			D.InstanceId = I->InstanceId;
			D.Kind = 1;
			D.Slot = (int32)P.Key;
			D.EnhancementLevel = I->EnhancementLevel;
			D.Modifiers = I->Modifiers;
			D.AuthorityRevision = L.Value.Revision;
			// 遗器带套装 ID（从定义规则里查出来）。
			if (const FDefinitionRule* Rule = Definitions.Find(D.DefinitionId))
			{
				D.SetId = Rule->SetId;
			}
			Out.Add(D);
		}
	}
	Out.Sort([](const FHSREquipmentSaveDto& A, const FHSREquipmentSaveDto& B)
	{
		if (A.CharacterId != B.CharacterId)
		{
			return A.CharacterId < B.CharacterId;
		}
		if (A.Kind != B.Kind)
		{
			return A.Kind < B.Kind;
		}
		if (A.Slot != B.Slot)
		{
			return A.Slot < B.Slot;
		}
		return A.InstanceId < B.InstanceId;
	});
}

// ExportSaveData（registry 版本）：把当前装备状态导出为「注册表 + 摆放」两份表，
// 供 schema >= 7 的存档使用。注册表列出所有实例详情（不含归属）；摆放表列出每个
// 角色槽位上放了哪个实例。两者各自排序。
void UHSREquipmentSubsystem::ExportSaveData(TArray<FHSREquipmentRegistryDto>& OutRegistry, TArray<FHSREquipmentPlacementDto>& OutPlacements) const
{
	OutRegistry.Reset();
	OutPlacements.Reset();

	// 注册表：遍历全部实例。
	for (const auto& Pair : InstanceRegistry)
	{
		FHSREquipmentRegistryDto Row;
		Row.InstanceId = Pair.Key;
		Row.DefinitionId = Pair.Value.DefinitionId;
		Row.Kind = static_cast<int32>(Pair.Value.Kind);
		Row.EnhancementLevel = Pair.Value.EnhancementLevel;
		Row.Modifiers = Pair.Value.Modifiers;
		if (const FDefinitionRule* Rule = Definitions.Find(Row.DefinitionId))
		{
			Row.SetId = Rule->SetId;
		}
		OutRegistry.Add(MoveTemp(Row));
	}

	// 摆放表：遍历每个角色的每个槽位。
	for (const auto& Owner : Loadouts)
	{
		for (const auto& Pair : Owner.Value.Equipment)
		{
			FHSREquipmentPlacementDto Row;
			Row.InstanceId = Pair.Value;
			Row.CharacterId = Owner.Key;
			Row.Kind = 0;
			Row.Slot = static_cast<int32>(Pair.Key);
			Row.AuthorityRevision = Owner.Value.Revision;
			OutPlacements.Add(Row);
		}
		for (const auto& Pair : Owner.Value.Relics)
		{
			FHSREquipmentPlacementDto Row;
			Row.InstanceId = Pair.Value;
			Row.CharacterId = Owner.Key;
			Row.Kind = 1;
			Row.Slot = static_cast<int32>(Pair.Key);
			Row.AuthorityRevision = Owner.Value.Revision;
			OutPlacements.Add(Row);
		}
	}
	OutRegistry.Sort([](const auto& A, const auto& B) { return A.InstanceId < B.InstanceId; });
	OutPlacements.Sort([](const auto& A, const auto& B)
	{
		if (A.CharacterId != B.CharacterId)
		{
			return A.CharacterId < B.CharacterId;
		}
		if (A.Kind != B.Kind)
		{
			return A.Kind < B.Kind;
		}
		if (A.Slot != B.Slot)
		{
			return A.Slot < B.Slot;
		}
		return A.InstanceId < B.InstanceId;
	});
}

// ValidateRestoreInstance：校验一个待恢复的装备实例行：槽位合法、定义存在且种类/槽位
// 匹配、强化等级在定义上限内、词条合法；遗器还要求套装 ID 与定义一致。
bool UHSREquipmentSubsystem::ValidateRestoreInstance(FName DefinitionId, EHSREquipmentKind Kind, int32 Slot,
	int32 EnhancementLevel, FName SetId, const TArray<FHSREquipmentModifier>& Modifiers) const
{
	if (!IsSlotValid(Kind, Slot))
	{
		return false;
	}
	const FDefinitionRule* Rule = Definitions.Find(DefinitionId);
	if (!Rule || Rule->Kind != Kind || Rule->Slot != Slot)
	{
		return false;
	}
	if (EnhancementLevel < 0 || EnhancementLevel > Rule->EnhancementCap)
	{
		return false;
	}
	if (!IsValidModifiers(Modifiers))
	{
		return false;
	}
	return Kind != EHSREquipmentKind::Relic || SetId == Rule->SetId;
}

// InsertIntoRestoreState：把一件实例插入到恢复状态里。武器槽位冲突则失败；遗器在
// 槽位不冲突时还会累加套装计数（RelicSetCounts 里该套装 +1）。
bool UHSREquipmentSubsystem::InsertIntoRestoreState(FHSREquipmentRestoreState& State,
	const FHSREquipmentInstance& Instance, int32 Slot, FName SetId)
{
	FHSREquipmentLoadout& Loadout = State.Loadout;
	if (Instance.Kind == EHSREquipmentKind::Equipment)
	{
		const EHSREquipmentSlot EquipmentSlot = static_cast<EHSREquipmentSlot>(Slot);
		if (Loadout.Equipment.Contains(EquipmentSlot))
		{
			return false;
		}
		Loadout.Equipment.Add(EquipmentSlot, Instance);
		return true;
	}

	const EHSRRelicSlot RelicSlot = static_cast<EHSRRelicSlot>(Slot);
	if (Loadout.Relics.Contains(RelicSlot))
	{
		return false;
	}
	Loadout.Relics.Add(RelicSlot, Instance);
	++State.RelicSetCounts.FindOrAdd(SetId);
	return true;
}

// PrepareRestore（扁平数组版）：把存档里的扁平装备 DTO 数组解析成恢复候选。
// 校验每个 DTO 的实例/角色 GUID 合法、无重复实例、权威版本号非负、整行能通过
// ValidateRestoreInstance，并且同一角色的多行版本号必须一致。
bool UHSREquipmentSubsystem::PrepareRestore(const TArray<FHSREquipmentSaveDto>& In, FHSREquipmentRestoreMap& Out) const
{
	Out.Reset();
	TSet<FGuid> Seen;
	for (const FHSREquipmentSaveDto& Dto : In)
	{
		if (!Dto.CharacterId.IsValid() || !Dto.InstanceId.IsValid() || Seen.Contains(Dto.InstanceId)
			|| Dto.AuthorityRevision < 0)
		{
			return false;
		}

		const EHSREquipmentKind Kind = static_cast<EHSREquipmentKind>(Dto.Kind);
		if (!ValidateRestoreInstance(Dto.DefinitionId, Kind, Dto.Slot, Dto.EnhancementLevel, Dto.SetId, Dto.Modifiers))
		{
			return false;
		}
		Seen.Add(Dto.InstanceId);

		FHSREquipmentInstance Instance;
		Instance.InstanceId = Dto.InstanceId;
		Instance.DefinitionId = Dto.DefinitionId;
		Instance.Kind = Kind;
		Instance.EnhancementLevel = Dto.EnhancementLevel;
		Instance.Modifiers = Dto.Modifiers;

		const bool bExistingCharacter = Out.Contains(Dto.CharacterId);
		FHSREquipmentRestoreState& State = Out.FindOrAdd(Dto.CharacterId);
		if (bExistingCharacter && State.Revision != Dto.AuthorityRevision)
		{
			return false;
		}
		State.Revision = Dto.AuthorityRevision;

		if (!InsertIntoRestoreState(State, Instance, Dto.Slot, Dto.SetId))
		{
			return false;
		}
	}
	return true;
}

// PrepareRestore（registry 版）：先把注册表里的每个实例解析成 FHSREquipmentInstance，
// 再用摆放表把它们放进对应角色的槽位。注册表实例可以不被任何槽位引用（背包里的
// 未装备实例）；摆放表要求实例必须存在于注册表、角色 GUID 合法、槽位不重复。
bool UHSREquipmentSubsystem::PrepareRestore(const TArray<FHSREquipmentRegistryDto>& Registry, const TArray<FHSREquipmentPlacementDto>& Placements, FHSREquipmentRegistryRestoreState& Out) const
{
	Out = FHSREquipmentRegistryRestoreState();

	// 注册表实例本身不带摆放，所以槽位匹配要到摆放阶段才检查。
	for (const FHSREquipmentRegistryDto& Dto : Registry)
	{
		if (!Dto.InstanceId.IsValid() || Out.Registry.Contains(Dto.InstanceId))
		{
			return false;
		}

		FHSREquipmentInstance Instance;
		Instance.InstanceId = Dto.InstanceId;
		Instance.DefinitionId = Dto.DefinitionId;
		Instance.Kind = static_cast<EHSREquipmentKind>(Dto.Kind);
		Instance.EnhancementLevel = Dto.EnhancementLevel;
		Instance.Modifiers = Dto.Modifiers;

		const FDefinitionRule* Rule = Definitions.Find(Instance.DefinitionId);
		if (!Rule
			|| !ValidateRestoreInstance(Instance.DefinitionId, Instance.Kind, Rule->Slot,
				Dto.EnhancementLevel, Dto.SetId, Instance.Modifiers))
		{
			return false;
		}
		Out.Registry.Add(Instance.InstanceId, MoveTemp(Instance));
	}

	// 摆放表：实例必须已在注册表，且每个实例只能被摆放一次。
	TSet<FGuid> Seen;
	for (const FHSREquipmentPlacementDto& Dto : Placements)
	{
		const FHSREquipmentInstance* Instance = Out.Registry.Find(Dto.InstanceId);
		if (!Instance || !Dto.CharacterId.IsValid() || Seen.Contains(Dto.InstanceId)
			|| Dto.AuthorityRevision < 0 || Dto.Kind != static_cast<int32>(Instance->Kind))
		{
			return false;
		}

		const FDefinitionRule* Rule = FindDefinition(*Instance);
		if (!Rule || !ValidateRestoreInstance(Instance->DefinitionId, Instance->Kind, Dto.Slot,
				Instance->EnhancementLevel, Rule->SetId, Instance->Modifiers))
		{
			return false;
		}
		Seen.Add(Dto.InstanceId);

		const bool bExistingCharacter = Out.Loadouts.Contains(Dto.CharacterId);
		FHSREquipmentRestoreState& State = Out.Loadouts.FindOrAdd(Dto.CharacterId);
		if (bExistingCharacter && State.Revision != Dto.AuthorityRevision)
		{
			return false;
		}
		State.Revision = Dto.AuthorityRevision;

		if (!InsertIntoRestoreState(State, *Instance, Dto.Slot, Rule->SetId))
		{
			return false;
		}
	}
	return true;
}

// CommitRestore（扁平数组版）：把恢复候选整体装入运行时。
// 重置 Loadouts/InstanceOwners/InstanceRegistry，再按候选重建；同时清空事务账本
// （移动与强化），因为恢复后这些历史事务已不再有效。
void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRestoreMap& Candidate)
{
	Loadouts.Reset();
	InstanceOwners.Reset();
	InstanceRegistry.Reset();
	for (const auto& P : Candidate)
	{
		FLoadoutState& S = Loadouts.Add(P.Key);
		S.Revision = P.Value.Revision;
		for (const auto& E : P.Value.Loadout.Equipment)
		{
			InstanceRegistry.Add(E.Value.InstanceId, E.Value);
			S.Equipment.Add(E.Key, E.Value.InstanceId);
			InstanceOwners.Add(E.Value.InstanceId, P.Key);
		}
		for (const auto& R : P.Value.Loadout.Relics)
		{
			InstanceRegistry.Add(R.Value.InstanceId, R.Value);
			S.Relics.Add(R.Key, R.Value.InstanceId);
			InstanceOwners.Add(R.Value.InstanceId, P.Key);
		}
	}
	MovementLedger.Reset();
	MovementLedgerOrder.Reset();
	EnhancementLedger.Reset();
	EnhancementLedgerOrder.Reset();
}

// NotifyRestored：恢复完成后，对每个变更过的角色广播 LoadoutChanged。
void UHSREquipmentSubsystem::NotifyRestored(const TSet<FGuid>& Changed)
{
	for (const FGuid& Id : Changed)
	{
		int32 Rev = Loadouts.FindRef(Id).Revision;
		LoadoutChanged.Broadcast(Id, Rev);
	}
}

// RegisterDefinition（武器版）：注册一个武器/装备定义。
EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterDefinition(const UHSREquipmentDefinition& Definition)
{
	if (Definition.DefinitionId.IsNone())
	{
		return EHSREquipmentOperationResult::InvalidDefinitionId;
	}
	if (Definition.EnhancementCap < 0)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (Definitions.Contains(Definition.DefinitionId))
	{
		return EHSREquipmentOperationResult::DuplicateDefinitionId;
	}

	FDefinitionRule Rule;
	Rule.Kind = EHSREquipmentKind::Equipment;
	Rule.Slot = static_cast<int32>(Definition.Slot);
	Rule.EnhancementCap = Definition.EnhancementCap;
	Definitions.Add(Definition.DefinitionId, Rule);
	return EHSREquipmentOperationResult::Success;
}

// CommitRestore（registry 版）：把 registry 恢复候选整体装入运行时（逻辑同扁平版，
// 只是数据源是 Registry/Placement 结构）。
void UHSREquipmentSubsystem::CommitRestore(const FHSREquipmentRegistryRestoreState& Candidate)
{
	InstanceRegistry = Candidate.Registry;
	Loadouts.Reset();
	InstanceOwners.Reset();
	for (const auto& P : Candidate.Loadouts)
	{
		FLoadoutState& S = Loadouts.Add(P.Key);
		S.Revision = P.Value.Revision;
		for (const auto& E : P.Value.Loadout.Equipment)
		{
			S.Equipment.Add(E.Key, E.Value.InstanceId);
			InstanceOwners.Add(E.Value.InstanceId, P.Key);
		}
		for (const auto& R : P.Value.Loadout.Relics)
		{
			S.Relics.Add(R.Key, R.Value.InstanceId);
			InstanceOwners.Add(R.Value.InstanceId, P.Key);
		}
	}
	MovementLedger.Reset();
	MovementLedgerOrder.Reset();
	EnhancementLedger.Reset();
	EnhancementLedgerOrder.Reset();
}

// RegisterDefinition（遗器版）：注册一个遗器定义，额外记录套装 ID。
EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterDefinition(const UHSRRelicDefinition& Definition)
{
	if (Definition.DefinitionId.IsNone())
	{
		return EHSREquipmentOperationResult::InvalidDefinitionId;
	}
	if (Definition.EnhancementCap < 0)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (Definitions.Contains(Definition.DefinitionId))
	{
		return EHSREquipmentOperationResult::DuplicateDefinitionId;
	}

	FDefinitionRule Rule;
	Rule.Kind = EHSREquipmentKind::Relic;
	Rule.Slot = static_cast<int32>(Definition.Slot);
	Rule.EnhancementCap = Definition.EnhancementCap;
	Rule.SetId = Definition.SetId;
	Definitions.Add(Definition.DefinitionId, Rule);
	return EHSREquipmentOperationResult::Success;
}

// RegisterSetDefinition：注册一个套装定义（记录其激活门槛）。
EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterSetDefinition(const UHSRRelicSetDefinition& Definition)
{
	if (Definition.SetId.IsNone())
	{
		return EHSREquipmentOperationResult::InvalidDefinitionId;
	}
	if (Definition.Threshold <= 0)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (SetThresholds.Contains(Definition.SetId))
	{
		return EHSREquipmentOperationResult::DuplicateDefinitionId;
	}

	SetThresholds.Add(Definition.SetId, Definition.Threshold);
	return EHSREquipmentOperationResult::Success;
}

// GetSetThreshold：读取套装的激活门槛。未注册的套装沿用历史的两件套行为（默认门槛），
// 而不是变成「永远无法激活」。
int32 UHSREquipmentSubsystem::GetSetThreshold(const FName SetId) const
{
	const int32* Authored = SetThresholds.Find(SetId);
	return Authored ? *Authored : FHSRRelicSetResolver::DefaultThreshold;
}

// IsDefinitionCompatible：判断定义是否匹配给定的种类与槽位（用于存档校验里的
// inventory-equipment-mapping 检查）。
bool UHSREquipmentSubsystem::IsDefinitionCompatible(const FName DefinitionId, const EHSREquipmentKind Kind, const int32 Slot) const
{
	const FDefinitionRule* Rule = Definitions.Find(DefinitionId);
	return Rule && Rule->Kind == Kind && Rule->Slot == Slot;
}

// RegisterInstance：把一件实例加入注册表（不绑定角色）。校验实例 ID、词条、定义存在、
// 种类/槽位匹配、强化等级在界内；若实例已存在则要求载荷完全一致（否则 InstancePayloadConflict）。
EHSREquipmentOperationResult UHSREquipmentSubsystem::RegisterInstance(const FHSREquipmentInstance& Instance)
{
	if (!Instance.InstanceId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidInstanceId;
	}
	if (!IsValidInstance(Instance))
	{
		return EHSREquipmentOperationResult::InvalidModifier;
	}
	const FDefinitionRule* Rule = FindDefinition(Instance);
	if (!Rule)
	{
		return EHSREquipmentOperationResult::UnknownDefinition;
	}
	if (Rule->Kind != Instance.Kind || !IsSlotValid(Rule->Kind, Rule->Slot))
	{
		return EHSREquipmentOperationResult::InvalidSlot;
	}
	if (Instance.EnhancementLevel < 0 || Instance.EnhancementLevel > Rule->EnhancementCap)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (const FHSREquipmentInstance* Existing = InstanceRegistry.Find(Instance.InstanceId))
	{
		return IsSamePayload(*Existing, Instance) ? EHSREquipmentOperationResult::NoOp : EHSREquipmentOperationResult::InstancePayloadConflict;
	}
	InstanceRegistry.Add(Instance.InstanceId, Instance);
	return EHSREquipmentOperationResult::Success;
}

// EnsureRegisteredFromItem：确保某个背包唯一物品（通过掉落/奖励进入背包）已经在装备
// 注册表里有对应实例。若还没有，就从映射目录按物品 ID 解析出装备定义并铸出一件实例。
// 铸件时会带上作者配置的基础词条，让掉落装备穿戴后真的提供属性加成。
EHSREquipmentOperationResult UHSREquipmentSubsystem::EnsureRegisteredFromItem(
	FName ItemId, const FGuid& InstanceId, const UHSRItemEquipmentMappingCatalog& MappingCatalog)
{
	if (!InstanceId.IsValid() || ItemId.IsNone())
	{
		return EHSREquipmentOperationResult::InvalidInstanceId;
	}
	FHSREquipmentInstance Existing;
	if (FindRegisteredInstance(InstanceId, Existing))
	{
		return EHSREquipmentOperationResult::NoOp;
	}

	FHSRItemEquipmentMappingEntry Mapping;
	if (!MappingCatalog.Resolve(ItemId, Mapping))
	{
		return EHSREquipmentOperationResult::UnknownDefinition;
	}

	FHSREquipmentInstance Instance;
	Instance.InstanceId = InstanceId;
	Instance.DefinitionId = Mapping.EquipmentDefinitionId;
	Instance.Kind = Mapping.Kind;

	// 带上作者配置的属性行，使掉落的遗器/武器穿戴后产生真实的派生属性提升。
	// 否则铸出的实例词条为空，加成恒为 0。定义资源按 DA_Relic_<槽位后缀> /
	// DA_Equipment_<后缀> 命名；后缀从 ID 末段派生（Relic.P12.Head -> Head）。
	const FString DefId = Mapping.EquipmentDefinitionId.ToString();
	FString Suffix = DefId;
	{
		int32 Dot = INDEX_NONE;
		if (DefId.FindLastChar(TEXT('.'), Dot))
		{
			Suffix = DefId.Mid(Dot + 1);
		}
	}
	if (Instance.Kind == EHSREquipmentKind::Relic)
	{
		if (UHSRRelicDefinition* Relic = LoadObject<UHSRRelicDefinition>(nullptr,
			*FString::Printf(TEXT("/Game/Data/Relics/DA_Relic_%s.DA_Relic_%s"), *Suffix, *Suffix)))
		{
			Instance.Modifiers = Relic->DefaultModifiers;
		}
	}
	else if (UHSREquipmentDefinition* Weapon = LoadObject<UHSREquipmentDefinition>(nullptr,
		*FString::Printf(TEXT("/Game/Data/Equipment/DA_Equipment_%s.DA_Equipment_%s"), *Suffix, *Suffix)))
	{
		Instance.Modifiers = Weapon->DefaultModifiers;
	}
	return RegisterInstance(Instance);
}

// FindRegisteredInstance：按实例 ID 查注册表（返回拷贝）。
bool UHSREquipmentSubsystem::FindRegisteredInstance(const FGuid& InstanceId, FHSREquipmentInstance& OutInstance) const
{
	const FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (!Instance)
	{
		return false;
	}
	OutInstance = *Instance;
	return true;
}

// Equip：把一件实例穿到角色身上（先注册，再按 ID 穿戴）。若注册成功但穿戴失败，
// 回滚注册（移除刚加的实例），保持状态干净。
EHSREquipmentOperationResult UHSREquipmentSubsystem::Equip(const FGuid& CharacterId, const FHSREquipmentInstance& Instance)
{
	const EHSREquipmentOperationResult Registration = RegisterInstance(Instance);
	if (Registration != EHSREquipmentOperationResult::Success && Registration != EHSREquipmentOperationResult::NoOp)
	{
		return Registration;
	}
	const EHSREquipmentOperationResult Result = EquipById(CharacterId, Instance.InstanceId);
	if (Registration == EHSREquipmentOperationResult::Success && Result != EHSREquipmentOperationResult::Success)
	{
		InstanceRegistry.Remove(Instance.InstanceId);
	}
	return Result;
}

// EquipById：按实例 ID 穿戴。校验角色/实例合法、定义匹配、实例未装备、目标槽位空闲，
// 通过后构造候选负载并 CommitLoadout。
EHSREquipmentOperationResult UHSREquipmentSubsystem::EquipById(const FGuid& CharacterId, const FGuid& InstanceId)
{
	if (!CharacterId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidCharacterId;
	}
	if (!InstanceId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidInstanceId;
	}
	const FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (!Instance)
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (!Rule || Rule->Kind != Instance->Kind || !IsSlotValid(Rule->Kind, Rule->Slot))
	{
		return EHSREquipmentOperationResult::InvalidSlot;
	}
	if (InstanceOwners.Contains(InstanceId))
	{
		return EHSREquipmentOperationResult::InstanceAlreadyEquipped;
	}
	FLoadoutState Candidate = Loadouts.FindRef(CharacterId);
	if (IsSlotOccupied(Candidate, Rule->Kind, Rule->Slot))
	{
		return EHSREquipmentOperationResult::SlotOccupied;
	}
	if (Rule->Kind == EHSREquipmentKind::Equipment)
	{
		Candidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Rule->Slot), InstanceId);
	}
	else
	{
		Candidate.Relics.Add(static_cast<EHSRRelicSlot>(Rule->Slot), InstanceId);
	}
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

// Replace：把一件实例替换到角色槽位（先注册，再按 ID 替换，失败回滚注册）。
EHSREquipmentOperationResult UHSREquipmentSubsystem::Replace(const FGuid& CharacterId, const FHSREquipmentInstance& Instance)
{
	const EHSREquipmentOperationResult Registration = RegisterInstance(Instance);
	if (Registration != EHSREquipmentOperationResult::Success && Registration != EHSREquipmentOperationResult::NoOp)
	{
		return Registration;
	}
	const EHSREquipmentOperationResult Result = ReplaceById(CharacterId, Instance.InstanceId);
	if (Registration == EHSREquipmentOperationResult::Success && Result != EHSREquipmentOperationResult::Success)
	{
		InstanceRegistry.Remove(Instance.InstanceId);
	}
	return Result;
}

// ReplaceById：按实例 ID 替换槽位（新实例顶掉旧实例）。要求角色已有该槽位的装备。
EHSREquipmentOperationResult UHSREquipmentSubsystem::ReplaceById(const FGuid& CharacterId, const FGuid& InstanceId)
{
	if (!CharacterId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidCharacterId;
	}
	if (!InstanceId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidInstanceId;
	}
	const FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (!Instance)
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (!Rule || Rule->Kind != Instance->Kind || !IsSlotValid(Rule->Kind, Rule->Slot))
	{
		return EHSREquipmentOperationResult::InvalidSlot;
	}
	if (InstanceOwners.Contains(InstanceId))
	{
		return EHSREquipmentOperationResult::InstanceAlreadyEquipped;
	}
	const FLoadoutState* Existing = Loadouts.Find(CharacterId);
	if (!Existing || !IsSlotOccupied(*Existing, Rule->Kind, Rule->Slot))
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	FLoadoutState Candidate = *Existing;
	if (Rule->Kind == EHSREquipmentKind::Equipment)
	{
		Candidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Rule->Slot), InstanceId);
	}
	else
	{
		Candidate.Relics.Add(static_cast<EHSRRelicSlot>(Rule->Slot), InstanceId);
	}
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

// ExecuteMovement：装备移动事务（穿戴/卸下/替换）的唯一入口。这个函数是「预演 ->
// 校验 -> 安装」三段式事务的完整实现，并带有幂等重放：同一 OperationId 重复调用时
// 若请求完全一致，直接返回上一次的结果（bReplay=true, bCommitted=false）。
FHSREquipmentMovementResult UHSREquipmentSubsystem::ExecuteMovement(const FHSREquipmentMovementRequest& Request,
	UHSRInventorySubsystem& Inventory, const UHSRItemEquipmentMappingCatalog& MappingCatalog)
{
	FHSREquipmentMovementResult Result;
	Result.OperationId = Request.OperationId;

	// 幂等重放：账本里已有该 OperationId。
	if (const FMovementLedgerEntry* ExistingOperation = MovementLedger.Find(Request.OperationId))
	{
		if (!IsSameMovementRequest(ExistingOperation->Request, Request))
		{
			Result.Code = EHSREquipmentMovementResultCode::OperationIdConflict;
			return Result;
		}
		Result = ExistingOperation->Result;
		Result.bCommitted = false;
		Result.bReplay = true;
		return Result;
	}

	// 预读当前背包与装备版本号，作为结果里的 OldRevision。
	FHSRInventorySnapshot InventorySnapshot;
	Inventory.GetSnapshot(InventorySnapshot);
	Result.OldInventoryRevision = InventorySnapshot.Revision;
	Result.NewInventoryRevision = InventorySnapshot.Revision;
	const FLoadoutState* ExistingLoadout = Loadouts.Find(Request.CharacterId);
	Result.OldEquipmentRevision = ExistingLoadout ? ExistingLoadout->Revision : 0;
	Result.NewEquipmentRevision = Result.OldEquipmentRevision;

	// 请求基础校验。
	if (!Request.OperationId.IsValid() || !Request.CharacterId.IsValid() || !Request.InstanceId.IsValid())
	{
		return Result;
	}
	if (Request.Intent != EHSREquipmentMovementIntent::Equip
		&& Request.Intent != EHSREquipmentMovementIntent::Unequip
		&& Request.Intent != EHSREquipmentMovementIntent::Replace)
	{
		return Result;
	}
	// 期望版本号冲突（并发保护）：调用方基于旧版本发起，已被他人改动则拒绝。
	if (Request.ExpectedInventoryRevision != InventorySnapshot.Revision)
	{
		Result.Code = EHSREquipmentMovementResultCode::InventoryRevisionConflict;
		return Result;
	}
	if (Request.ExpectedEquipmentRevision != Result.OldEquipmentRevision)
	{
		Result.Code = EHSREquipmentMovementResultCode::EquipmentRevisionConflict;
		return Result;
	}

	// 确保实例已注册。掉落/奖励进入背包的装备在背包里是唯一物品但还没有装备实例，
	// 这里按映射目录铸一个；Unequip 未知实例则保持拒绝。
	FHSREquipmentInstance RegistryInstance;
	if (!FindRegisteredInstance(Request.InstanceId, RegistryInstance))
	{
		if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		const FHSRItemInstance* InventoryMembership = InventorySnapshot.UniqueItems.FindByPredicate(
			[&Request](const FHSRItemInstance& Item) { return Item.InstanceId == Request.InstanceId; });
		if (!InventoryMembership)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		const EHSREquipmentOperationResult AutoRegistration = EnsureRegisteredFromItem(
			InventoryMembership->DefinitionId, Request.InstanceId, MappingCatalog);
		if (AutoRegistration != EHSREquipmentOperationResult::Success
			&& AutoRegistration != EHSREquipmentOperationResult::NoOp)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (!FindRegisteredInstance(Request.InstanceId, RegistryInstance))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
	}

	// 映射校验：物品 <-> 装备定义的映射必须匹配请求的种类/槽位。
	FHSRItemEquipmentMappingEntry Mapping;
	FGuid DisplacedInstanceId;
	FHSREquipmentInstance DisplacedRegistryInstance;
	FHSRItemEquipmentMappingEntry DisplacedMapping;
	const FHSRItemInstance* InventoryMembership = InventorySnapshot.UniqueItems.FindByPredicate([&Request](const FHSRItemInstance& Item)
	{
		return Item.InstanceId == Request.InstanceId;
	});
	const bool bResolvedMapping = Request.Intent != EHSREquipmentMovementIntent::Unequip
		? InventoryMembership && MappingCatalog.Resolve(InventoryMembership->DefinitionId, Mapping)
		: MappingCatalog.ResolveEquipmentDefinition(RegistryInstance.DefinitionId, Mapping);
	const bool bMappingValid = bResolvedMapping && MappingCatalog.Validate(
		Mapping.ItemId, EHSRItemStorageKind::Unique,
		[this, &Request, &RegistryInstance](const FName DefinitionId, const EHSREquipmentKind Kind, const int32 Slot)
		{
			const FDefinitionRule* Rule = Definitions.Find(DefinitionId);
			return Rule && DefinitionId == RegistryInstance.DefinitionId && Kind == RegistryInstance.Kind
				&& Kind == Request.Kind && Slot == Request.Slot && Rule->Kind == Kind && Rule->Slot == Slot;
		}, Mapping);
	if (!bMappingValid)
	{
		Result.Code = EHSREquipmentMovementResultCode::MappingRejected;
		return Result;
	}

	// 针对意图的额外校验：
	//   - Replace：目标槽位必须已有旧装备（被替换对象），且旧装备的映射也要合法；
	//   - Unequip：实例必须确实属于该角色且位于请求的槽位。
	if (Request.Intent == EHSREquipmentMovementIntent::Replace)
	{
		if (!ExistingLoadout)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		const FGuid* Current = FindPlacedInstance(*ExistingLoadout, Request.Kind, Request.Slot);
		if (!Current || !InstanceOwners.Contains(*Current) || InstanceOwners.FindRef(*Current) != Request.CharacterId
			|| !FindRegisteredInstance(*Current, DisplacedRegistryInstance))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (!MappingCatalog.ResolveEquipmentDefinition(DisplacedRegistryInstance.DefinitionId, DisplacedMapping)
			|| !MappingCatalog.Validate(DisplacedMapping.ItemId, EHSRItemStorageKind::Unique,
				[this, &Request, &DisplacedRegistryInstance](const FName DefinitionId, const EHSREquipmentKind Kind, const int32 Slot)
				{
					const FDefinitionRule* Rule = Definitions.Find(DefinitionId);
					return Rule && DefinitionId == DisplacedRegistryInstance.DefinitionId
						&& Kind == DisplacedRegistryInstance.Kind && Kind == Request.Kind && Slot == Request.Slot
						&& Rule->Kind == Kind && Rule->Slot == Slot;
				}, DisplacedMapping))
		{
			Result.Code = EHSREquipmentMovementResultCode::MappingRejected;
			return Result;
		}
		DisplacedInstanceId = *Current;
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		const FGuid* Owner = InstanceOwners.Find(Request.InstanceId);
		const FGuid* Placed = ExistingLoadout ? FindPlacedInstance(*ExistingLoadout, Request.Kind, Request.Slot) : nullptr;
		if (!Owner || *Owner != Request.CharacterId || !Placed || *Placed != Request.InstanceId)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
	}

	// 背包预演：按意图让背包子系统产出候选（穿戴=移除唯一物品、卸下=加回唯一物品、
	// 替换=同时移除新物品并加回被换下的物品）。
	FHSRInventoryMovementCandidate InventoryCandidate;
	EHSRInventoryOperationResult InventoryResult = EHSRInventoryOperationResult::InvalidDefinition;
	if (Request.Intent == EHSREquipmentMovementIntent::Equip)
	{
		InventoryResult = Inventory.PrepareEquipmentRemovalCandidate(Request.InstanceId, Mapping.ItemId, Request.ExpectedInventoryRevision, InventoryCandidate);
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		InventoryResult = Inventory.PrepareEquipmentAdditionCandidate(Request.InstanceId, Mapping.ItemId, Request.ExpectedInventoryRevision, InventoryCandidate);
	}
	else
	{
		InventoryResult = Inventory.PrepareEquipmentSwapCandidate(Request.InstanceId, Mapping.ItemId,
			DisplacedInstanceId, DisplacedMapping.ItemId, Request.ExpectedInventoryRevision, InventoryCandidate);
	}
	if (InventoryResult != EHSRInventoryOperationResult::Success)
	{
		Result.Code = EHSREquipmentMovementResultCode::InventoryRejected;
		return Result;
	}

	// 装备负载候选：在副本上按意图增删槽位。
	FLoadoutState EquipmentCandidate = ExistingLoadout ? *ExistingLoadout : FLoadoutState();
	if (Request.Intent == EHSREquipmentMovementIntent::Equip)
	{
		if (InstanceOwners.Contains(Request.InstanceId) || IsSlotOccupied(EquipmentCandidate, Request.Kind, Request.Slot))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (Request.Kind == EHSREquipmentKind::Equipment)
		{
			EquipmentCandidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Request.Slot), Request.InstanceId);
		}
		else
		{
			EquipmentCandidate.Relics.Add(static_cast<EHSRRelicSlot>(Request.Slot), Request.InstanceId);
		}
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		const FGuid* Owner = InstanceOwners.Find(Request.InstanceId);
		const FGuid* Placed = FindPlacedInstance(EquipmentCandidate, Request.Kind, Request.Slot);
		if (!Owner || *Owner != Request.CharacterId || !Placed || *Placed != Request.InstanceId)
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (Request.Kind == EHSREquipmentKind::Equipment)
		{
			EquipmentCandidate.Equipment.Remove(static_cast<EHSREquipmentSlot>(Request.Slot));
		}
		else
		{
			EquipmentCandidate.Relics.Remove(static_cast<EHSRRelicSlot>(Request.Slot));
		}
	}
	else
	{
		if (InstanceOwners.Contains(Request.InstanceId))
		{
			Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
			return Result;
		}
		if (Request.Kind == EHSREquipmentKind::Equipment)
		{
			EquipmentCandidate.Equipment.Add(static_cast<EHSREquipmentSlot>(Request.Slot), Request.InstanceId);
		}
		else
		{
			EquipmentCandidate.Relics.Add(static_cast<EHSRRelicSlot>(Request.Slot), Request.InstanceId);
		}
	}
	EquipmentCandidate.Revision = Result.OldEquipmentRevision + 1;

	// 装备负载投影：把候选解析成完整 Loadout，供投影钩子（战斗侧刷新属性）使用。
	FHSREquipmentLoadout ProjectionLoadout;
	if (!ResolveLoadout(EquipmentCandidate, ProjectionLoadout))
	{
		Result.Code = EHSREquipmentMovementResultCode::EquipmentRejected;
		return Result;
	}

	// 投影钩子：Preflight 与 Commit 必须成对绑定；任一钩子拒绝则中止事务。
	const bool bHasProjectionPreflight = MovementProjectionPreflight.IsBound();
	const bool bHasProjectionCommit = MovementProjectionCommit.IsBound();
	if (bHasProjectionPreflight != bHasProjectionCommit
		|| (bHasProjectionPreflight && !MovementProjectionPreflight.Execute(Request, ProjectionLoadout)))
	{
		Result.Code = EHSREquipmentMovementResultCode::ProjectionRejected;
		return Result;
	}
	if (MovementProjectionApply.IsBound() && !MovementProjectionApply.Execute(Request, ProjectionLoadout))
	{
		Result.Code = EHSREquipmentMovementResultCode::ProjectionRejected;
		return Result;
	}

	// 安装阶段：先装背包候选，再装装备负载，再更新实例归属（Equip=加、Unequip=减、
	// Replace=换），最后推进版本号。
	const int64 NewInventoryRevision = InventoryCandidate.NextRevision;
	Inventory.InstallEquipmentMovementCandidateNoFail(MoveTemp(InventoryCandidate));
	FLoadoutState& InstalledLoadout = Loadouts.FindOrAdd(Request.CharacterId);
	InstalledLoadout = MoveTemp(EquipmentCandidate);
	if (Request.Intent == EHSREquipmentMovementIntent::Equip)
	{
		InstanceOwners.Add(Request.InstanceId, Request.CharacterId);
	}
	else if (Request.Intent == EHSREquipmentMovementIntent::Unequip)
	{
		InstanceOwners.Remove(Request.InstanceId);
	}
	else
	{
		InstanceOwners.Remove(DisplacedInstanceId);
		InstanceOwners.Add(Request.InstanceId, Request.CharacterId);
		Result.DisplacedInstanceId = DisplacedInstanceId;
	}
	Inventory.FinalizeEquipmentMovementRevisionNoFail(NewInventoryRevision);
	Result.NewInventoryRevision = NewInventoryRevision;
	Result.NewEquipmentRevision = InstalledLoadout.Revision;
	Result.Code = EHSREquipmentMovementResultCode::Success;
	Result.bCommitted = true;

	// 记录账本（幂等重放的数据源），发布背包变更与负载变更，并调用 Commit 投影钩子。
	RecordLedgerEntry(MovementLedger, MovementLedgerOrder, Request.OperationId,
		FMovementLedgerEntry{Request, Result});
	Inventory.PublishEquipmentMovementCommit(NewInventoryRevision);
	LoadoutChanged.Broadcast(Request.CharacterId, InstalledLoadout.Revision);
	if (bHasProjectionCommit)
	{
		MovementProjectionCommit.Execute(Request, ProjectionLoadout);
	}
	return Result;
}

// ExecuteEnhancement：遗器/装备强化事务。与 ExecuteMovement 相同的幂等账本模式：
// 同一 OperationId 重复执行返回重放结果。预演阶段校验请求、定义、规则与材料库存，
// 安装阶段扣材料、改实例等级与词条、推进版本号。
FHSREquipmentEnhancementResult UHSREquipmentSubsystem::ExecuteEnhancement(
	const FHSREquipmentEnhancementRequest& Request, UHSRInventorySubsystem& Inventory,
	const UHSREquipmentEnhancementCatalog& Catalog)
{
	FHSREquipmentEnhancementResult Result;
	Result.OperationId = Request.OperationId;

	// 幂等重放。
	if (const FEnhancementLedgerEntry* ExistingOperation = EnhancementLedger.Find(Request.OperationId))
	{
		if (!IsSameEnhancementRequest(ExistingOperation->Request, Request))
		{
			Result.Code = EHSREquipmentEnhancementResultCode::OperationIdConflict;
			return Result;
		}
		Result = ExistingOperation->Result;
		Result.bCommitted = false;
		Result.bReplay = true;
		return Result;
	}

	// 预读版本号。
	FHSRInventorySnapshot InventorySnapshot;
	Inventory.GetSnapshot(InventorySnapshot);
	Result.OldInventoryRevision = InventorySnapshot.Revision;
	Result.NewInventoryRevision = InventorySnapshot.Revision;
	const FLoadoutState* ExistingLoadout = Loadouts.Find(Request.CharacterId);
	Result.OldEquipmentRevision = ExistingLoadout ? ExistingLoadout->Revision : 0;
	Result.NewEquipmentRevision = Result.OldEquipmentRevision;

	// 请求基础校验与版本号冲突检查。
	if (!Request.OperationId.IsValid() || !Request.CharacterId.IsValid() || !Request.InstanceId.IsValid()
		|| Request.TargetLevel < 0 || Request.ExpectedEnhancementLevel < 0)
	{
		return Result;
	}
	if (Request.ExpectedInventoryRevision != InventorySnapshot.Revision)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::InventoryRevisionConflict;
		return Result;
	}
	if (Request.ExpectedEquipmentRevision != Result.OldEquipmentRevision)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::EquipmentRevisionConflict;
		return Result;
	}

	// 实例归属与当前等级校验。
	FHSREquipmentInstance* CurrentInstance = InstanceRegistry.Find(Request.InstanceId);
	if (CurrentInstance == nullptr)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::EquipmentRejected;
		return Result;
	}
	Result.OldEnhancementLevel = CurrentInstance->EnhancementLevel;
	Result.NewEnhancementLevel = CurrentInstance->EnhancementLevel;
	const FGuid* Owner = InstanceOwners.Find(Request.InstanceId);
	if (Owner == nullptr || *Owner != Request.CharacterId || CurrentInstance->Kind != Request.Kind)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::EquipmentRejected;
		return Result;
	}
	if (Request.ExpectedEnhancementLevel != CurrentInstance->EnhancementLevel)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::EnhancementLevelConflict;
		return Result;
	}
	if (Request.TargetLevel < CurrentInstance->EnhancementLevel)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::CatalogRejected;
		return Result;
	}

	// 规则解析与定义校验：规则必须存在、目标等级不超强化上限、材料成本为正、
	// 目标词条合法。
	FHSREquipmentEnhancementRule Rule;
	if (!Catalog.ResolveRule(CurrentInstance->DefinitionId, CurrentInstance->Kind, Request.TargetLevel, Rule))
	{
		Result.Code = EHSREquipmentEnhancementResultCode::CatalogRejected;
		return Result;
	}
	const FDefinitionRule* DefinitionRule = Definitions.Find(CurrentInstance->DefinitionId);
	if (DefinitionRule == nullptr || DefinitionRule->Kind != Rule.Kind
		|| Rule.TargetLevel > DefinitionRule->EnhancementCap || Rule.MaterialCost <= 0
		|| !IsValidModifiers(Rule.TargetModifiers))
	{
		Result.Code = EHSREquipmentEnhancementResultCode::CatalogRejected;
		return Result;
	}
	// 目标等级与当前相同 => NoOp（记入账本保证幂等）。
	if (Request.TargetLevel == CurrentInstance->EnhancementLevel)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::NoOp;
		RecordLedgerEntry(EnhancementLedger, EnhancementLedgerOrder, Request.OperationId,
			FEnhancementLedgerEntry{Request, Result});
		return Result;
	}

	// 材料预演：从背包扣材料成本。
	FHSRInventoryEnhancementCandidate InventoryCandidate;
	if (Inventory.PrepareEquipmentEnhancementCandidate(Rule.MaterialItemId, Rule.MaterialCost,
		Request.ExpectedInventoryRevision, InventoryCandidate) != EHSRInventoryOperationResult::Success)
	{
		Result.Code = EHSREquipmentEnhancementResultCode::InventoryRejected;
		return Result;
	}

	// 实例候选：把等级与词条更新为目标值。
	FHSREquipmentInstance CandidateInstance = *CurrentInstance;
	CandidateInstance.EnhancementLevel = Rule.TargetLevel;
	CandidateInstance.Modifiers = Rule.TargetModifiers;
	if (EnhancementProjectionPreflight.IsBound()
		&& !EnhancementProjectionPreflight.Execute(Request, CandidateInstance))
	{
		Result.Code = EHSREquipmentEnhancementResultCode::ProjectionRejected;
		return Result;
	}

	// 安装阶段：扣材料 -> 更新实例 -> 推进装备版本号 -> 定版背包版本号。
	Inventory.InstallEquipmentEnhancementCandidateNoFail(MoveTemp(InventoryCandidate));
	CurrentInstance = InstanceRegistry.Find(Request.InstanceId);
	check(CurrentInstance != nullptr);
	*CurrentInstance = CandidateInstance;
	FLoadoutState& InstalledLoadout = Loadouts.FindOrAdd(Request.CharacterId);
	InstalledLoadout.Revision = Result.OldEquipmentRevision + 1;
	Inventory.FinalizeEquipmentEnhancementRevisionNoFail(InventorySnapshot.Revision + 1);
	Result.NewInventoryRevision = InventorySnapshot.Revision + 1;
	Result.NewEquipmentRevision = InstalledLoadout.Revision;
	Result.NewEnhancementLevel = CandidateInstance.EnhancementLevel;
	Result.Code = EHSREquipmentEnhancementResultCode::Success;
	Result.bCommitted = true;

	// 记录账本、发布变更、调用 Commit 投影钩子。
	RecordLedgerEntry(EnhancementLedger, EnhancementLedgerOrder, Request.OperationId,
		FEnhancementLedgerEntry{Request, Result});
	Inventory.PublishEquipmentEnhancementCommit(Result.NewInventoryRevision);
	LoadoutChanged.Broadcast(Request.CharacterId, InstalledLoadout.Revision);
	if (EnhancementProjectionCommit.IsBound())
	{
		EnhancementProjectionCommit.Execute(Request, CandidateInstance);
	}
	return Result;
}

// Unequip：按「角色 + 种类 + 槽位 + 期望实例」卸下装备。要求槽位上恰好是该实例。
EHSREquipmentOperationResult UHSREquipmentSubsystem::Unequip(const FGuid& CharacterId, EHSREquipmentKind Kind, int32 Slot, const FGuid& ExpectedInstanceId)
{
	if (!CharacterId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidCharacterId;
	}
	if (!ExpectedInstanceId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidInstanceId;
	}
	if (!IsSlotValid(Kind, Slot))
	{
		return EHSREquipmentOperationResult::InvalidSlot;
	}
	const FLoadoutState* Existing = Loadouts.Find(CharacterId);
	if (Existing == nullptr)
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	const FGuid* Current = FindPlacedInstance(*Existing, Kind, Slot);
	if (Current == nullptr)
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	if (*Current != ExpectedInstanceId)
	{
		return EHSREquipmentOperationResult::InstanceMismatch;
	}

	FLoadoutState Candidate = *Existing;
	if (Kind == EHSREquipmentKind::Equipment)
	{
		Candidate.Equipment.Remove(static_cast<EHSREquipmentSlot>(Slot));
	}
	else
	{
		Candidate.Relics.Remove(static_cast<EHSRRelicSlot>(Slot));
	}
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

// SetEnhancementLevel：直接设定某件已装备实例的强化等级（开发者/存档恢复用）。
EHSREquipmentOperationResult UHSREquipmentSubsystem::SetEnhancementLevel(const FGuid& CharacterId, const FGuid& InstanceId, int32 NewLevel)
{
	if (!CharacterId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidCharacterId;
	}
	if (!InstanceId.IsValid())
	{
		return EHSREquipmentOperationResult::InvalidInstanceId;
	}
	const FGuid* Owner = InstanceOwners.Find(InstanceId);
	if (Owner == nullptr)
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	if (*Owner != CharacterId)
	{
		return EHSREquipmentOperationResult::InstanceMismatch;
	}
	FLoadoutState* Existing = Loadouts.Find(CharacterId);
	check(Existing != nullptr);
	FHSREquipmentInstance* Instance = InstanceRegistry.Find(InstanceId);
	if (Instance == nullptr)
	{
		return EHSREquipmentOperationResult::TargetNotFound;
	}
	const FDefinitionRule* Rule = FindDefinition(*Instance);
	if (Rule == nullptr || NewLevel < 0 || NewLevel > Rule->EnhancementCap)
	{
		return EHSREquipmentOperationResult::InvalidEnhancementLevel;
	}
	if (Instance->EnhancementLevel == NewLevel)
	{
		return EHSREquipmentOperationResult::NoOp;
	}
	Instance->EnhancementLevel = NewLevel;
	FLoadoutState Candidate = *Existing;
	CommitLoadout(CharacterId, MoveTemp(Candidate));
	return EHSREquipmentOperationResult::Success;
}

// GetLoadout：导出某角色的完整装备负载（实例详情 + 版本号）。
bool UHSREquipmentSubsystem::GetLoadout(const FGuid& CharacterId, FHSREquipmentLoadout& OutLoadout, int32& OutRevision) const
{
	const FLoadoutState* State = Loadouts.Find(CharacterId);
	if (State == nullptr)
	{
		return false;
	}
	if (!ResolveLoadout(*State, OutLoadout))
	{
		return false;
	}
	OutRevision = State->Revision;
	return true;
}

// FindInstanceOwner：查询某个实例当前的归属角色。
bool UHSREquipmentSubsystem::FindInstanceOwner(const FGuid& InstanceId, FGuid& OutCharacterId) const
{
	const FGuid* Owner = InstanceOwners.Find(InstanceId);
	if (Owner == nullptr)
	{
		return false;
	}
	OutCharacterId = *Owner;
	return true;
}

// GetRelicSetSnapshots：汇总某角色的遗器套装快照（每套的件数、门槛、是否激活）。
void UHSREquipmentSubsystem::GetRelicSetSnapshots(const FGuid& CharacterId, TArray<FHSRRelicSetSnapshot>& Out) const
{
	Out.Reset();
	const FLoadoutState* State = Loadouts.Find(CharacterId);
	if (!State)
	{
		return;
	}

	// 先按套装累计件数。
	TMap<FName, int32> Counts;
	for (const auto& Pair : State->Relics)
	{
		if (const FHSREquipmentInstance* Instance = InstanceRegistry.Find(Pair.Value))
		{
			if (const FDefinitionRule* Rule = Definitions.Find(Instance->DefinitionId))
			{
				if (!Rule->SetId.IsNone())
				{
					++Counts.FindOrAdd(Rule->SetId);
				}
			}
		}
	}

	// 历史坑：旧代码在 Row.Threshold 赋值前就拿来比较，导致永远对着结构体默认的
	// 两件套门槛测试、完全忽略作者配置的值。现在统一走 GetSetThreshold。
	for (const auto& Pair : Counts)
	{
		FHSRRelicSetSnapshot Row;
		Row.SetId = Pair.Key;
		Row.EquippedCount = Pair.Value;
		Row.Threshold = GetSetThreshold(Pair.Key);
		Row.bActive = Row.EquippedCount >= Row.Threshold;
		Row.SetSourceId = Row.bActive ? Row.SetId : NAME_None;
		Out.Add(Row);
	}
	Out.Sort([](const FHSRRelicSetSnapshot& A, const FHSRRelicSetSnapshot& B) { return A.SetId.LexicalLess(B.SetId); });
}

// IsValidInstance：实例是否具备合法的定义 ID 与词条。
bool UHSREquipmentSubsystem::IsValidInstance(const FHSREquipmentInstance& Instance) const
{
	return !Instance.DefinitionId.IsNone() && IsValidModifiers(Instance.Modifiers);
}

// IsValidModifiers：校验词条数组。每个词条种类在合法区间内、数值有限且非负；
// 同类词条累加后仍须有限且不超 float 上限。
bool UHSREquipmentSubsystem::IsValidModifiers(const TArray<FHSREquipmentModifier>& Modifiers) const
{
	double Totals[4] = { 0.0, 0.0, 0.0, 0.0 };
	for (const FHSREquipmentModifier& Modifier : Modifiers)
	{
		const int32 StatIndex = static_cast<int32>(Modifier.Stat);
		if (StatIndex < 0 || StatIndex >= UE_ARRAY_COUNT(Totals)
			|| !FMath::IsFinite(Modifier.Value) || Modifier.Value < 0.0f)
		{
			return false;
		}
		Totals[StatIndex] += static_cast<double>(Modifier.Value);
		if (!std::isfinite(Totals[StatIndex]) || Totals[StatIndex] > static_cast<double>(TNumericLimits<float>::Max()))
		{
			return false;
		}
	}
	return true;
}

// FindDefinition：按实例定义 ID 查定义规则。
const UHSREquipmentSubsystem::FDefinitionRule* UHSREquipmentSubsystem::FindDefinition(const FHSREquipmentInstance& Instance) const
{
	return Definitions.Find(Instance.DefinitionId);
}

// IsSlotValid：槽位索引是否落在对应种类的合法区间内。
bool UHSREquipmentSubsystem::IsSlotValid(EHSREquipmentKind Kind, int32 Slot) const
{
	return Slot >= 0 && Slot < (Kind == EHSREquipmentKind::Equipment
		? static_cast<int32>(EHSREquipmentSlot::Feet) + 1
		: static_cast<int32>(EHSRRelicSlot::LinkRope) + 1);
}

// IsSlotOccupied：该槽位是否已被占用。
bool UHSREquipmentSubsystem::IsSlotOccupied(const FLoadoutState& Loadout, EHSREquipmentKind Kind, int32 Slot) const
{
	return FindPlacedInstance(Loadout, Kind, Slot) != nullptr;
}

// FindPlacedInstance：返回槽位上已放置的实例 ID（无则 nullptr）。
const FGuid* UHSREquipmentSubsystem::FindPlacedInstance(const FLoadoutState& Loadout, EHSREquipmentKind Kind, int32 Slot) const
{
	return Kind == EHSREquipmentKind::Equipment
		? Loadout.Equipment.Find(static_cast<EHSREquipmentSlot>(Slot))
		: Loadout.Relics.Find(static_cast<EHSRRelicSlot>(Slot));
}

// ResolveLoadout：把「槽位 -> 实例 ID」的负载状态解析成「槽位 -> 实例详情」。
// 任一实例缺失都视为状态损坏（返回 false）。
bool UHSREquipmentSubsystem::ResolveLoadout(const FLoadoutState& State, FHSREquipmentLoadout& OutLoadout) const
{
	OutLoadout = FHSREquipmentLoadout();
	for (const auto& Pair : State.Equipment)
	{
		const FHSREquipmentInstance* Instance = InstanceRegistry.Find(Pair.Value);
		if (!Instance)
		{
			return false;
		}
		OutLoadout.Equipment.Add(Pair.Key, *Instance);
	}
	for (const auto& Pair : State.Relics)
	{
		const FHSREquipmentInstance* Instance = InstanceRegistry.Find(Pair.Value);
		if (!Instance)
		{
			return false;
		}
		OutLoadout.Relics.Add(Pair.Key, *Instance);
	}
	return true;
}

// CommitLoadout：提交一份新的负载状态。先解除旧槽位上实例的归属，再写入新状态并
// 递增版本号，最后为新槽位上的实例登记归属并广播 LoadoutChanged。
void UHSREquipmentSubsystem::CommitLoadout(const FGuid& CharacterId, FLoadoutState Candidate)
{
	FLoadoutState& State = Loadouts.FindOrAdd(CharacterId);
	for (const auto& Pair : State.Equipment)
	{
		InstanceOwners.Remove(Pair.Value);
	}
	for (const auto& Pair : State.Relics)
	{
		InstanceOwners.Remove(Pair.Value);
	}
	Candidate.Revision = State.Revision + 1;
	State = MoveTemp(Candidate);
	for (const auto& Pair : State.Equipment)
	{
		InstanceOwners.Add(Pair.Value, CharacterId);
	}
	for (const auto& Pair : State.Relics)
	{
		InstanceOwners.Add(Pair.Value, CharacterId);
	}
	LoadoutChanged.Broadcast(CharacterId, State.Revision);
}

// IsSamePayload：判断两件实例的载荷是否完全一致（用于重复注册判定）。
bool UHSREquipmentSubsystem::IsSamePayload(const FHSREquipmentInstance& A, const FHSREquipmentInstance& B)
{
	if (A.InstanceId != B.InstanceId || A.DefinitionId != B.DefinitionId
		|| A.Kind != B.Kind || A.EnhancementLevel != B.EnhancementLevel
		|| A.Modifiers.Num() != B.Modifiers.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < A.Modifiers.Num(); ++Index)
	{
		if (A.Modifiers[Index].Stat != B.Modifiers[Index].Stat || A.Modifiers[Index].Value != B.Modifiers[Index].Value)
		{
			return false;
		}
	}
	return true;
}

// IsSameMovementRequest：判断两次移动请求是否完全一致（幂等重放的判定依据）。
bool UHSREquipmentSubsystem::IsSameMovementRequest(const FHSREquipmentMovementRequest& A,
	const FHSREquipmentMovementRequest& B)
{
	return A.OperationId == B.OperationId && A.CharacterId == B.CharacterId && A.InstanceId == B.InstanceId
		&& A.Intent == B.Intent && A.Kind == B.Kind && A.Slot == B.Slot
		&& A.ExpectedInventoryRevision == B.ExpectedInventoryRevision
		&& A.ExpectedEquipmentRevision == B.ExpectedEquipmentRevision;
}

// IsSameEnhancementRequest：判断两次强化请求是否完全一致（幂等重放的判定依据）。
bool UHSREquipmentSubsystem::IsSameEnhancementRequest(const FHSREquipmentEnhancementRequest& A,
	const FHSREquipmentEnhancementRequest& B)
{
	return A.OperationId == B.OperationId && A.CharacterId == B.CharacterId && A.InstanceId == B.InstanceId
		&& A.Kind == B.Kind && A.ExpectedInventoryRevision == B.ExpectedInventoryRevision
		&& A.ExpectedEquipmentRevision == B.ExpectedEquipmentRevision
		&& A.ExpectedEnhancementLevel == B.ExpectedEnhancementLevel && A.TargetLevel == B.TargetLevel;
}
