#pragma once
#include "CoreMinimal.h"

namespace HSREnemyTargetPolicy
{
	inline float Weight(float Health, float MaxHealth, bool bElite, bool bPreviousTarget)
	{
		const float Ratio = FMath::Clamp(Health / FMath::Max(1.f, MaxHealth), 0.f, 1.f);
		return bElite ? (1.f + 2.f * (1.f - Ratio)) * (bPreviousTarget ? .5f : 1.f) : 1.f;
	}
	inline int32 Choose(const TArray<float>& Weights, FRandomStream& Random)
	{
		float Total = 0.f;
		for (float W : Weights) { if (!FMath::IsFinite(W) || W <= 0.f) return INDEX_NONE; Total += W; }
		if (Weights.IsEmpty() || !FMath::IsFinite(Total)) return INDEX_NONE;
		float Roll = Random.FRandRange(0.f, Total);
		for (int32 I = 0; I < Weights.Num(); ++I) { Roll -= Weights[I]; if (Roll <= 0.f) return I; }
		return Weights.Num()-1;
	}
}
