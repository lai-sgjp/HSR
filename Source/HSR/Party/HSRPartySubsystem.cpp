#include "HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"

#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/World.h"

// 匿名命名空间：PIE 开发测试命令（HSR.PartyTest），用于快速验证队伍子系统的
// 增删换位语义与失败不变性。
namespace
{
	void RunHSRPartyTest()
	{
		if (!GEngine)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.PartyTest FAILED NoEngine"));
			return;
		}

		// 取 PIE 世界的队伍子系统。
		UHSRPartySubsystem* Party = nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World(); World && World->IsPlayInEditor())
			{
				if (UGameInstance* GI = World->GetGameInstance())
				{
					Party = GI->GetSubsystem<UHSRPartySubsystem>();
					break;
				}
			}
		}
		if (!Party)
		{
			UE_LOG(LogTemp, Error, TEXT("HSR.PartyTest FAILED NoPIEPartySubsystem"));
			return;
		}

		// 订阅变更事件并计数，用于验证失败操作不触发事件。
		int32 EventCount = 0;
		FDelegateHandle EventHandle = Party->OnPartyChanged().AddLambda([&EventCount](int64) { ++EventCount; });

		// 正常加入两个角色。
		FHSRPartySnapshot Before;
		Party->GetSnapshot(Before);
		const EHSRPartyResult AddA = Party->AddCharacter(TEXT("Character.A"));
		const EHSRPartyResult AddB = Party->AddCharacter(TEXT("Character.B"));
		FHSRPartySnapshot Filled;
		Party->GetSnapshot(Filled);
		const int64 FailureRevision = Filled.Revision;
		const int32 FailureEvents = EventCount;

		// 各种失败路径：重复、空名、未知角色、越界槽位——都应失败且不改状态。
		const EHSRPartyResult Duplicate = Party->AddCharacter(TEXT("Character.A"));
		const EHSRPartyResult None = Party->AddCharacter(NAME_None);
		const EHSRPartyResult Unknown = Party->AddCharacter(TEXT("Character.Unknown"));
		const EHSRPartyResult BadSlot = Party->AddCharacter(TEXT("Character.A"), 99);
		FHSRPartySnapshot AfterFailures;
		Party->GetSnapshot(AfterFailures);
		const bool bFailuresUnchanged = AfterFailures.Revision == FailureRevision && EventCount == FailureEvents
			&& AfterFailures.Slots.Num() == Filled.Slots.Num();

		// 换位、移除、替换。
		const EHSRPartyResult Swap = Party->SwapSlots(0, 1);
		const EHSRPartyResult Remove = Party->RemoveCharacter(0);
		const EHSRPartyResult Replace = Party->ReplaceCharacter(0, TEXT("Character.A"));
		FHSRPartySnapshot Final;
		Party->GetSnapshot(Final);

		UE_LOG(LogTemp, Log, TEXT("HSR.PartyTest RESULT AddA=%d AddB=%d Duplicate=%d None=%d Unknown=%d BadSlot=%d Swap=%d Remove=%d Replace=%d Revision=%lld Events=%d FailureStateUnchanged=%d Slots=%d"),
			static_cast<int32>(AddA), static_cast<int32>(AddB), static_cast<int32>(Duplicate),
			static_cast<int32>(None), static_cast<int32>(Unknown), static_cast<int32>(BadSlot),
			static_cast<int32>(Swap), static_cast<int32>(Remove), static_cast<int32>(Replace),
			Final.Revision, EventCount, bFailuresUnchanged ? 1 : 0, Final.Slots.Num());

		Party->OnPartyChanged().Remove(EventHandle);
	}

	FAutoConsoleCommand HSRPartyTestCommand(
		TEXT("HSR.PartyTest"),
		TEXT("Runs the PartySubsystem PIE development harness."),
		FConsoleCommandDelegate::CreateStatic(&RunHSRPartyTest));
}
#endif

// Initialize：初始化槽位数组（宽度=容量），并缓存角色档案子系统用于校验角色是否已知。
void UHSRPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Slots.SetNum(Capacity);
	Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
}

// IsKnownProfile：判断该角色 ID 是否已注册（存在档案快照）。
bool UHSRPartySubsystem::IsKnownProfile(FName CharacterId) const
{
	FHSRCharacterProfileSnapshot Snapshot;
	return !CharacterId.IsNone() && Profiles.IsValid() && Profiles->GetProfileSnapshot(CharacterId, Snapshot);
}

