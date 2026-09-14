#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "../UI/Character/HSRCharacterShellWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Curves/CurveFloat.h"
#include "Misc/ScopeExit.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/HSRSkillDefinition.h"

namespace HSR::CharacterShellPresentationTests
{
	struct FFixture
	{
		UHSRCharacterShellWidget* Widget;
		FHSRCharacterShellSnapshot Snapshot;
		explicit FFixture(UObject* Outer = GetTransientPackage()) : Widget(NewObject<UHSRCharacterShellWidget>(Outer))
		{
			Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
			auto* Root = Widget->WidgetTree->ConstructWidget<UVerticalBox>();
			Widget->WidgetTree->RootWidget = Root;
			for (FName Name : {TEXT("SelectedCharacterText"), TEXT("TXT_Level"), TEXT("TXT_HP"),
				TEXT("TXT_Attack"), TEXT("TXT_Defense"), TEXT("TXT_Speed"), TEXT("UnavailableText")})
			{
				auto* Label = Widget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
				Label->SetText(FText::FromString(TEXT("Demo.Character.Internal")));
				Root->AddChild(Label);
			}
			for (FName Name : {TEXT("DetailTabButton"), TEXT("WeaponTabButton"), TEXT("TracesTabButton"),
				TEXT("RelicsTabButton"), TEXT("InformationTabButton"), TEXT("EidolonTabButton"),
				TEXT("OutfitTabButton"), TEXT("BTN_ChangeWeapon")})
			{
				auto* Button = Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
				Button->SetIsEnabled(false); // Reproduce the legacy authored disabled tabs.
				Root->AddChild(Button);
			}
			for (FName Name : {TEXT("StatsBox"), TEXT("ContentHost"), TEXT("CharacterListHost")})
				Root->AddChild(Widget->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), Name));
			Root->AddChild(Widget->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("IMG_CharacterPortrait")));
			Snapshot.bIsValid = true;
			Snapshot.SelectedCharacterId = TEXT("Demo.Character.Internal");
			Snapshot.SelectedTab = EHSRCharacterShellTab::Detail;
			Snapshot.CharacterDetail.bIsValid = true;
			Snapshot.CharacterDetail.DisplayName = FText::FromString(TEXT("花火"));
			Snapshot.CharacterDetail.Level = 12;
			Snapshot.CharacterDetail.MaxLevel = 80;
			Snapshot.CharacterDetail.DerivedStats.MaxHealth = 500;
			Snapshot.EquipmentDetail.bIsValid = true;
		}
		UWidget* Find(FName Name) const { return Widget->WidgetTree->FindWidget(Name); }
		FString Text(FName Name) const { return CastChecked<UTextBlock>(Find(Name))->GetText().ToString(); }
		void Apply() { Widget->ApplySnapshotForAutomation(Snapshot); }
		FString ContentText() const
		{
			FString Result;
			const auto* Content = Cast<UPanelWidget>(Find(TEXT("PR_TabContent")));
			if (Content) for (auto* Child : Content->GetAllChildren())
				if (auto* Label = Cast<UTextBlock>(Child)) Result += Label->GetText().ToString() + TEXT("\n");
			return Result;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterShellPresentationLocalizationTest,
	"HSR.UI.CharacterShell.Presentation.LocalizedSnapshotAndPortrait",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterShellPresentationLocalizationTest::RunTest(const FString&)
{
	using namespace HSR::CharacterShellPresentationTests;
	FFixture Fixture;
	auto* Portrait = NewObject<UTexture2D>(Fixture.Widget);
	Fixture.Snapshot.CharacterDetail.PortraitPath = FSoftObjectPath(Portrait);
	Fixture.Snapshot.CharacterDetail.bHasPortrait = true;
	Fixture.Apply();
	TestEqual(TEXT("display name replaces internal ID"), Fixture.Text(TEXT("SelectedCharacterText")), FString(TEXT("花火")));
	TestTrue(TEXT("health label localized"), Fixture.Text(TEXT("TXT_HP")).StartsWith(TEXT("生命上限")));
	TestTrue(TEXT("level appears"), Fixture.Text(TEXT("TXT_Level")).Contains(TEXT("12")));
	TestEqual(TEXT("actual portrait used"), CastChecked<UImage>(Fixture.Find(TEXT("IMG_CharacterPortrait")))->GetBrush().GetResourceObject(), static_cast<UObject*>(Portrait));
	for (FName Name : {TEXT("DetailTabButton"), TEXT("WeaponTabButton"), TEXT("TracesTabButton"), TEXT("RelicsTabButton"), TEXT("InformationTabButton")})
		TestTrue(*FString::Printf(TEXT("implemented tab enabled: %s"), *Name.ToString()), Fixture.Find(Name)->GetIsEnabled());
	TestEqual(TEXT("no unused eidolon placeholder"), Fixture.Find(TEXT("EidolonTabButton"))->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("no unused outfit placeholder"), Fixture.Find(TEXT("OutfitTabButton"))->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("weapon action hidden on details"), Fixture.Find(TEXT("BTN_ChangeWeapon"))->GetVisibility(), ESlateVisibility::Collapsed);
	Fixture.Snapshot.CharacterDetail.bHasPortrait = false;
	Fixture.Apply();
	TestEqual(TEXT("missing portrait clears previous character image"), Fixture.Find(TEXT("IMG_CharacterPortrait"))->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterShellPresentationTabStateTest,
	"HSR.UI.CharacterShell.Presentation.TabContentAndActionScope",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterShellPresentationTabStateTest::RunTest(const FString&)
{
	using namespace HSR::CharacterShellPresentationTests;
	FFixture Fixture;
	Fixture.Snapshot.SelectedTab = EHSRCharacterShellTab::Weapon;
	Fixture.Apply();
	TestEqual(TEXT("weapon action visible on weapon page"), Fixture.Find(TEXT("BTN_ChangeWeapon"))->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("detail stats hidden on weapon page"), Fixture.Find(TEXT("StatsBox"))->GetVisibility(), ESlateVisibility::Collapsed);
	auto* Content = CastChecked<UPanelWidget>(Fixture.Find(TEXT("PR_TabContent")));
	TestTrue(TEXT("empty weapon page offers real equipment flow"), Content->GetChildrenCount() > 0);
	Fixture.Snapshot.SelectedTab = EHSRCharacterShellTab::Information;
	Fixture.Apply();
	TestEqual(TEXT("weapon action hidden after leaving weapon page"), Fixture.Find(TEXT("BTN_ChangeWeapon"))->GetVisibility(), ESlateVisibility::Collapsed);
	TestTrue(TEXT("archive renders profile progress"), Content->GetChildrenCount() >= 3);
	Fixture.Snapshot.EquipmentDetail.bIsValid = false;
	Fixture.Apply();
	TestFalse(TEXT("unavailable authority cannot be equipped through tab"), Fixture.Find(TEXT("WeaponTabButton"))->GetIsEnabled());
	TestTrue(TEXT("profile tab stays usable without equipment"), Fixture.Find(TEXT("InformationTabButton"))->GetIsEnabled());
	Fixture.Snapshot.bIsValid = false;
	Fixture.Snapshot.CharacterDetail.bIsValid = false;
	Fixture.Apply();
	TestEqual(TEXT("invalid snapshot clears old stats"), Fixture.Text(TEXT("TXT_HP")), FString());
	TestEqual(TEXT("invalid snapshot clears previous page content"), Content->GetChildrenCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRCharacterShellProgressionPresentationTest,
	"HSR.UI.CharacterShell.Presentation.CombatSkillLevelAndExperience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRCharacterShellProgressionPresentationTest::RunTest(const FString&)
{
	if (!GEngine) return false;
	auto* GI = NewObject<UGameInstance>(GEngine);
	GI->AddToRoot();
	GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	ON_SCOPE_EXIT { GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GI->RemoveFromRoot(); };
	auto* Definition = NewObject<UHSRCharacterDefinition>(GI);
	Definition->CharacterId = TEXT("Automation.Character.SkillPresentation");
	Definition->MaxLevel = 2;
	auto* Curve = NewObject<UCurveFloat>(Definition);
	Curve->FloatCurve.AddKey(2, 500);
	Definition->CumulativeExperienceCurve = Curve;
	auto* Skill = NewObject<UHSRSkillDefinition>(Definition);
	Skill->SkillId = TEXT("Automation.Skill.Basic");
	Skill->DisplayName = FText::FromString(TEXT("普攻"));
	Definition->SkillDefinitions.Add(Skill);
	Definition->SkillMaxLevels.Add(Skill->SkillId, 10);
	if (!TestEqual(TEXT("Presentation profile registers"), GI->GetSubsystem<UHSRCharacterProfileSubsystem>()->RegisterDefinition(Definition), EHSRCharacterProfileResult::Success)) return false;
	HSR::CharacterShellPresentationTests::FFixture F(GI);
	F.Snapshot.SelectedCharacterId = Definition->CharacterId;
	F.Snapshot.SelectedTab = EHSRCharacterShellTab::Traces;
	FHSRCharacterDetailSkill Record; Record.SkillId = Skill->SkillId; Record.Level = 0; Record.MaxLevel = 10;
	F.Snapshot.CharacterDetail.Skills.Add(Record);
	F.Apply();
	TestTrue(TEXT("Absent skill progression displays real combat base level"), F.ContentText().Contains(TEXT("普攻　战斗基础等级 1")));
	const FString BaseContent = F.ContentText();
	F.Snapshot.CharacterDetail.Skills[0].Level = 8;
	F.Apply();
	TestEqual(TEXT("Unapplied saved level does not claim stronger combat effects"), F.ContentText(), BaseContent);
	TestFalse(TEXT("No unimplemented skill upgrade action"), F.ContentText().Contains(TEXT("升级")));
	F.Snapshot.SelectedTab = EHSRCharacterShellTab::Detail;
	F.Snapshot.CharacterDetail.ExperienceForCurrentLevel = 100;
	F.Snapshot.CharacterDetail.ExperienceForNextLevel = 500;
	F.Snapshot.CharacterDetail.Experience = 250;
	F.Apply();
	TestTrue(TEXT("Experience progress uses cumulative threshold difference"), F.ContentText().Contains(TEXT("升级经验 150 / 400")));
	TestTrue(TEXT("Experience source is explained"), F.ContentText().Contains(TEXT("通过战斗获得经验")));
	float Percent = -1.f;
	F.Widget->WidgetTree->ForEachWidget([&Percent](UWidget* Widget) { if (auto* Bar = Cast<UProgressBar>(Widget)) Percent = Bar->GetPercent(); });
	TestEqual(TEXT("Experience bar matches snapshot"), Percent, .375f);
	F.Snapshot.CharacterDetail.bAtMaxLevel = true;
	F.Apply();
	TestTrue(TEXT("Max level avoids impossible next-level progress"), F.ContentText().Contains(TEXT("已达当前等级上限")));
	return true;
}

#endif
