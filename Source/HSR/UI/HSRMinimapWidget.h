#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../Map/HSRWorldMarkers.h"
#include "HSRMinimapWidget.generated.h"

/** Full current-area quest and unclaimed-chest projection, shared with world guidance. */
UCLASS()
class HSR_API UHSRMinimapWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual int32 NativePaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Clip,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool bEnabled) const override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    void RefreshPosition();
    FTimerHandle RefreshTimer;
    TArray<FHSRWorldMarker> Markers;
};
