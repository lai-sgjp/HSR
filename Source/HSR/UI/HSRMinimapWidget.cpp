#include "HSRMinimapWidget.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "../Exploration/HSRSceneInteraction.h"

void UHSRMinimapWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshPosition();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(RefreshTimer,this,&ThisClass::RefreshPosition,.2f,true);
}
void UHSRMinimapWidget::NativeDestruct()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimer);
    Super::NativeDestruct();
}
void UHSRMinimapWidget::RefreshPosition()
{
    Markers = HSRWorldMarkers::Collect(GetWorld());
    InvalidateLayoutAndVolatility();
}
int32 UHSRMinimapWidget::NativePaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& E,int32 L,const FWidgetStyle& S,bool Enabled) const
{
    L=Super::NativePaint(Args,G,Clip,E,L,S,Enabled);
    const FVector2D Size=G.GetLocalSize(), C=Size*.5;
    FSlateDrawElement::MakeBox(E,++L,G.ToPaintGeometry(),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(.015,.03,.055,.92)*S.GetColorAndOpacityTint());
    const bool Hub=GetWorld() && GetWorld()->GetMapName().Contains(TEXT("ObservationCar"));
    const auto Line=[&](TArray<FVector2D> Points,FLinearColor Color,float Width)
    { FSlateDrawElement::MakeLines(E,L+1,G.ToPaintGeometry(),Points,ESlateDrawEffect::None,Color*S.GetColorAndOpacityTint(),true,Width); };
    const FLinearColor Road(.35,.43,.53,1);
    const float WorldExtent=Hub ? 3800.f : 13000.f;
    const float Scale=FMath::Min(Size.X,Size.Y)*.45f/WorldExtent;
    const auto Point=[&](float X,float Y) {return C+FVector2D(X,-Y)*Scale;};
    const auto Rect=[&](float X,float Y,float W,float H,FLinearColor Color)
    {Line({Point(X-W/2,Y-H/2),Point(X+W/2,Y-H/2),Point(X+W/2,Y+H/2),Point(X-W/2,Y+H/2),Point(X-W/2,Y-H/2)},Color,2);};
    if (Hub)
    {
        Rect(0,0,7000,2500,Road);Rect(500,300,5400,1600,Road);
        Line({Point(-3300,-800),Point(3300,-800)},Road,3);
    }
    if (!Hub)
    {
        Rect(0,0,25000,25000,Road);
        Line({Point(0,-12000),Point(0,6900)},Road,5);
        Line({Point(-12000,0),Point(12000,0)},Road,5);
        for (float X : {-6000.f,6000.f}) for(float Y : {-5700.f,5700.f})
            Rect(X>0 && Y>0 ? 8300.f : X,Y,3800,3200,Road);
        Rect(0,8300,9200,2800,FLinearColor(.6,.53,.37));
        for(float X : {-3800.f,3800.f}) Line({Point(X,3900),Point(X,6900)},Road,3);
        Line({Point(5000,0),Point(5000,7200),Point(4000,7200)},Road,3);
        TArray<FVector2D> Circle;
        for (int32 I=0;I<=32;++I) Circle.Add(C+FVector2D(FMath::Cos(I*2*PI/32),FMath::Sin(I*2*PI/32))*2750*Scale);
        Line(Circle,FLinearColor(.68,.49,.2),2);
    }
    for(const FHSRWorldMarker& Mark : Markers)
    {
        const FVector2D P=Point(Mark.Location.X,Mark.Location.Y);
        const FLinearColor Color = Mark.bQuest ? FLinearColor(1,.75,.2) : FLinearColor(.3,.85,.85);
        Line({P+FVector2D(0,-5),P+FVector2D(5,0),P+FVector2D(0,5),P-FVector2D(5,0),P-FVector2D(0,5)},Color,2);
        if (Size.X > 350)
        {
            const APawn* Pawn = GetOwningPlayerPawn();
            const FText Label = Pawn ? FText::Format(NSLOCTEXT("HSRMarkers", "MapDistance", "{0} · {1} m"), Mark.Label,
                FText::AsNumber(FMath::RoundToInt(FVector::Distance(Pawn->GetActorLocation(), Mark.Location)/100.f))) : Mark.Label;
            FSlateDrawElement::MakeText(E,L+2,G.ToPaintGeometry(FVector2D(220,24),FSlateLayoutTransform(P+FVector2D(8,-10))),Label,FCoreStyle::GetDefaultFontStyle("Regular",12),ESlateDrawEffect::None,Color);
        }
    }
    if (const APawn* Pawn=GetOwningPlayerPawn())
    {
        const FVector Pos=Pawn->GetActorLocation();
        const FVector2D P=Point(Pos.X,Pos.Y);
        const float Angle=-FMath::DegreesToRadians(Pawn->GetActorRotation().Yaw);
        const FVector2D Forward(FMath::Cos(Angle),FMath::Sin(Angle)),Right(-Forward.Y,Forward.X);
        Line({P+Forward*7,P-Forward*5+Right*4,P-Forward*2,P-Forward*5-Right*4,P+Forward*7},FLinearColor(.25,.9,1),2);
    }
    return L+2;
}
