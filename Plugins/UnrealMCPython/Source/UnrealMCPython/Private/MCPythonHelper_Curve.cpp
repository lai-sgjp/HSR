#include "MCPythonHelper.h"
#include "Curves/CurveFloat.h"
bool UMCPythonHelper::SetFloatCurveKeys(UCurveFloat* Curve,const TArray<float>& Times,const TArray<float>& Values)
{
    if (!Curve || Times.Num()!=Values.Num() || Times.IsEmpty()) return false;
    for (int32 I=0; I<Times.Num(); ++I)
        if (!FMath::IsFinite(Times[I]) || !FMath::IsFinite(Values[I]) || (I>0 && Times[I]<=Times[I-1])) return false;
    Curve->Modify();
    Curve->FloatCurve.Reset();
    for (int32 I=0; I<Times.Num(); ++I)
    {
        const FKeyHandle Key=Curve->FloatCurve.AddKey(Times[I],Values[I]);
        Curve->FloatCurve.SetKeyInterpMode(Key,RCIM_Linear);
    }
    Curve->MarkPackageDirty();
    return true;
}
