#include "HSRCharacterShellWidget.h"

#include "HSRCharacterShellViewModel.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Party/HSRPartySubsystem.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "../../Save/HSRSaveSubsystem.h"
#include "Engine/GameInstance.h"

void UHSRCharacterShellWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshCount = 0;
	bHasCurrentSnapshot = false;
	ViewModel = NewObject<UHSRCharacterShellViewModel>(this);
	if (!ViewModel)
	{
		OnShellUnavailable(EHSRCharacterShellResult::NotInitialized);
		return;
	}
	ShellChangedHandle = ViewModel->OnChanged().AddUObject(this, &ThisClass::HandleShellChanged);
	UGameInstance* GameInstance = GetGameInstance();
	UHSRCharacterProfileSubsystem* Profiles = GameInstance
		? GameInstance->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	UHSRSaveSubsystem* Save = GameInstance ? GameInstance->GetSubsystem<UHSRSaveSubsystem>() : nullptr;
	UHSRPartySubsystem* Party = GameInstance ? GameInstance->GetSubsystem<UHSRPartySubsystem>() : nullptr;
	UHSREquipmentSubsystem* Equipment = GameInstance
		? GameInstance->GetSubsystem<UHSREquipmentSubsystem>() : nullptr;
	ViewModel->Initialize(Profiles, Save, Party, Equipment);
	FHSRCharacterShellSnapshot InitialSnapshot;
	if (ViewModel->GetSnapshot(InitialSnapshot)) HandleShellChanged(InitialSnapshot);
}

void UHSRCharacterShellWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (ShellChangedHandle.IsValid()) ViewModel->OnChanged().Remove(ShellChangedHandle);
		ShellChangedHandle.Reset();
		ViewModel->Uninitialize();
		ViewModel = nullptr;
	}
	bHasCurrentSnapshot = false;
	Super::NativeDestruct();
}

EHSRCharacterShellResult UHSRCharacterShellWidget::SelectCharacter(FName CharacterId)
{
	return ViewModel ? ViewModel->SelectCharacter(CharacterId) : EHSRCharacterShellResult::NotInitialized;
}

EHSRCharacterShellResult UHSRCharacterShellWidget::SelectTab(EHSRCharacterShellTab Tab)
{
	return ViewModel ? ViewModel->SelectTab(Tab) : EHSRCharacterShellResult::NotInitialized;
}

EHSRCharacterShellResult UHSRCharacterShellWidget::RefreshShell()
{
	return ViewModel ? ViewModel->Refresh() : EHSRCharacterShellResult::NotInitialized;
}

bool UHSRCharacterShellWidget::GetCurrentSnapshot(FHSRCharacterShellSnapshot& OutSnapshot) const
{
	if (!bHasCurrentSnapshot) return false;
	OutSnapshot = CurrentSnapshot;
	return true;
}

void UHSRCharacterShellWidget::HandleShellChanged(const FHSRCharacterShellSnapshot& InSnapshot)
{
	CurrentSnapshot = InSnapshot;
	bHasCurrentSnapshot = true;
	++RefreshCount;
	if (InSnapshot.bIsValid) OnShellSnapshotChanged(InSnapshot);
	else OnShellUnavailable(InSnapshot.FailureReason);
}
