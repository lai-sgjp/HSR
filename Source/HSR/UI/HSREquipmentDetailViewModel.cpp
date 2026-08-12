#include "HSREquipmentDetailViewModel.h"
#include "../Equipment/HSREquipmentSubsystem.h"
#include "../Equipment/HSREquipmentStatAggregator.h"

// 装备详情 ViewModel：把指定角色的装备负载（含圣遗物套装）编译成
// 可直接展示的快照（物品行、来源行、属性合计、套装激活），并广播给 UI。
void UHSREquipmentDetailViewModel::BeginDestroy()
{
	Shutdown();
	Super::BeginDestroy();
}

// 初始化：绑定装备子系统负载变更事件并立即重建一次快照。
// 若参数非法（无子系统或无效角色 GUID），则产出 NotInitialized 失败快照。
void UHSREquipmentDetailViewModel::Initialize(UHSREquipmentSubsystem* In, const FGuid& Id)
{
	Shutdown();
	Equipment = In;
	CharacterId = Id;
	if (In && Id.IsValid())
	{
		Subscription = In->OnLoadoutChanged().AddUObject(this, &ThisClass::Rebuild);
		Rebuild(Id, 0);
		return;
	}
	Snapshot = FHSREquipmentDetailSnapshot();
	Snapshot.CharacterId = Id;
	Snapshot.FailureReason = EHSREquipmentDetailResult::NotInitialized;
	bHas = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}

// 关闭：解绑事件、清空缓存快照。
void UHSREquipmentDetailViewModel::Shutdown()
{
	if (Equipment.IsValid())
	{
		Equipment->OnLoadoutChanged().Remove(Subscription);
	}
	Equipment.Reset();
	bHas = false;
	Snapshot = FHSREquipmentDetailSnapshot();
}

// 重建快照：从装备子系统拉取该角色的负载，组装物品/来源/属性合计/套装行。
void UHSREquipmentDetailViewModel::Rebuild(const FGuid& Id, int32)
{
	if (Id != CharacterId || !Equipment.IsValid())
	{
		return;
	}
	FHSREquipmentLoadout L;
	int32 Revision = 0;
	Snapshot = FHSREquipmentDetailSnapshot();
	Snapshot.CharacterId = CharacterId;
	if (!Equipment->GetLoadout(CharacterId, L, Revision))
	{
		// 角色没有装备：给出“空负载”的合法快照。
		Snapshot.bIsValid = true;
		Snapshot.FailureReason = EHSREquipmentDetailResult::Empty;
	}
	else
	{
		Snapshot.Revision = Revision;
		// 装备与圣遗物合并为统一的物品列表，按实例 ID 排序保证稳定展示顺序。
		for (const auto& P : L.Equipment)
		{
			Snapshot.Items.Add(P.Value);
		}
		for (const auto& P : L.Relics)
		{
			Snapshot.Items.Add(P.Value);
		}
		Snapshot.Items.Sort([](const auto& A, const auto& B)
		{
			return A.InstanceId < B.InstanceId;
		});
		// 每件物品的每个修饰符展开成一行“来源”（含稳定的来源 ID）。
		for (const FHSREquipmentInstance& Item : Snapshot.Items)
		{
			for (int32 I = 0; I < Item.Modifiers.Num(); ++I)
			{
				const auto& M = Item.Modifiers[I];
				FHSREquipmentSourceRow R;
				R.SourceId = FName(*FString::Printf(TEXT("%s.%d"), *Item.InstanceId.ToString(EGuidFormats::Digits), I));
				R.DefinitionId = Item.DefinitionId;
				R.Stat = M.Stat;
				R.AuthoredValue = M.Value;
				R.EffectiveValue = M.Value;
				Snapshot.Sources.Add(R);
			}
		}
		// 合计值来自聚合器而不是本地求和：面板不能显示权威会拒绝的值
		// （非有限或负修饰符）。
		FHSREquipmentAggregate Totals;
		if (UHSREquipmentStatAggregator::Aggregate(L, Revision, Totals))
		{
			Snapshot.MaxHealth = Totals.MaxHealth;
			Snapshot.Attack = Totals.Attack;
			Snapshot.Defense = Totals.Defense;
			Snapshot.Speed = Totals.Speed;
		}
		else
		{
			Snapshot.FailureReason = EHSREquipmentDetailResult::InvalidSnapshot;
		}
		// 圣遗物套装：登记套装行；激活的套装额外提供一条来源。
		TArray<FHSRRelicSetSnapshot> Sets;
		Equipment->GetRelicSetSnapshots(CharacterId, Sets);
		for (const auto& Set : Sets)
		{
			FHSRRelicSetDetailRow Row;
			Row.SetId = Set.SetId;
			Row.SetSourceId = Set.SetSourceId;
			Row.EquippedCount = Set.EquippedCount;
			Row.Threshold = Set.Threshold;
			Row.bActive = Set.bActive;
			Snapshot.RelicSets.Add(Row);
			if (Set.bActive)
			{
				FHSREquipmentSourceRow Source;
				Source.SourceId = Set.SetSourceId;
				Source.DefinitionId = Set.SetId;
				Snapshot.Sources.Add(Source);
			}
		}
		Snapshot.Sources.Sort([](const auto& A, const auto& B)
		{
			return A.SourceId.LexicalLess(B.SourceId);
		});
		Snapshot.bIsValid = true;
		Snapshot.FailureReason = EHSREquipmentDetailResult::Success;
	}
	bHas = true;
	Changed.Broadcast(Snapshot);
	OnSnapshotChanged.Broadcast(Snapshot);
}
