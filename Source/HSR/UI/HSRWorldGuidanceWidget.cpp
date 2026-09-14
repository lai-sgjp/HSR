#include "HSRWorldGuidanceWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void UHSRWorldGuidanceWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	RefreshElapsed += DeltaTime;
	if (RefreshElapsed >= .2f) { Markers = HSRWorldMarkers::Collect(GetWorld()); RefreshElapsed = 0.f; }
}

int32 UHSRWorldGuidanceWidget::NativePaint(const FPaintArgs& Args, const FGeometry& G, const FSlateRect& Clip, FSlateWindowElementList& E, int32 L, const FWidgetStyle& Style, bool Enabled) const
{
	L = Super::NativePaint(Args, G, Clip, E, L, Style, Enabled);
	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = GetOwningPlayerPawn();
	if (!PC || !Pawn) return L;
	const FVector2D Size = G.GetLocalSize();
	TArray<FVector2D> Labels;
	for (const auto& Marker : Markers)
	{
		FVector2D P;
		const bool bOnScreen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC, Marker.Location + FVector(0,0,140), P, true);
		if (!bOnScreen)
		{
			if (!Marker.bQuest) continue;
			FVector Camera; FRotator Rotation; PC->GetPlayerViewPoint(Camera, Rotation);
			const FVector Local = Rotation.UnrotateVector(Marker.Location-Camera);
			P = Size*.5 + FVector2D(Local.Y >= 0 ? Size.X : -Size.X, -Local.Z);
		}
		P.X = FMath::Clamp(P.X, 380.f, FMath::Max(380.f, Size.X-650.f));
		P.Y = FMath::Clamp(P.Y, 340.f, FMath::Max(340.f, Size.Y-260.f));
		for (const FVector2D& Label : Labels)
			if (FMath::Abs(P.X-Label.X) < 360.f && FMath::Abs(P.Y-Label.Y) < 28.f) P.Y = Label.Y+28.f;
		Labels.Add(P);
		const FLinearColor Color = Marker.bQuest ? FLinearColor(1,.78,.25) : FLinearColor(.35,.9,.9);
		const FText Text = FText::Format(NSLOCTEXT("HSRMarkers", "World", "◇ {0}  {1} m"), Marker.Label, FText::AsNumber(FMath::RoundToInt(FVector::Distance(Pawn->GetActorLocation(),Marker.Location)/100.f)));
		FSlateDrawElement::MakeText(E,L+1,G.ToPaintGeometry(FVector2D(280,30),FSlateLayoutTransform(P)),Text,FCoreStyle::GetDefaultFontStyle("Bold",14),ESlateDrawEffect::None,Color);
	}
	return L+1;
}