// IsDuplicate：候选槽位里是否已存在该角色（可跳过 IgnoreSlot，用于替换场景）。
bool UHSRPartySubsystem::IsDuplicate(const TArray<FHSRPartySlot>& Candidate, FName CharacterId, int32 IgnoreSlot) const
{
	for (int32 Index = 0; Index < Candidate.Num(); ++Index)
	{
		if (Index != IgnoreSlot && Candidate[Index].CharacterId == CharacterId)
		{
			return true;
		}
	}
	return false;
}

// Commit：真正提交一份候选槽位数组。要求宽度正好等于容量；提交前拼一段成员日志
// （用于调试），提交后递增 Revision 并广播 PartyChanged。
bool UHSRPartySubsystem::Commit(TArray<FHSRPartySlot>&& Candidate)
{
	if (Candidate.Num() != Capacity)
	{
		return false;
	}

	FString Members;
	for (int32 Index = 0; Index < Candidate.Num(); ++Index)
	{
		if (!Candidate[Index].IsEmpty())
		{
			if (!Members.IsEmpty())
			{
				Members += TEXT(",");
			}
			Members += FString::Printf(TEXT("%d:%s"), Index, *Candidate[Index].CharacterId.ToString());
		}
	}

	Slots = MoveTemp(Candidate);
	++Revision;
	UE_LOG(LogTemp, Log, TEXT("HSR.Party Commit Revision=%lld Members=%s"),
		Revision, Members.IsEmpty() ? TEXT("(empty)") : *Members);
	PartyChanged.Broadcast(Revision);
	return true;
}

