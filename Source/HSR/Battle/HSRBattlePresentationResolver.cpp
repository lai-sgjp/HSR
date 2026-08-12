#include "HSRBattlePresentationResolver.h"

namespace
{
	// 未配置映射时的兜底攻击/受击展示 ID（Demo 用占位）。
	static FName FallbackAttackId()
	{
		return TEXT("Demo.Presentation.Fallback.Attack");
	}

	static FName FallbackHitId()
	{
		return TEXT("Demo.Presentation.Fallback.Hit");
	}
}

// 登记一条展示映射：把“展示事件类型”映射到对应的攻击/受击表现资源。
void FHSRBattlePresentationResolver::AddMapping(const FHSRBattlePresentationMapping& Mapping)
{
	Mappings.Add(static_cast<uint8>(Mapping.EventType), Mapping);
}

// 把展示事件拷贝成展示意图（纯数据拷贝，不含表现资源）。
void FHSRBattlePresentationResolver::CopyEvent(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent)
{
	OutIntent.EventId = Event.EventId;
	OutIntent.ActionId = Event.ActionId;
	OutIntent.SourceParticipantId = Event.SourceParticipantId;
	OutIntent.TargetParticipantId = Event.TargetParticipantId;
	OutIntent.EventType = Event.EventType;
	OutIntent.Value = Event.Value;
	OutIntent.bCritical = Event.bCritical;
	OutIntent.bBreak = Event.bBreak;
}

// 解析展示事件 → 展示意图：按事件类型查映射；缺映射或映射不完整时回退到 Demo 占位 ID。
bool FHSRBattlePresentationResolver::Resolve(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent) const
{
	CopyEvent(Event, OutIntent);
	if (const FHSRBattlePresentationMapping* Mapping = Mappings.Find(static_cast<uint8>(Event.EventType)))
	{
		OutIntent.AttackPresentationId = Mapping->AttackPresentationId;
		OutIntent.HitPresentationId = Mapping->HitPresentationId;
		OutIntent.bFallback = OutIntent.AttackPresentationId.IsNone() || OutIntent.HitPresentationId.IsNone();
		if (OutIntent.bFallback)
		{
			OutIntent.AttackPresentationId = FallbackAttackId();
			OutIntent.HitPresentationId = FallbackHitId();
		}
		return true;
	}

	// 没有映射：整体回退到占位 ID。
	OutIntent.AttackPresentationId = FallbackAttackId();
	OutIntent.HitPresentationId = FallbackHitId();
	OutIntent.bFallback = true;
	return true;
}

// 消费式解析：同一事件 ID 只解析一次（后续重复请求返回 false）。
bool FHSRBattlePresentationResolver::Consume(const FHSRBattlePresentationEvent& Event, FHSRBattlePresentationIntent& OutIntent)
{
	if (!Event.EventId.IsValid() || ConsumedEventIds.Contains(Event.EventId))
	{
		return false;
	}
	if (!Resolve(Event, OutIntent))
	{
		return false;
	}
	ConsumedEventIds.Add(Event.EventId);
	return true;
}
