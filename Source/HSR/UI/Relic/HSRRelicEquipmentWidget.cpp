#include "HSRRelicEquipmentWidget.h"

#include "HSRRelicEquipmentViewModel.h"
#include "../../Data/Definitions/HSREquipmentEnhancementCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Equipment/HSREquipmentTypes.h"
#include "../../Inventory/HSRInventorySubsystem.h"
#include "Engine/GameInstance.h"

void UHSRRelicEquipmentWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ViewModel = NewObject<UHSRRelicEquipmentViewModel>(this);
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] Construct VM=%d CharacterId=%s valid=%d"),
		this, ViewModel != nullptr, *CharacterId.ToString(), CharacterId.IsValid());
	if (ViewModel)
	{
		SnapshotHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleSnapshot);
		if (CharacterId.IsValid()) InitializeRuntimeContext();
		else OnRelicUnavailable(EHSRRelicEquipmentResult::NotInitialized);
	}
	else
	{
		OnRelicUnavailable(EHSRRelicEquipmentResult::NotInitialized);
	}
}

void UHSRRelicEquipmentWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (SnapshotHandle.IsValid()) ViewModel->OnChanged().Remove(SnapshotHandle);
		SnapshotHandle.Reset();
		ViewModel->Shutdown();
		ViewModel = nullptr;
	}
	bHasSnapshot = false;
	Super::NativeDestruct();
}

void UHSRRelicEquipmentWidget::InitializeForCharacter(const FGuid& InCharacterId,
	UHSRItemEquipmentMappingCatalog* InMappingCatalog,
	UHSREquipmentEnhancementCatalog* InEnhancementCatalog)
{
	CharacterId = InCharacterId;
	if (InMappingCatalog) MappingCatalog = InMappingCatalog;
	if (InEnhancementCatalog) EnhancementCatalog = InEnhancementCatalog;
	if (IsConstructed()) InitializeRuntimeContext();
}

void UHSRRelicEquipmentWidget::InitializeForCharacterProfile(const FName CharacterProfileId)
{
	InitializeForCharacter(HSRCharacterGuidFromProfileName(CharacterProfileId));
}

void UHSRRelicEquipmentWidget::InitializeRuntimeContext()
{
	if (!ViewModel) return;
	UGameInstance* GameInstance = GetGameInstance();
	UHSREquipmentSubsystem* Equipment = GameInstance
		? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	UHSRInventorySubsystem* Inventory = GameInstance
		? GameInstance->GetSubsystem<UHSRInventorySubsystem>() : nullptr;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] InitRuntime equip=%d inv=%d mapCat=%d enhCat=%d char=%d"),
		this, Equipment != nullptr, Inventory != nullptr, MappingCatalog != nullptr,
		EnhancementCatalog != nullptr, CharacterId.IsValid());
	ViewModel->Initialize(Equipment, Inventory, MappingCatalog, EnhancementCatalog, CharacterId);
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::SelectSlot(const EHSRRelicSlot InSlot)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->SelectSlot(InSlot) : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] SelectSlot slot=%d result=%d"),
		this, static_cast<int32>(InSlot), static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::SelectCandidate(const FGuid& InInstanceId)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->SelectCandidate(InInstanceId) : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] SelectCandidate id=%s result=%d"),
		this, *InInstanceId.ToString(), static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::OpenEnhancement()
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->OpenEnhancement() : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] OpenEnhancement result=%d"), this, static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::CommitSelectedMovement()
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->CommitSelectedMovement() : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] CommitMovement result=%d"), this, static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::CommitEnhancement(const int32 TargetLevel)
{
	const EHSRRelicEquipmentResult Result = ViewModel
		? ViewModel->CommitEnhancement(TargetLevel) : EHSRRelicEquipmentResult::NotInitialized;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] CommitEnhancement target=%d result=%d"),
		this, TargetLevel, static_cast<int32>(Result));
	return Result;
}

EHSRRelicEquipmentResult UHSRRelicEquipmentWidget::Back()
{
	return ViewModel ? ViewModel->Back() : EHSRRelicEquipmentResult::NotInitialized;
}

bool UHSRRelicEquipmentWidget::GetCurrentSnapshot(FHSRRelicEquipmentSnapshot& OutSnapshot) const
{
	if (!bHasSnapshot) return false;
	OutSnapshot = CurrentSnapshot;
	return true;
}

bool UHSRRelicEquipmentWidget::GetEnhancementOption(const int32 Index,
	FHSRRelicEnhancementOption& OutOption) const
{
	if (!bHasSnapshot || !CurrentSnapshot.EnhancementOptions.IsValidIndex(Index)) return false;
	OutOption = CurrentSnapshot.EnhancementOptions[Index];
	return true;
}

int32 UHSRRelicEquipmentWidget::GetEnhancementOptionCount() const
{
	return bHasSnapshot ? CurrentSnapshot.EnhancementOptions.Num() : 0;
}

bool UHSRRelicEquipmentWidget::HasEnhancementOptions() const
{
	return GetEnhancementOptionCount() > 0;
}

void UHSRRelicEquipmentWidget::HandleSnapshot(const FHSRRelicEquipmentSnapshot& InSnapshot)
{
	CurrentSnapshot = InSnapshot;
	bHasSnapshot = true;
	UE_LOG(LogTemp, Log, TEXT("HSRRelic[%p] Snapshot stage=%d valid=%d reason=%d options=%d slots=%d cand=%d curInst=%d"),
		this, static_cast<int32>(InSnapshot.Stage), InSnapshot.bIsValid,
		static_cast<int32>(InSnapshot.FailureReason), InSnapshot.EnhancementOptions.Num(),
		InSnapshot.Slots.Num(), InSnapshot.Candidates.Num(), InSnapshot.CurrentInstanceId.IsValid());
	OnRelicSnapshotChanged(InSnapshot);
	if (!InSnapshot.bIsValid) OnRelicUnavailable(InSnapshot.FailureReason);
}
