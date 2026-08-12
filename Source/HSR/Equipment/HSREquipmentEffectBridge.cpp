#include "HSREquipmentEffectBridge.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "../Progression/HSRProgressionGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

// CanApply：在不实际应用的前提下，检查「给这个 ASC 挂上该 Effect 类」是否可行。
// 通过 MakeOutgoingSpec 生成一个临时 Spec 来验证类可用性；只要 Spec 能成功构造，
// 就认为可以应用。Revision<0 视为无效聚合数据。
bool UHSREquipmentEffectBridge::CanApply(UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> C, const FHSREquipmentAggregate& A) const
{
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceCanApplyFailure)
	{
		return false;
	}
#endif
	if (!ASC || !C || A.Revision < 0)
	{
		return false;
	}
	return ASC->MakeOutgoingSpec(C, 1.f, ASC->MakeEffectContext()).IsValid();
}

// CanRemove：检查某个 Key 对应的已应用 Effect 是否仍然「活着」。
// 语义：如果 Sources 里没有该 Key、或 Handle 无效、或 ASC 失效、或该 Effect 已经
// 不在 ASC 上生效，则不需要（也不能）移除——返回 true 表示「可以移除/无需移除」。
bool UHSREquipmentEffectBridge::CanRemove(const FGuid& Key) const
{
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceCanRemoveFailure)
	{
		return false;
	}
#endif
	const FSource* Source = Sources.Find(Key);
	return !Source || !Source->Handle.IsValid() || !Source->ASC.IsValid()
		|| Source->ASC->GetActiveGameplayEffect(Source->Handle) != nullptr;
}

// Apply：为 Key 在指定 ASC 上应用（或热更新）一个携带装备属性加成的 GameplayEffect。
// 核心是「幂等热更新」：若同一 Key 已应用过同一个 Effect 类、且四项加成值都没变，
// 则只更新指纹里的 Revision 并直接返回成功（不必重挂 Effect，避免无谓的闪烁/抖动）。
// 若数值有变化，则先把旧的 Effect 移除、再挂新的。这样 GAS 侧始终只有一个
// 代表该 Key 的 Effect 生效，且其 SetByCaller 数值与最新的聚合结果一致。
bool UHSREquipmentEffectBridge::Apply(const FGuid& Key, UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> C, const FHSREquipmentAggregate& A)
{
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceApplyFailure)
	{
		bForceApplyFailure = false;
		return false;
	}
#endif
	if (!Key.IsValid() || !ASC || !C || A.Revision < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR.EquipmentBridge Apply InvalidInput Key=%s ASC=%s Class=%s Revision=%lld"),
			*Key.ToString(), ASC ? *ASC->GetName() : TEXT("None"), C ? *C->GetName() : TEXT("None"), A.Revision);
		return false;
	}

	// 快路径：同一个 Key、同一个 Effect 类、四项数值都没变——只把指纹 Revision 推到最新。
	if (FSource* O = Sources.Find(Key))
	{
		if (O->ASC.Get() == ASC && O->Class == C && O->Handle.IsValid()
			&& ASC->GetActiveGameplayEffect(O->Handle)
			&& O->Fingerprint.MaxHealth == A.MaxHealth && O->Fingerprint.Attack == A.Attack
			&& O->Fingerprint.Defense == A.Defense && O->Fingerprint.Speed == A.Speed)
		{
			O->Fingerprint.Revision = A.Revision;
			return true;
		}
	}

	// 生成 Spec，并把四项加成通过 SetByCaller 写进去。
	const FGameplayEffectSpecHandle S = ASC->MakeOutgoingSpec(C, 1.f, ASC->MakeEffectContext());
	if (!S.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR.EquipmentBridge Apply SpecInvalid Key=%s Class=%s"),
			*Key.ToString(), *C->GetName());
		return false;
	}
	S.Data->SetSetByCallerMagnitude(HSREquipmentTags::BonusMaxHealth, A.MaxHealth);
	S.Data->SetSetByCallerMagnitude(HSREquipmentTags::BonusAttack, A.Attack);
	S.Data->SetSetByCallerMagnitude(HSREquipmentTags::BonusDefense, A.Defense);
	S.Data->SetSetByCallerMagnitude(HSREquipmentTags::BonusSpeed, A.Speed);

	FActiveGameplayEffectHandle H = ASC->ApplyGameplayEffectSpecToSelf(*S.Data.Get());
	if (!H.WasSuccessfullyApplied())
	{
		UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentBridge Apply Failed Key=%s Class=%s Revision=%lld Values=%.3f/%.3f/%.3f/%.3f"),
			*Key.ToString(), *C->GetName(), A.Revision, A.MaxHealth, A.Attack, A.Defense, A.Speed);
		return false;
	}

	// 数值变化路径：把旧 Effect 摘掉，替换成刚应用的新 Effect。
	// 若旧 Effect 移除失败，则回滚新 Effect（同时移除），避免残留两个同名 Effect 叠加。
	if (FSource* O = Sources.Find(Key))
	{
		if (O->ASC.IsValid() && O->Handle.IsValid() && O->ASC->GetActiveGameplayEffect(O->Handle))
		{
			if (!O->ASC->RemoveActiveGameplayEffect(O->Handle))
			{
				ASC->RemoveActiveGameplayEffect(H);
				UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentBridge Apply OldRemoveFailed Key=%s OldHandle=%s NewHandle=%s"),
					*Key.ToString(), *O->Handle.ToString(), *H.ToString());
				return false;
			}
		}
	}

	// 记录新的 Source 快照，供后续 CanRemove / Remove / 热更新比对使用。
	FSource Src;
	Src.ASC = ASC;
	Src.Class = C;
	Src.Fingerprint = A;
	Src.Handle = H;
	Sources.Add(Key, Src);
	return true;
}

