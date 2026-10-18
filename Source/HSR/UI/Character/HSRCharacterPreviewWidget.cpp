#include "HSRCharacterPreviewWidget.h"

#include "Preview/HSRCharacterPreviewSession.h"
#include "../../Progression/HSRCharacterProfileSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "InputCoreTypes.h"

TSharedRef<SWidget> UHSRCharacterPreviewWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("PreviewWidgetTree"));
	if (!WidgetTree->RootWidget)
	{
		auto* Root = WidgetTree->ConstructWidget<UOverlay>();
		WidgetTree->RootWidget = Root;
		PreviewImage = WidgetTree->ConstructWidget<UImage>();
		PreviewImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* ImageSlot = Root->AddChildToOverlay(PreviewImage);
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
		StatusText = WidgetTree->ConstructWidget<UTextBlock>();
		StatusText->SetAutoWrapText(true);
		StatusText->SetVisibility(ESlateVisibility::HitTestInvisible);
		FSlateFontInfo Font = StatusText->GetFont(); Font.Size = 17; StatusText->SetFont(Font);
		auto* StatusSlot = Root->AddChildToOverlay(StatusText);
		StatusSlot->SetVerticalAlignment(VAlign_Bottom);
		StatusSlot->SetHorizontalAlignment(HAlign_Center);
		StatusSlot->SetPadding(FMargin(12, 12, 12, 54));
		auto* Reset = WidgetTree->ConstructWidget<UButton>();
		auto* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(NSLOCTEXT("HSRPreview", "Reset", "重置视角"));
		Reset->SetContent(Label);
		Reset->OnClicked.AddDynamic(this, &ThisClass::ResetPreviewView);
		auto* ResetSlot = Root->AddChildToOverlay(Reset);
		ResetSlot->SetVerticalAlignment(VAlign_Bottom);
		ResetSlot->SetHorizontalAlignment(HAlign_Center);
		ResetSlot->SetPadding(FMargin(12));
	}
	return Super::RebuildWidget();
}

void UHSRCharacterPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Native UUserWidget defaults to SelfHitTestInvisible; the empty preview
	// surface must receive pointer events even when the Image ignores hit tests.
	SetVisibility(ESlateVisibility::Visible);
	if (Session) Session->Close();
	Session = NewObject<UHSRCharacterPreviewSession>(this);
	Session->OnChanged.AddDynamic(this, &ThisClass::HandlePreviewChanged);
	Session->Initialize(GetWorld(), GetGameInstance() ? GetGameInstance()->GetSubsystem<UHSRCharacterProfileSubsystem>() : nullptr);
	if (!PendingCharacterId.IsNone()) Session->RequestCharacter(PendingCharacterId);
	HandlePreviewChanged();
}

void UHSRCharacterPreviewWidget::NativeDestruct()
{
	bDragging = false;
	if (Session)
	{
		Session->OnChanged.RemoveDynamic(this, &ThisClass::HandlePreviewChanged);
		Session->Close();
		Session = nullptr;
	}
	if (PreviewImage) PreviewImage->SetBrushFromTexture(nullptr);
	DisplayMaterial = nullptr;
	LastSize = FVector2D::ZeroVector;
	Super::NativeDestruct();
}

void UHSRCharacterPreviewWidget::SetCharacter(FName CharacterId)
{
	PendingCharacterId = CharacterId;
	if (Session) Session->RequestCharacter(CharacterId);
}

void UHSRCharacterPreviewWidget::SetPreviewMaterial(UMaterialInterface* Material)
{
	if (PreviewMaterial == Material) return;
	PreviewMaterial = Material;
	DisplayMaterial = nullptr;
	HandlePreviewChanged();
}

void UHSRCharacterPreviewWidget::HandlePreviewChanged()
{
	if (!Session || !PreviewImage || !StatusText) return;
	const bool bReady = Session->GetState() == EHSRCharacterPreviewState::Ready;
	if (PreviewMaterial && !DisplayMaterial)
	{
		DisplayMaterial = UMaterialInstanceDynamic::Create(PreviewMaterial, this);
		PreviewImage->SetBrushFromMaterial(DisplayMaterial);
	}
	if (DisplayMaterial) DisplayMaterial->SetTextureParameterValue(TEXT("PreviewTexture"), Session->GetRenderTarget());
	PreviewImage->SetVisibility(bReady && DisplayMaterial ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	FText Message;
	switch (Session->GetState())
	{
	case EHSRCharacterPreviewState::Loading: Message = NSLOCTEXT("HSRPreview", "Loading", "正在加载角色…"); break;
	case EHSRCharacterPreviewState::Error: Message = Session->GetErrorText(); break;
	case EHSRCharacterPreviewState::Ready: Message = DisplayMaterial
		? NSLOCTEXT("HSRPreview", "Help", "左键拖动旋转 · 滚轮缩放")
		: NSLOCTEXT("HSRPreview", "MaterialMissing", "预览显示材质未配置"); break;
	default: break;
	}
	StatusText->SetText(Message);
}

void UHSRCharacterPreviewWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	// Layout-dependent size is only available after Slate arranges this child.
	const FVector2D Size = Geometry.GetLocalSize();
	if (Session && Size.X > 1 && Size.Y > 1 && !Size.Equals(LastSize, 1.f))
	{
		LastSize = Size;
		Session->SetViewportSize(Size);
	}
}

FReply UHSRCharacterPreviewWidget::NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && Session && Session->GetState() == EHSRCharacterPreviewState::Ready)
	{
		bDragging = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Unhandled();
}

FReply UHSRCharacterPreviewWidget::NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton && bDragging)
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply UHSRCharacterPreviewWidget::NativeOnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bDragging || !Session) return FReply::Unhandled();
	if (!Event.IsMouseButtonDown(EKeys::LeftMouseButton) || !Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
	{
		bDragging = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	const FVector2D Delta = Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()) - Geometry.AbsoluteToLocal(Event.GetLastScreenSpacePosition());
	Session->Rotate(Delta.X);
	return FReply::Handled();
}

FReply UHSRCharacterPreviewWidget::NativeOnMouseWheel(const FGeometry&, const FPointerEvent& Event)
{
	if (!Session || Session->GetState() != EHSRCharacterPreviewState::Ready) return FReply::Unhandled();
	Session->Zoom(Event.GetWheelDelta());
	return FReply::Handled();
}

void UHSRCharacterPreviewWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& Event)
{
	bDragging = false;
	Super::NativeOnMouseCaptureLost(Event);
}

void UHSRCharacterPreviewWidget::ResetPreviewView()
{
	bDragging = false;
	if (Session) Session->ResetView();
}
