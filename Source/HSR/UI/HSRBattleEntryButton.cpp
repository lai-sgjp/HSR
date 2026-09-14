#include "HSRBattleEntryButton.h"
#include "HSRBattleCommandWidget.h"
#include "Components/VerticalBox.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/Texture2D.h"

void UHSRBattleEntryButton::InitializeCard(UHSRBattleCommandWidget* InOwner, FName InId, bool bInSkill)
{
    Owner=InOwner; Id=InId; bSkill=bInSkill;
    auto* Box=NewObject<UVerticalBox>(this);
    auto* Size=NewObject<USizeBox>(this);
    Size->SetWidthOverride(bSkill ? 150 : 135); Size->AddChild(Box); AddChild(Size);
    Portrait=NewObject<UImage>(this); Portrait->SetDesiredSizeOverride(FVector2D(60,60));
    Portrait->SetVisibility(ESlateVisibility::Collapsed);
    if (!bSkill)
    {
        auto* PortraitFrame=NewObject<USizeBox>(this);
        PortraitFrame->SetWidthOverride(60); PortraitFrame->SetHeightOverride(60);
        PortraitFrame->SetVisibility(ESlateVisibility::Collapsed);
        PortraitFrame->AddChild(Portrait);
        Box->AddChildToVerticalBox(PortraitFrame)->SetHorizontalAlignment(HAlign_Center);
    }
    Label=NewObject<UTextBlock>(this); Label->SetColorAndOpacity(FSlateColor(FLinearColor(.95,.96,1)));
    auto Font=Label->GetFont(); Font.Size=16; Label->SetFont(Font); Box->AddChild(Label);
    Health=NewObject<UProgressBar>(this); Health->SetFillColorAndOpacity(FLinearColor(.25,.85,.7));
    Energy=NewObject<UProgressBar>(this); Energy->SetFillColorAndOpacity(FLinearColor(.9,.67,.25));
    if (!bSkill) { Box->AddChild(Health); Box->AddChild(Energy); }
    Detail=NewObject<UTextBlock>(this); Font.Size=12; Detail->SetFont(Font); Detail->SetAutoWrapText(true); Box->AddChild(Detail);
    SetSelected(false);
    OnClicked.AddUniqueDynamic(this,&ThisClass::Select);
}
void UHSRBattleEntryButton::SetSelected(bool bSelected)
{
    FButtonStyle Style=GetStyle();
    Style.Normal.TintColor=FSlateColor(bSelected ? FLinearColor(.35,.24,.085,.95) : FLinearColor(.025,.06,.10,.9));
    Style.Hovered.TintColor=FSlateColor(FLinearColor(.1,.24,.32,.95));
    Style.Pressed.TintColor=FSlateColor(FLinearColor(.5,.34,.12,1));
    Style.NormalPadding=FMargin(10,7); SetStyle(Style);
}
void UHSRBattleEntryButton::ShowParticipant(const FHSRBattleParticipantView& V,bool bSelected,bool bAvailable)
{
    Label->SetText(V.GetDisplayLabel());
    UTexture2D* Texture = V.Portrait.LoadSynchronous();
    Portrait->SetBrushFromTexture(Texture);
    Portrait->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (auto* Frame=Portrait->GetParent()) Frame->SetVisibility(Texture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    Health->SetPercent(V.MaxHealth>0 ? V.Health/V.MaxHealth : 0);
    Energy->SetPercent(V.MaxEnergy>0 ? V.Energy/V.MaxEnergy : 0);
    Detail->SetText(FText::Format(NSLOCTEXT("HSRBattle","CardHP","{0} / {1}"),FText::AsNumber(FMath::CeilToInt(V.Health)),FText::AsNumber(FMath::CeilToInt(V.MaxHealth))));
    bCanSelect = bAvailable && !V.bDefeated;
    // Disabled Slate buttons dim their entire subtree, including vital combat information.
    // Keep the card readable and gate only its selection intent.
    SetIsEnabled(true); SetSelected(bSelected);
    Label->SetColorAndOpacity(FSlateColor(V.bDefeated ? FLinearColor(.65f,.67f,.71f) : FLinearColor(.95f,.96f,1.f)));
    SetToolTipText(V.bDefeated ? NSLOCTEXT("HSRBattle", "DefeatedCard", "已倒下")
        : bCanSelect ? NSLOCTEXT("HSRBattle", "SelectableCard", "选择此目标")
        : NSLOCTEXT("HSRBattle", "InformationCard", "当前技能无法选择此目标"));
}
void UHSRBattleEntryButton::ShowSkill(const FHSRBattleCommandSkillView& V,bool bSelected,bool bUnlocked)
{
    Label->SetText(V.DisplayName);
    const FText Reason = V.bAvailable ? FText::GetEmpty() : HSRBattleText::FailureReason(V.DisabledReason);
    Detail->SetText(Reason.IsEmpty() ? V.BuildCostText()
        : FText::Format(NSLOCTEXT("HSRBattle", "UnavailableSkillCard", "{0}\n{1}"), V.BuildCostText(), Reason));
    Detail->SetColorAndOpacity(FSlateColor(V.bAvailable ? FLinearColor(.86f,.91f,1.f) : FLinearColor(1.f,.72f,.48f)));
    SetToolTipText(Reason.IsEmpty() ? V.Description : FText::Format(
        NSLOCTEXT("HSRBattle", "SkillReasonTooltip", "{0}\n{1}"), V.Description, Reason));
    // Inspection remains available even when cost or target restrictions prevent execution.
    bCanSelect = bUnlocked;
    SetIsEnabled(true); SetSelected(bSelected);
}
void UHSRBattleEntryButton::ShowForecast(const FHSRBattleParticipantView& V,int32 Position)
{
    Label->SetText(FText::Format(NSLOCTEXT("HSRBattle","OrderCard","{0}  {1}"),FText::AsNumber(Position),V.GetDisplayLabel()));
    Portrait->SetVisibility(ESlateVisibility::Collapsed); Health->SetVisibility(ESlateVisibility::Collapsed); Energy->SetVisibility(ESlateVisibility::Collapsed);
    if (auto* Frame=Portrait->GetParent()) Frame->SetVisibility(ESlateVisibility::Collapsed);
    Detail->SetVisibility(ESlateVisibility::Collapsed); bCanSelect=false; SetIsEnabled(true);
}
void UHSRBattleEntryButton::Select()
{
    if (bCanSelect && Owner.IsValid()) { if (bSkill) Owner->SelectSkillById(Id); else Owner->SelectTarget(Id); }
}
