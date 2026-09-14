#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Curves/CurveFloat.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "../UI/HSRPreBattleCandidateWidget.h"
#include "../UI/HSRUIManagerSubsystem.h"
#include "../Character/HSRCharacterBase.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Progression/HSRCharacterProfileSubsystem.h"
#include "../Party/HSRPartySubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHSRPreBattlePresentationTest,
	"HSR.UI.PreBattleCandidate.Presentation.FourCharactersAndNoPlaceholders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FHSRPreBattlePresentationTest::RunTest(const FString&)
{
	if (!GEngine) return false;
	auto* GI = NewObject<UGameInstance>(GEngine); GI->AddToRoot(); GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	ON_SCOPE_EXIT { GI->Shutdown(); World->DestroyWorld(false); GEngine->DestroyWorldContext(World); GI->RemoveFromRoot(); };
	auto* Profiles = GI->GetSubsystem<UHSRCharacterProfileSubsystem>();
	auto* Party = GI->GetSubsystem<UHSRPartySubsystem>();
	Party->InitializeForDevelopmentTest(Profiles);
	TArray<FName> Ids;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		auto* Definition = NewObject<UHSRCharacterDefinition>(GI);
		Definition->CharacterId = FName(*FString::Printf(TEXT("Automation.PreBattle.Character%d"), Index));
		Definition->DisplayName = FText::FromString(FString::Printf(TEXT("测试角色%d"), Index + 1));
		Definition->MaxLevel = 2;
		auto* Curve = NewObject<UCurveFloat>(Definition); Curve->FloatCurve.AddKey(2, 100);
		Definition->CumulativeExperienceCurve = Curve;
		if (Index < 4) Definition->CharacterClass = AHSRCharacterBase::StaticClass();
		if (!TestEqual(TEXT("Character definition registers"), Profiles->RegisterDefinition(Definition), EHSRCharacterProfileResult::Success)) return false;
		Ids.Add(Definition->CharacterId);
	}
	TestEqual(TEXT("Permanent leader seeded"), Party->AddCharacter(Ids[0], 0), EHSRPartyResult::Success);
	FHSRPartySnapshot Before; Party->GetSnapshot(Before);
	auto* Widget = NewObject<UHSRPreBattleCandidateWidget>(GI);
	Widget->WidgetTree = NewObject<UWidgetTree>(Widget);
	auto* Root = Widget->WidgetTree->ConstructWidget<UVerticalBox>(); Widget->WidgetTree->RootWidget = Root;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Root->AddChild(Widget->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*FString::Printf(TEXT("Text_Slot%d_Character"), Index))));
		Root->AddChild(Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(*FString::Printf(TEXT("Button_ReplaceSlot%d"), Index))));
	}
	auto* Choices = Widget->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SelectionPanel")); Root->AddChild(Choices);
	auto* Legacy = Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_CharacterA")); Choices->AddChild(Legacy);
	auto* Buffs = Widget->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BuffSelection")); Root->AddChild(Buffs);
	Buffs->AddChild(Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_BuffTest")));
	FHSREncounterRequest Template;
	Template.EncounterId = TEXT("Encounter.PreBattle.Automation"); Template.EnemyDefinitionId = TEXT("Enemy.Automation"); Template.BattleMapPath = TEXT("/Game/Maps/Map_Battle");
	Widget->InitializeCandidate(Template);
	TestEqual(TEXT("Only four playable definitions become choices"), Choices->GetChildrenCount(), 4);
	TestNull(TEXT("Legacy CharacterA button removed"), Legacy->GetParent());
	TestEqual(TEXT("No registered buff definitions hides whole buff area"), Buffs->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("No dummy buff buttons remain"), Buffs->GetChildrenCount(), 0);
	for (int32 Index = 1; Index < 4; ++Index)
	{
		auto* Slot = Widget->WidgetTree->FindWidget<UButton>(FName(*FString::Printf(TEXT("Button_ReplaceSlot%d"), Index)));
		Slot->OnClicked.Broadcast();
		TestEqual(TEXT("Clicked replacement button selects correct slot"), Widget->GetSelectedReplacementSlot(), Index);
		CastChecked<UButton>(Choices->GetChildAt(Index))->OnClicked.Broadcast();
		TestEqual(TEXT("Clicked real character is added to selected slot"), Widget->GetCandidateSnapshot().CandidateCharacterIds[Index], Ids[Index]);
	}
	FHSREncounterRequest Confirmed;
	TestEqual(TEXT("Four-person candidate confirms"), Widget->ConfirmCandidate(Confirmed), EHSRPreBattleCandidateResult::Success);
	TestEqual(TEXT("All four real characters carried to encounter"), Confirmed.PlayerPartyIds.Num(), 4);
	TestEqual(TEXT("Slot name comes from display definition"), Widget->WidgetTree->FindWidget<UTextBlock>(TEXT("Text_Slot3_Character"))->GetText().ToString(), FString(TEXT("测试角色4")));
	TestEqual(TEXT("Unregistered example buff cannot enter candidate"), Widget->SetBuff(TEXT("Buff.Test")), EHSRPreBattleCandidateResult::InvalidCandidate);
	TestEqual(TEXT("Cancel restores original draft"), Widget->CancelCandidate(), EHSRPreBattleCandidateResult::Success);
	TestTrue(TEXT("Cancel removes local extra members"), Widget->GetCandidateSnapshot().CandidateCharacterIds[1].IsNone());
	Widget->SetCandidateSlot(1, Ids[1]);
	auto* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	auto* UIManager = NewObject<UHSRUIManagerSubsystem>(LocalPlayer);
	UIManager->RegisterPreBattlePopup(Widget);
	TestEqual(TEXT("Global Back dismisses preparation before routing underlying page"), UIManager->RequestBack(), EHSRUIScreenResult::Success);
	TestTrue(TEXT("Global Back cancels the local draft"), Widget->GetCandidateSnapshot().CandidateCharacterIds[1].IsNone());
	FHSRPartySnapshot After; Party->GetSnapshot(After);
	TestEqual(TEXT("Preparation and cancel never alter permanent party"), After.Revision, Before.Revision);
	return true;
}
#endif
