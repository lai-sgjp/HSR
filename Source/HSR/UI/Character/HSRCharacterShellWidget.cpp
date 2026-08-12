#include "HSRCharacterShellWidget.h"

#include "HSRCharacterShellViewModel.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Party/HSRPartySubsystem.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "../../Save/HSRSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"

// 构造完成：创建并初始化角色壳 ViewModel，订阅其变化，立即拉取一次初始快照。
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
	// 用当前各子系统初始化 ViewModel。
	ViewModel->Initialize(Profiles, Save, Party, Equipment);
	FHSRCharacterShellSnapshot InitialSnapshot;
	if (ViewModel->GetSnapshot(InitialSnapshot))
	{
		HandleShellChanged(InitialSnapshot);
	}
}

// 析构：解绑订阅并卸载 ViewModel。
void UHSRCharacterShellWidget::NativeDestruct()
{
	if (ViewModel)
	{
		if (ShellChangedHandle.IsValid())
		{
			ViewModel->OnChanged().Remove(ShellChangedHandle);
		}
		ShellChangedHandle.Reset();
		ViewModel->Uninitialize();
		ViewModel = nullptr;
	}
	bHasCurrentSnapshot = false;
	Super::NativeDestruct();
}

// 选择角色（转交 ViewModel）。
EHSRCharacterShellResult UHSRCharacterShellWidget::SelectCharacter(FName CharacterId)
{
	return ViewModel ? ViewModel->SelectCharacter(CharacterId) : EHSRCharacterShellResult::NotInitialized;
}

// 切换标签页（转交 ViewModel）。
EHSRCharacterShellResult UHSRCharacterShellWidget::SelectTab(EHSRCharacterShellTab Tab)
{
	return ViewModel ? ViewModel->SelectTab(Tab) : EHSRCharacterShellResult::NotInitialized;
}

// 刷新（转交 ViewModel）。
EHSRCharacterShellResult UHSRCharacterShellWidget::RefreshShell()
{
	return ViewModel ? ViewModel->Refresh() : EHSRCharacterShellResult::NotInitialized;
}

// 取当前快照；尚未有任何快照时返回 false。
bool UHSRCharacterShellWidget::GetCurrentSnapshot(FHSRCharacterShellSnapshot& OutSnapshot) const
{
	if (!bHasCurrentSnapshot)
	{
		return false;
	}
	OutSnapshot = CurrentSnapshot;
	return true;
}

// 快照变化回调：缓存最新快照、累计刷新次数、更新属性文本；
// 按快照有效性决定走“正常展示”还是“不可用”回调。
void UHSRCharacterShellWidget::HandleShellChanged(const FHSRCharacterShellSnapshot& InSnapshot)
{
	CurrentSnapshot = InSnapshot;
	bHasCurrentSnapshot = true;
	++RefreshCount;
	UpdateDetailStats(InSnapshot);
	if (InSnapshot.bIsValid)
	{
		OnShellSnapshotChanged(InSnapshot);
	}
	else
	{
		OnShellUnavailable(InSnapshot.FailureReason);
	}
}

// 用快照中的派生属性刷新详情面板文本（HP/攻击/防御/速度）。
// 快照无效时清空文本，避免显示过期的数值。
void UHSRCharacterShellWidget::UpdateDetailStats(const FHSRCharacterShellSnapshot& InSnapshot)
{
	if (!WidgetTree)
	{
		return;
	}
	const FHSRCharacterDerivedStats& Stats = InSnapshot.CharacterDetail.DerivedStats;
	const bool bAvailable = InSnapshot.CharacterDetail.bIsValid;
	UTextBlock* HP = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_HP"));
	UTextBlock* Attack = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Attack"));
	UTextBlock* Defense = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Defense"));
	UTextBlock* Speed = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Speed"));
	if (HP)
	{
		HP->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "HP", "HP: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.MaxHealth)))
			: FText::GetEmpty());
	}
	if (Attack)
	{
		Attack->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "Atk", "Attack: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.Attack)))
			: FText::GetEmpty());
	}
	if (Defense)
	{
		Defense->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "Def", "Defense: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.Defense)))
			: FText::GetEmpty());
	}
	if (Speed)
	{
		Speed->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "Spd", "Speed: {0}"), FText::AsNumber(FMath::RoundToInt(Stats.Speed)))
			: FText::GetEmpty());
	}
}
