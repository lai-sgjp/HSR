#include "HSRInteractionComponent.h"
#include "HSRInteractableInterface.h"

// 构造函数：关闭 Tick，初始化候选标记。
UHSRInteractionComponent::UHSRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bCandidateEverRegistered = false;
}

// 登记一个交互候选：同一时刻只接受一个候选；候选必须实现交互接口。
void UHSRInteractionComponent::RegisterCandidate(AActor* Candidate)
{
	if (!Candidate)
	{
		return;
	}

	// 已是同一候选，忽略重复登记。
	if (CurrentCandidate.IsValid() && CurrentCandidate.Get() == Candidate)
	{
		return;
	}

	// 已有其他候选时拒绝新候选（单候选模型）。
	if (CurrentCandidate.IsValid() && CurrentCandidate.Get() != Candidate)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::RegisterCandidate - Already has candidate %s, rejecting %s"),
			*CurrentCandidate.Get()->GetName(), *Candidate->GetName());
		return;
	}

	// 候选必须实现交互接口。
	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("UHSRInteractionComponent::RegisterCandidate - %s does not implement IHSRInteractableInterface"), *Candidate->GetName());
		return;
	}

	CurrentCandidate = Candidate;
	bCandidateEverRegistered = true;
	OnCandidateChanged.Broadcast(Candidate);
	UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::RegisterCandidate - Owner=%s Registered %s"),
		*GetOwner()->GetName(), *Candidate->GetName());
}

// 注销候选：仅当与当前候选匹配时清空。
void UHSRInteractionComponent::UnregisterCandidate(AActor* Candidate)
{
	if (!Candidate || !CurrentCandidate.IsValid())
	{
		return;
	}

	if (CurrentCandidate.Get() == Candidate)
	{
		AActor* OldCandidate = CurrentCandidate.Get();
		CurrentCandidate.Reset();
		bCandidateEverRegistered = false;
		OnCandidateChanged.Broadcast(nullptr);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::UnregisterCandidate - Owner=%s Unregistered %s"),
			*GetOwner()->GetName(), *OldCandidate->GetName());
	}
}

// 当前是否有一个可用的交互候选。
bool UHSRInteractionComponent::HasValidCandidate() const
{
	return IsCandidateValid();
}

// 当前候选的交互提示文本。
FText UHSRInteractionComponent::GetCurrentPrompt() const
{
	AActor* Candidate = CurrentCandidate.Get();
	if (!Candidate || !Candidate->Implements<UHSRInteractableInterface>())
	{
		return FText::GetEmpty();
	}
	return IHSRInteractableInterface::Execute_GetInteractionPrompt(Candidate);
}

// 校验候选是否仍有效：存在、实现接口、且交互可用。
bool UHSRInteractionComponent::IsCandidateValid() const
{
	AActor* Candidate = CurrentCandidate.Get();
	if (!Candidate)
	{
		return false;
	}

	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		return false;
	}

	if (!IHSRInteractableInterface::Execute_IsInteractionAvailable(Candidate))
	{
		return false;
	}

	return true;
}

// 尝试与当前候选交互：一路处理“候选已销毁/从未登记/不再实现接口/当前不可用”的失败，
// 最后真正执行交互并广播结果。
FHSRInteractionResult UHSRInteractionComponent::TryInteract()
{
	AActor* Candidate = CurrentCandidate.Get();
	AActor* OwnerActor = GetOwner();

	// 候选曾经登记过，但弱引用已失效（目标被销毁）。
	if (!Candidate && bCandidateEverRegistered)
	{
		CurrentCandidate.Reset();
		bCandidateEverRegistered = false;
		OnCandidateChanged.Broadcast(nullptr);
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Candidate was destroyed.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s FAILED TargetInvalid (candidate destroyed)"),
			*GetOwner()->GetName());
		return Result;
	}

	// 从来没有候选（或候选被正常注销）。
	if (!Candidate)
	{
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::NoCandidate,
			FText::FromString(TEXT("No interaction candidate available.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s FAILED NoCandidate"),
			*GetOwner()->GetName());
		return Result;
	}

	// 候选已不再实现交互接口。
	if (!Candidate->Implements<UHSRInteractableInterface>())
	{
		CurrentCandidate.Reset();
		bCandidateEverRegistered = false;
		OnCandidateChanged.Broadcast(nullptr);
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::TargetInvalid,
			FText::FromString(TEXT("Candidate no longer implements the interaction interface.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s FAILED TargetInvalid (no interface)"),
			*GetOwner()->GetName(), *Candidate->GetName());
		return Result;
	}

	// 候选当前不可用。
	if (!IHSRInteractableInterface::Execute_IsInteractionAvailable(Candidate))
	{
		FHSRInteractionResult Result = FHSRInteractionResult::MakeFailure(
			EHSRInteractionFailureReason::Unavailable,
			FText::FromString(TEXT("Candidate is currently unavailable.")));
		OnInteractionCompleted.Broadcast(Result);
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s FAILED Unavailable"),
			*GetOwner()->GetName(), *Candidate->GetName());
		return Result;
	}

	// 执行交互（上下文携带交互者与其位置）。
	FHSRInteractionContext Context(OwnerActor, OwnerActor->GetActorLocation());
	FHSRInteractionResult Result = IHSRInteractableInterface::Execute_ExecuteInteraction(Candidate, Context);

	if (!Result.bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s FAILED ExecutionFailed reason=%d"),
			*GetOwner()->GetName(), *Candidate->GetName(), static_cast<int32>(Result.FailureReason));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("UHSRInteractionComponent::TryInteract - Owner=%s Candidate=%s SUCCESS"),
			*GetOwner()->GetName(), *Candidate->GetName());
	}

	OnInteractionCompleted.Broadcast(Result);
	return Result;
}

// 组件销毁前：清理候选与委托。
void UHSRInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CurrentCandidate.Reset();
	bCandidateEverRegistered = false;
	OnCandidateChanged.Clear();
	OnInteractionCompleted.Clear();
	Super::EndPlay(EndPlayReason);
}
