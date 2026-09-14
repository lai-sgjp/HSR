#include "HSRCharacterShellWidget.h"

#include "HSRCharacterShellViewModel.h"
#include "../HSRUIManagerSubsystem.h"
#include "../../Equipment/HSREquipmentSubsystem.h"
#include "../../Party/HSRPartySubsystem.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "../../Save/HSRSaveSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "../Relic/HSRRelicEquipmentWidget.h"
#include "../../Data/Definitions/HSRCharacterDefinition.h"
#include "../../Data/Definitions/HSRInventoryCatalog.h"
#include "../../Data/Definitions/HSRItemEquipmentMappingCatalog.h"
#include "../../Data/HSRSkillDefinition.h"

// 构造完成：创建并初始化角色壳 ViewModel，订阅其变化，立即拉取一次初始快照。
void UHSRCharacterShellWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (WidgetTree)
	{
		if (UButton* ChangeWeapon = WidgetTree->FindWidget<UButton>(TEXT("BTN_ChangeWeapon")))
			ChangeWeapon->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleChangeWeaponClicked);
	}
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
	if (WidgetTree)
	{
		if (UButton* ChangeWeapon = WidgetTree->FindWidget<UButton>(TEXT("BTN_ChangeWeapon")))
			ChangeWeapon->OnClicked.RemoveDynamic(this, &ThisClass::HandleChangeWeaponClicked);
	}
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
	ChoiceBindings.Reset();
	RelicPanel = nullptr;
	NativeTabContent = nullptr;
	RelicPanelCharacterId = NAME_None;
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

bool UHSRCharacterShellWidget::RequestChangeWeapon()
{
	UHSRUIManagerSubsystem* Manager = GetOwningUIManager();
	if (!Manager || !bHasCurrentSnapshot || !CurrentSnapshot.bIsValid
		|| CurrentSnapshot.SelectedTab != EHSRCharacterShellTab::Weapon) return false;
	const EHSRUIScreenResult Result = Manager->OpenInventoryForCharacter(CurrentSnapshot.SelectedCharacterId);
	return Result == EHSRUIScreenResult::Success || Result == EHSRUIScreenResult::NoOp;
}

void UHSRCharacterShellWidget::HandleChangeWeaponClicked()
{
	RequestChangeWeapon();
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
	if (InSnapshot.bIsValid)
	{
		OnShellSnapshotChanged(InSnapshot);
	}
	else
	{
		OnShellUnavailable(InSnapshot.FailureReason);
	}
	RefreshPresentation(); // Native presentation is applied after legacy Blueprint callbacks.
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
	const bool bAvailable = InSnapshot.bIsValid && InSnapshot.CharacterDetail.bIsValid;
	UTextBlock* HP = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_HP"));
	UTextBlock* Attack = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Attack"));
	UTextBlock* Defense = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Defense"));
	UTextBlock* Speed = WidgetTree->FindWidget<UTextBlock>(TEXT("TXT_Speed"));
	if (HP)
	{
		HP->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "HP", "生命上限　{0}"), FText::AsNumber(FMath::RoundToInt(Stats.MaxHealth)))
			: FText::GetEmpty());
	}
	if (Attack)
	{
		Attack->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "Atk", "攻击力　{0}"), FText::AsNumber(FMath::RoundToInt(Stats.Attack)))
			: FText::GetEmpty());
	}
	if (Defense)
	{
		Defense->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "Def", "防御力　{0}"), FText::AsNumber(FMath::RoundToInt(Stats.Defense)))
			: FText::GetEmpty());
	}
	if (Speed)
	{
		Speed->SetText(bAvailable
			? FText::Format(NSLOCTEXT("HSRShell", "Spd", "速度　{0}"), FText::AsNumber(FMath::RoundToInt(Stats.Speed)))
			: FText::GetEmpty());
	}
}

void UHSRCharacterChoiceButton::Initialize(UHSRCharacterShellWidget* InOwner, FName InCharacterId, int32 InTab)
{
	Owner = InOwner; CharacterId = InCharacterId; Tab = InTab;
}

