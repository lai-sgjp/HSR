#include "HSRCharacterShellWidget.h"

#include "HSRCharacterShellViewModel.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Party/HSRPartySubsystem.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "../../Save/HSRSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
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
	UpdateDetailStats(InSnapshot);
	if (InSnapshot.bIsValid) OnShellSnapshotChanged(InSnapshot);
	else OnShellUnavailable(InSnapshot.FailureReason);
}

void UHSRCharacterShellWidget::UpdateDetailStats(const FHSRCharacterShellSnapshot& InSnapshot)
{
	if (!WidgetTree) return;
	const FHSRCharacterDerivedStats& Stats = InSnapshot.CharacterDetail.DerivedStats;
	const bool bAvailable = InSnapshot.CharacterDetail.bIsValid;
	UTextBlock* HP = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_HP"));
	UTextBlock* Attack = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Attack"));
	UTextBlock* Defense = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Defense"));
	UTextBlock* Speed = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Speed"));
	if (HP) HP->SetText(bAvailable ? FText::Format(NSLOCTEXT("HSRShell", "HP", "HP: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.MaxHealth))) : FText::GetEmpty());
	if (Attack) Attack->SetText(bAvailable ? FText::Format(NSLOCTEXT("HSRShell", "Atk", "Attack: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.Attack))) : FText::GetEmpty());
	if (Defense) Defense->SetText(bAvailable ? FText::Format(NSLOCTEXT("HSRShell", "Def", "Defense: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.Defense))) : FText::GetEmpty());
	if (Speed) Speed->SetText(bAvailable ? FText::Format(NSLOCTEXT("HSRShell", "Spd", "Speed: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.Speed))) : FText::GetEmpty());
}
