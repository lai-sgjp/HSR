#include "HSRCharacterDetailWidget.h"

#include "HSRCharacterDetailViewModel.h"
#include "../Party/HSRPartySubsystem.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRCharacterDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshCount = 0;
	bHasCurrentSnapshot = false;
	ViewModel = NewObject<UHSRCharacterDetailViewModel>(this);
	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance ? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	UHSRSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	if (!ViewModel || !Profiles || !Save || !Party)
	{
		UE_LOG(LogTemp, Error, TEXT("P11-005 DetailWidgetInit Result=FAIL Reason=MissingViewModelOrSubsystems"));
		return;
	}

	ViewModel->Initialize(Profiles, Save, Party);
	DetailChangedHandle = ViewModel->OnChanged().AddUObject(this, &UHSRCharacterDetailWidget::HandleDetailChanged);
	EHSRCharacterDetailResult Result = ViewModel->SelectCharacter(TEXT("Character.A"));
	if (Result != EHSRCharacterDetailResult::Success)
	{
		Result = ViewModel->SelectPartySlot0();
	}
	if (Result == EHSRCharacterDetailResult::Success)
	{
		UE_LOG(LogTemp, Log, TEXT("P11-005 DetailWidgetInit Result=SUCCESS SelectionResult=%d"), static_cast<int32>(Result));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("P11-005 DetailWidgetInit Result=FAIL SelectionResult=%d"), static_cast<int32>(Result));
	}
}

void UHSRCharacterDetailWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (DetailChangedHandle.IsValid())
		{
			ViewModel->OnChanged().Remove(DetailChangedHandle);
			DetailChangedHandle.Reset();
		}
		ViewModel->Uninitialize();
		ViewModel = nullptr;
	}
	bHasCurrentSnapshot = false;
	Super::NativeDestruct();
}

bool UHSRCharacterDetailWidget::GetCurrentSnapshot(FHSRCharacterDetailSnapshot& OutSnapshot) const
{
	if (!bHasCurrentSnapshot) return false;
	OutSnapshot = CurrentSnapshot;
	return true;
}

void UHSRCharacterDetailWidget::HandleDetailChanged(const FHSRCharacterDetailSnapshot& Snapshot)
{
	CurrentSnapshot = Snapshot;
	bHasCurrentSnapshot = true;
	++RefreshCount;
	UE_LOG(LogTemp, Log, TEXT("P11-005 DetailRefresh Count=%d Character=%s Revision=%lld Level=%d Experience=%d MaxLevel=%d Valid=%d"), RefreshCount, *Snapshot.CharacterId.ToString(), Snapshot.RuntimeRevision, Snapshot.Level, Snapshot.Experience, Snapshot.MaxLevel, Snapshot.bIsValid ? 1 : 0);
	OnDetailSnapshotChanged(Snapshot);
}