void UHSRCharacterChoiceButton::HandleClicked()
{
	if (!Owner.IsValid()) return;
	if (Tab == INDEX_NONE) Owner->SelectCharacter(CharacterId);
	else Owner->SelectTab(static_cast<EHSRCharacterShellTab>(Tab));
}

void UHSRCharacterShellWidget::RefreshPresentation()
{
	if (!WidgetTree) return;
	UpdateDetailStats(CurrentSnapshot);
	const auto Text = [this](FName Name, const FText& Value)
	{
		if (auto* Label = WidgetTree->FindWidget<UTextBlock>(Name))
		{
			Label->SetText(Value);
			Label->SetColorAndOpacity(FSlateColor(FLinearColor(.91f,.94f,1.f)));
			Label->SetIsEnabled(true);
		}
	};
	const auto& Detail = CurrentSnapshot.CharacterDetail;
	Text(TEXT("SelectedCharacterText"), Detail.DisplayName.IsEmpty()
		? NSLOCTEXT("HSRShell", "UnnamedCharacter", "角色") : Detail.DisplayName);
	Text(TEXT("TXT_Level"), FText::Format(NSLOCTEXT("HSRShell", "Level", "等级 {0} / {1}"),
		FText::AsNumber(Detail.Level), FText::AsNumber(Detail.MaxLevel)));
	if (auto* Portrait = WidgetTree->FindWidget<UImage>(TEXT("IMG_CharacterPortrait")))
	{
		auto* Texture = Detail.bHasPortrait ? Cast<UTexture2D>(Detail.PortraitPath.TryLoad()) : nullptr;
		Portrait->SetBrushFromTexture(Texture);
		Portrait->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	ChoiceBindings.Reset();
	struct FTab { const TCHAR* Button; const TCHAR* Label; const TCHAR* Text; EHSRCharacterShellTab Tab; };
	const FTab Tabs[] = {
		{TEXT("DetailTabButton"), TEXT("TXT_Detail"), TEXT("角色"), EHSRCharacterShellTab::Detail},
		{TEXT("WeaponTabButton"), TEXT("TXT_Weapon"), TEXT("武器"), EHSRCharacterShellTab::Weapon},
		{TEXT("TracesTabButton"), TEXT("TXT_Traces"), TEXT("技能"), EHSRCharacterShellTab::Traces},
		{TEXT("RelicsTabButton"), TEXT("TXT_Relics"), TEXT("遗器"), EHSRCharacterShellTab::Relics},
		{TEXT("InformationTabButton"), TEXT("TXT_Information"), TEXT("档案"), EHSRCharacterShellTab::Information}
	};
	for (const FTab& Tab : Tabs)
	{
		Text(Tab.Label, FText::FromString(Tab.Text));
		if (auto* Button = WidgetTree->FindWidget<UButton>(Tab.Button))
		{
			Button->SetVisibility(ESlateVisibility::Visible);
			Button->SetRenderOpacity(1.f);
			const bool bEquipmentTab = Tab.Tab == EHSRCharacterShellTab::Weapon || Tab.Tab == EHSRCharacterShellTab::Relics;
			Button->SetIsEnabled(CurrentSnapshot.bIsValid && (!bEquipmentTab || CurrentSnapshot.EquipmentDetail.bIsValid));
			Button->SetBackgroundColor(CurrentSnapshot.SelectedTab == Tab.Tab
				? FLinearColor(.72f, .53f, .2f, 1.f) : FLinearColor(.08f, .12f, .19f, 1.f));
			Button->OnClicked.Clear();
			auto* Binding = NewObject<UHSRCharacterChoiceButton>(this);
			Binding->Initialize(this, NAME_None, static_cast<int32>(Tab.Tab));
			Button->OnClicked.AddDynamic(Binding, &UHSRCharacterChoiceButton::HandleClicked);
			ChoiceBindings.Add(Binding);
		}
	}
	for (FName Name : {FName(TEXT("EidolonTabButton")), FName(TEXT("OutfitTabButton"))})
		if (auto* Widget = WidgetTree->FindWidget(Name)) Widget->SetVisibility(ESlateVisibility::Collapsed);
	if (auto* Button = WidgetTree->FindWidget<UButton>(TEXT("BTN_ChangeWeapon")))
	{
		Button->SetVisibility(CurrentSnapshot.bIsValid && CurrentSnapshot.SelectedTab == EHSRCharacterShellTab::Weapon
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button->SetIsEnabled(CurrentSnapshot.bIsValid && CurrentSnapshot.EquipmentDetail.bIsValid);
	}
	if (auto* Unavailable = WidgetTree->FindWidget<UTextBlock>(TEXT("UnavailableText")))
	{
		Unavailable->SetText(NSLOCTEXT("HSRShell", "Unavailable", "角色资料暂不可用"));
		Unavailable->SetVisibility(CurrentSnapshot.bIsValid ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (auto* Stats = WidgetTree->FindWidget(TEXT("StatsBox")))
		Stats->SetVisibility(CurrentSnapshot.SelectedTab == EHSRCharacterShellTab::Detail
			? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	PopulateCharacters();
	PopulateTabContent();
}

void UHSRCharacterShellWidget::PopulateCharacters()
{
	auto* Host = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("CharacterListHost")));
	if (!Host) return;
	Host->ClearChildren();
	Host->SetVisibility(ESlateVisibility::Visible);
	auto* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
	for (const auto& Entry : CurrentSnapshot.CharacterEntries)
	{
		const UHSRCharacterDefinition* Definition = nullptr;
		if (Profiles) Profiles->GetDefinition(Entry.CharacterId, Definition);
		auto* Button = WidgetTree->ConstructWidget<UButton>();
		Button->SetIsEnabled(Entry.bIsAvailable);
		Button->SetBackgroundColor(Entry.bIsSelected ? FLinearColor(.6f, .43f, .17f, 1.f) : FLinearColor(.07f, .1f, .16f, 1.f));
		auto* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
		Button->SetContent(Row);
		if (auto* Texture = Definition ? Definition->Portrait.LoadSynchronous() : nullptr)
		{
			auto* Frame = WidgetTree->ConstructWidget<USizeBox>();
			Frame->SetWidthOverride(64); Frame->SetHeightOverride(80);
			auto* Picture = WidgetTree->ConstructWidget<UImage>();
			Picture->SetBrushFromTexture(Texture); Frame->SetContent(Picture);
			Row->AddChildToHorizontalBox(Frame)->SetPadding(FMargin(4, 4, 12, 4));
		}
		auto* Name = WidgetTree->ConstructWidget<UTextBlock>();
		Name->SetText(Definition && !Definition->DisplayName.IsEmpty() ? Definition->DisplayName
			: !Entry.DisplayName.IsEmpty() ? Entry.DisplayName : NSLOCTEXT("HSRShell", "UnnamedCharacter", "角色"));
		FSlateFontInfo Font = Name->GetFont(); Font.Size = 19; Name->SetFont(Font);
		Name->SetColorAndOpacity(FSlateColor(FLinearColor(.94f, .95f, 1.f)));
		Row->AddChildToHorizontalBox(Name)->SetVerticalAlignment(VAlign_Center);
		if (auto* ButtonSlot = Cast<UVerticalBoxSlot>(Host->AddChild(Button))) ButtonSlot->SetPadding(FMargin(0, 0, 0, 8));
		auto* Binding = NewObject<UHSRCharacterChoiceButton>(this);
		Binding->Initialize(this, Entry.CharacterId);
		Button->OnClicked.AddDynamic(Binding, &UHSRCharacterChoiceButton::HandleClicked);
		ChoiceBindings.Add(Binding);
	}
}

void UHSRCharacterShellWidget::AddContentText(UPanelWidget* Host, const FText& Text, int32 FontSize)
{
	if (!Host) return;
	auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(Text); Label->SetAutoWrapText(true);
	FSlateFontInfo Font = Label->GetFont(); Font.Size = FontSize; Label->SetFont(Font);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor(.91f, .94f, 1.f)));
	if (auto* LabelSlot = Cast<UVerticalBoxSlot>(Host->AddChild(Label))) LabelSlot->SetPadding(FMargin(0, 5, 0, 12));
}

FText UHSRCharacterShellWidget::EquipmentName(FName DefinitionId) const
{
	FHSRItemEquipmentMappingEntry Mapping;
	FHSRInventoryCatalogEntry Entry;
	if (EquipmentMappingCatalog && PresentationCatalog
		&& EquipmentMappingCatalog->ResolveEquipmentDefinition(DefinitionId, Mapping)
		&& PresentationCatalog->FindEntry(Mapping.ItemId, Entry) && !Entry.DisplayName.IsEmpty()) return Entry.DisplayName;
	return NSLOCTEXT("HSRShell", "Equipment", "装备");
}

void UHSRCharacterShellWidget::PopulateTabContent()
{
	auto* Container = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("ContentHost")));
	if (!Container) return;
	Container->SetVisibility(ESlateVisibility::Visible);
	if (!NativeTabContent)
	{
		NativeTabContent = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("PR_TabContent")));
		if (!NativeTabContent)
			NativeTabContent = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PR_TabContent"));
	}
	auto* Host = NativeTabContent.Get();
	if (!Host->GetParent()) Container->AddChild(Host);
	Host->ClearChildren();
	Host->SetVisibility(ESlateVisibility::Visible);
	const auto Tab = CurrentSnapshot.SelectedTab;
	const bool bRelics = CurrentSnapshot.bIsValid && Tab == EHSRCharacterShellTab::Relics;
	if (auto* RelicHost = WidgetTree->FindWidget(TEXT("RelicHostSizeBox")))
		RelicHost->SetVisibility(bRelics ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!CurrentSnapshot.bIsValid) return;
	const auto& Detail = CurrentSnapshot.CharacterDetail;
	if (Tab == EHSRCharacterShellTab::Detail)
	{
		if (!WidgetTree->FindWidget(TEXT("TXT_Level")))
			AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "Level", "等级 {0} / {1}"),
				FText::AsNumber(Detail.Level), FText::AsNumber(Detail.MaxLevel)));
		const int32 Required = FMath::Max(0, Detail.ExperienceForNextLevel - Detail.ExperienceForCurrentLevel);
		const int32 Earned = FMath::Clamp(Detail.Experience - Detail.ExperienceForCurrentLevel, 0, Required);
		AddContentText(Host, Detail.bAtMaxLevel ? NSLOCTEXT("HSRShell", "MaxLevel", "已达当前等级上限")
			: FText::Format(NSLOCTEXT("HSRShell", "ExperienceProgress", "升级经验 {0} / {1}"),
				FText::AsNumber(Earned), FText::AsNumber(Required)));
		auto* Progress = WidgetTree->ConstructWidget<UProgressBar>();
		Progress->SetPercent(Detail.bAtMaxLevel ? 1.f : Required > 0 ? static_cast<float>(Earned) / Required : 0.f);
		Progress->SetFillColorAndOpacity(FLinearColor(.82f, .65f, .28f));
		auto* Frame = WidgetTree->ConstructWidget<USizeBox>();
		Frame->SetHeightOverride(12.f);
		Frame->SetContent(Progress);
		Host->AddChild(Frame);
		AddContentText(Host, NSLOCTEXT("HSRShell", "ExperienceSource", "通过战斗获得经验"), 18);
	}
	else if (Tab == EHSRCharacterShellTab::Weapon)
	{
		bool bFound = false;
		for (const auto& Item : CurrentSnapshot.EquipmentDetail.Items)
		{
			if (Item.Kind != EHSREquipmentKind::Equipment) continue;
			bFound = true;
			AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "EquippedWeapon", "已装备：{0} +{1}"),
				EquipmentName(Item.DefinitionId), FText::AsNumber(Item.EnhancementLevel)), 24);
			for (const auto& Modifier : Item.Modifiers)
			{
				const TCHAR* Stat = Modifier.Stat == EHSREquipmentStat::Attack ? TEXT("攻击力")
					: Modifier.Stat == EHSREquipmentStat::MaxHealth ? TEXT("生命上限")
					: Modifier.Stat == EHSREquipmentStat::Defense ? TEXT("防御力") : TEXT("速度");
				AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "WeaponBonus", "{0} +{1}"),
					FText::FromString(Stat), FText::AsNumber(Modifier.Value)));
			}
		}
		if (!bFound) AddContentText(Host, NSLOCTEXT("HSRShell", "NoWeapon", "尚未装备武器。点击更换武器，从背包选择并对比属性。"));
	}
	else if (Tab == EHSRCharacterShellTab::Traces)
	{
		const UHSRCharacterDefinition* Definition = nullptr;
		auto* Profiles = GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr;
		if (Profiles) Profiles->GetDefinition(CurrentSnapshot.SelectedCharacterId, Definition);
		bool bFound = false;
		if (Definition) for (const auto& Reference : Definition->SkillDefinitions)
		{
			const auto* Skill = Reference.LoadSynchronous();
			if (!Skill) continue;
			bFound = true;
			FText Name = Skill->DisplayName.IsEmpty() ? NSLOCTEXT("HSRShell", "Skill", "技能") : Skill->DisplayName;
			// GrantSingleSkill grants level 1. Saved SkillLevels currently have no combat
			// scaling contract, so neither an absent record (0) nor a stored upgrade is power.
			Name = FText::Format(NSLOCTEXT("HSRShell", "BattleSkillBaseLevel", "{0}　战斗基础等级 1"), Name);
			AddContentText(Host, Name, 24);
			if (!Skill->Description.IsEmpty()) AddContentText(Host, Skill->Description, 18);
			float Energy = 0;
			Skill->CostGameplayEffectClass.LoadSynchronous();
			if (Skill->TryGetDisplayEnergyCost(Energy) && Energy > 0)
				AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "SkillEnergy", "能量消耗 {0}"), FText::AsNumber(Energy)), 18);
		}
		if (!bFound) AddContentText(Host, NSLOCTEXT("HSRShell", "NoSkills", "暂无已配置的技能资料"));
	}
	else if (Tab == EHSRCharacterShellTab::Information)
	{
		AddContentText(Host, Detail.DisplayName, 28);
		AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "ProfileProgress", "等级 {0} / {1}　突破阶数 {2}"),
			FText::AsNumber(Detail.Level), FText::AsNumber(Detail.MaxLevel), FText::AsNumber(Detail.Ascension)));
		AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "ProfileExperience", "累计经验 {0}"), FText::AsNumber(Detail.Experience)));
		if (!Detail.bAtMaxLevel) AddContentText(Host, FText::Format(NSLOCTEXT("HSRShell", "NextLevel", "距离下一级还需 {0} 经验"),
			FText::AsNumber(FMath::Max(0, Detail.ExperienceForNextLevel - Detail.Experience))), 18);
	}
	else if (bRelics)
	{
		auto* RelicHost = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("RelicPanelHost")));
		if (RelicHost && RelicWidgetClass)
		{
			if (!RelicPanel) RelicPanel = CreateWidget<UHSRRelicEquipmentWidget>(GetOwningPlayer(), RelicWidgetClass);
			if (RelicPanel)
			{
				if (RelicPanelCharacterId != CurrentSnapshot.SelectedCharacterId)
				{
					RelicPanel->InitializeForCharacterProfile(CurrentSnapshot.SelectedCharacterId);
					RelicPanelCharacterId = CurrentSnapshot.SelectedCharacterId;
				}
				if (RelicPanel->GetParent() != RelicHost) { RelicHost->ClearChildren(); RelicHost->AddChild(RelicPanel); }
				RelicHost->SetVisibility(ESlateVisibility::Visible);
			}
		}
		else AddContentText(Host, NSLOCTEXT("HSRShell", "RelicBindingMissing", "遗器面板尚未就绪"));
	}
}