// Remove：按 Key 移除之前应用的效果，并清理 Sources 记录。
// 若 Effect 已不在 ASC 上（例如世界已销毁、ASC 失效），视作移除成功，只清理记录。
bool UHSREquipmentEffectBridge::Remove(const FGuid& Key)
{
#if WITH_EDITOR || WITH_DEV_AUTOMATION_TESTS
	if (bForceRemoveFailure)
	{
		bForceRemoveFailure = false;
		return false;
	}
#endif
	FSource* S = Sources.Find(Key);
	if (!S)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR.EquipmentBridge Remove MissingSource Key=%s"), *Key.ToString());
		return true;
	}
	bool Ok = !S->Handle.IsValid() || !S->ASC.IsValid()
		|| !S->ASC->GetActiveGameplayEffect(S->Handle)
		|| S->ASC->RemoveActiveGameplayEffect(S->Handle);
	if (Ok)
	{
		Sources.Remove(Key);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("HSR.EquipmentBridge Remove Failed Key=%s Handle=%s ASC=%s"),
			*Key.ToString(), S ? *S->Handle.ToString() : TEXT("None"),
			S && S->ASC.IsValid() ? *S->ASC->GetName() : TEXT("None"));
	}
	return Ok;
}

// RemoveAll：清空全部已应用的装备效果。逐个移除，任何一个失败都会把结果置为 false，
// 但会继续尝试移除其余项；全部移除成功后才清空 Sources 与 SetSourceKeys。
bool UHSREquipmentEffectBridge::RemoveAll()
{
	bool Ok = true;
	for (auto& P : Sources)
	{
		if (UAbilitySystemComponent* A = P.Value.ASC.Get())
		{
			if (P.Value.Handle.IsValid() && A->GetActiveGameplayEffect(P.Value.Handle))
			{
				Ok = A->RemoveActiveGameplayEffect(P.Value.Handle) && Ok;
			}
		}
	}
	if (Ok)
	{
		Sources.Empty();
		SetSourceKeys.Empty();
	}
	return Ok;
}

// ApplySetSource：把套装效果当成一个「命名的效果来源」来应用。
// 每个 SetSourceId 对应一个稳定的 FGuid Key（由 SetSourceId 的哈希派生，首次分配后
// 复用），后续对该套装的数值更新都走同一个 Key，从而复用 Apply 的热更新逻辑。
bool UHSREquipmentEffectBridge::ApplySetSource(const FName& SetSourceId, UAbilitySystemComponent* ASC, TSubclassOf<UGameplayEffect> EffectClass, const FHSREquipmentAggregate& Aggregate)
{
	if (SetSourceId.IsNone())
	{
		return false;
	}
	FGuid& Key = SetSourceKeys.FindOrAdd(SetSourceId);
	if (!Key.IsValid())
	{
		Key = FGuid(0, GetTypeHash(SetSourceId), 0, 1);
	}
	return Apply(Key, ASC, EffectClass, Aggregate);
}

// RemoveSetSource：按 SetSourceId 移除套装效果，并清理命名映射。
bool UHSREquipmentEffectBridge::RemoveSetSource(const FName& SetSourceId)
{
	const FGuid* Key = SetSourceKeys.Find(SetSourceId);
	if (!Key)
	{
		UE_LOG(LogTemp, Warning, TEXT("HSR.EquipmentBridge RemoveSetSource MissingSource Set=%s"), *SetSourceId.ToString());
		return true;
	}
	const bool bRemoved = Remove(*Key);
	if (bRemoved)
	{
		SetSourceKeys.Remove(SetSourceId);
	}
	return bRemoved;
}
