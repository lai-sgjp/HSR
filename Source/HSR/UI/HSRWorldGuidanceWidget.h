#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Map/HSRWorldMarkers.h"
#include "HSRWorldGuidanceWidget.generated.h"

UCLASS()
class HSR_API UHSRWorldGuidanceWidget : public UUserWidget
{
	GENERATED_BODY()
protected:
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Clip, FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bEnabled) const override;
private:
	TArray<FHSRWorldMarker> Markers;
	float RefreshElapsed = 1.f;
};