// AddCharacter：加入角色。优先放 PreferredSlot（-1 表示自动找空位）；失败路径包括
// 未知角色、重复角色、槽位满、槽位非法、指定槽位被占。
EHSRPartyResult UHSRPartySubsystem::AddCharacter(FName CharacterId, int32 PreferredSlot)
{
	if (!IsKnownProfile(CharacterId))
	{
		return EHSRPartyResult::ProfileNotFound;
	}
	if (IsDuplicate(Slots, CharacterId))
	{
		return EHSRPartyResult::DuplicateCharacter;
	}

	int32 Slot = PreferredSlot;
	if (Slot == INDEX_NONE)
	{
		Slot = Slots.IndexOfByPredicate([](const FHSRPartySlot& Entry) { return Entry.IsEmpty(); });
	}
	if (!IsValidSlot(Slot))
	{
		return PreferredSlot == INDEX_NONE ? EHSRPartyResult::Full : EHSRPartyResult::InvalidSlot;
	}
	if (!Slots[Slot].IsEmpty())
	{
		return EHSRPartyResult::Full;
	}

	TArray<FHSRPartySlot> Candidate = Slots;
	Candidate[Slot].CharacterId = CharacterId;
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

// RemoveCharacter：清空指定槽位。
EHSRPartyResult UHSRPartySubsystem::RemoveCharacter(int32 Slot)
{
	if (!IsValidSlot(Slot))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (Slots[Slot].IsEmpty())
	{
		return EHSRPartyResult::EmptySlot;
	}

	TArray<FHSRPartySlot> Candidate = Slots;
	Candidate[Slot] = FHSRPartySlot();
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

// ReplaceCharacter：把指定槽位的角色替换成另一角色（跳过自身做重复检查）。
EHSRPartyResult UHSRPartySubsystem::ReplaceCharacter(int32 Slot, FName CharacterId)
{
	if (!IsValidSlot(Slot))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (!IsKnownProfile(CharacterId))
	{
		return EHSRPartyResult::ProfileNotFound;
	}
	if (IsDuplicate(Slots, CharacterId, Slot))
	{
		return EHSRPartyResult::DuplicateCharacter;
	}

	TArray<FHSRPartySlot> Candidate = Slots;
	Candidate[Slot].CharacterId = CharacterId;
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

// SwapSlots：交换两个槽位。任一为空或两者相同则拒绝。
EHSRPartyResult UHSRPartySubsystem::SwapSlots(int32 FirstSlot, int32 SecondSlot)
{
	if (!IsValidSlot(FirstSlot) || !IsValidSlot(SecondSlot))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (FirstSlot == SecondSlot || Slots[FirstSlot].IsEmpty() || Slots[SecondSlot].IsEmpty())
	{
		return EHSRPartyResult::EmptySlot;
	}

	TArray<FHSRPartySlot> Candidate = Slots;
	Candidate.Swap(FirstSlot, SecondSlot);
	return Commit(MoveTemp(Candidate)) ? EHSRPartyResult::Success : EHSRPartyResult::InvalidCandidate;
}

// CommitCandidate：按快照整份提交（供外部同步队伍用）。要求快照版本号与当前一致、
// 宽度正确、无重复/未知角色；提交后若 ActiveSlot 变化则额外递增版本并广播。
EHSRPartyResult UHSRPartySubsystem::CommitCandidate(const FHSRPartySnapshot& Candidate)
{
	if (Candidate.Revision != Revision)
	{
		return EHSRPartyResult::RevisionConflict;
	}
	if (Candidate.Slots.Num() != Capacity)
	{
		return EHSRPartyResult::InvalidCandidate;
	}

	TSet<FName> Seen;
	for (const FHSRPartySlot& Slot : Candidate.Slots)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}
		if (!IsKnownProfile(Slot.CharacterId))
		{
			return EHSRPartyResult::ProfileNotFound;
		}
		if (Seen.Contains(Slot.CharacterId))
		{
			return EHSRPartyResult::DuplicateCharacter;
		}
		Seen.Add(Slot.CharacterId);
	}

	TArray<FHSRPartySlot> SlotsCandidate = Candidate.Slots;
	if (!Commit(MoveTemp(SlotsCandidate)))
	{
		return EHSRPartyResult::InvalidCandidate;
	}

	// 若提交里带有合法的 ActiveSlot 且与当前不同，则更新并再次广播。
	if (Candidate.ActiveSlot >= 0 && Candidate.ActiveSlot < Capacity && !Slots[Candidate.ActiveSlot].IsEmpty())
	{
		if (ActiveSlot != Candidate.ActiveSlot)
		{
			ActiveSlot = Candidate.ActiveSlot;
			++Revision;
			PartyChanged.Broadcast(Revision);
		}
	}
	return EHSRPartyResult::Success;
}

// GetSnapshot：导出当前队伍快照。
bool UHSRPartySubsystem::GetSnapshot(FHSRPartySnapshot& OutSnapshot) const
{
	OutSnapshot.Slots = Slots;
	OutSnapshot.ActiveSlot = ActiveSlot;
	OutSnapshot.Revision = Revision;
	return true;
}

// SetActiveSlot：切换当前出战角色。槽位为空/非法或已是目标槽则直接返回。
EHSRPartyResult UHSRPartySubsystem::SetActiveSlot(int32 Slot)
{
	if (!IsValidSlot(Slot))
	{
		return EHSRPartyResult::InvalidSlot;
	}
	if (Slots[Slot].IsEmpty())
	{
		return EHSRPartyResult::EmptySlot;
	}
	if (ActiveSlot == Slot)
	{
		return EHSRPartyResult::Success;
	}

	ActiveSlot = Slot;
	++Revision;
	UE_LOG(LogTemp, Log, TEXT("HSR.Party SetActiveSlot Slot=%d CharacterId=%s Revision=%lld"),
		Slot, Slots[Slot].IsEmpty() ? TEXT("None") : *Slots[Slot].CharacterId.ToString(), Revision);
	PartyChanged.Broadcast(Revision);
	return EHSRPartyResult::Success;
}

// PrepareRestore：为读档恢复做干跑。窄队伍（旧档）被接受并补齐到当前容量——
// 旧 USaveGame blob 不经过 MigrateToCurrent，因此仍带加宽前的槽位数量。校验每个
// 非空槽位的角色已注册且不重复；恢复后若 ActiveSlot 对应槽位为空，则自动改指到
// 第一个非空槽位。
bool UHSRPartySubsystem::PrepareRestore(const FHSRPartySnapshot& Saved, FHSRPartySnapshot& Out) const
{
	if (Saved.Slots.Num() > Capacity || Saved.Revision < 0 || Saved.ActiveSlot < 0 || Saved.ActiveSlot >= Capacity)
	{
		return false;
	}

	TSet<FName> Seen;
	for (const auto& Slot : Saved.Slots)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}
		if (Seen.Contains(Slot.CharacterId) || !IsKnownProfile(Slot.CharacterId))
		{
			return false;
		}
		Seen.Add(Slot.CharacterId);
	}

	Out = Saved;
	Out.Slots.SetNum(Capacity);
	// 修复 ActiveSlot：若它指向空位，改指第一个非空槽；全空则归零。
	if (Out.Slots[Out.ActiveSlot].IsEmpty())
	{
		Out.ActiveSlot = Out.Slots.IndexOfByPredicate([](const FHSRPartySlot& Slot) { return !Slot.IsEmpty(); });
		if (Out.ActiveSlot == INDEX_NONE)
		{
			Out.ActiveSlot = 0;
		}
	}
	return true;
}
