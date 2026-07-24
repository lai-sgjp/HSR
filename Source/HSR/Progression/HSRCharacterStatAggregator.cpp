#include "HSRCharacterStatAggregator.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
bool UHSRCharacterStatAggregator::BuildContext(const UHSRCharacterDefinition* D, const FHSRCharacterRuntimeState& R, int64 Revision, FHSRCharacterProgressionContext& Out)
{
	if (!D || D->CharacterId.IsNone() || R.CharacterId != D->CharacterId || R.Level < 1 || R.Level > D->MaxLevel || Revision < 0) return false;
	const float Values[] = {D->BaseMaxHealth,D->BaseAttack,D->BaseDefense,D->BaseSpeed,D->MaxHealthPerLevel,D->AttackPerLevel,D->DefensePerLevel,D->SpeedPerLevel};
	for (float V : Values) if (!FMath::IsFinite(V) || V < 0.0f) return false;
	if (D->BaseSpeed <= 0.0f) return false;
	const float L = static_cast<float>(R.Level - 1);
	FHSRCharacterProgressionContext C; C.CharacterId=R.CharacterId; C.RuntimeRevision=Revision;
	C.ProgressionBonuses.MaxHealth=FMath::Clamp(D->MaxHealthPerLevel*L,0.0f,MAX_flt); C.ProgressionBonuses.Attack=FMath::Clamp(D->AttackPerLevel*L,0.0f,MAX_flt); C.ProgressionBonuses.Defense=FMath::Clamp(D->DefensePerLevel*L,0.0f,MAX_flt); C.ProgressionBonuses.Speed=FMath::Clamp(D->SpeedPerLevel*L,0.0f,MAX_flt);
	C.DerivedStats.MaxHealth=FMath::Clamp(D->BaseMaxHealth+C.ProgressionBonuses.MaxHealth,0.0f,MAX_flt); C.DerivedStats.Attack=FMath::Clamp(D->BaseAttack+C.ProgressionBonuses.Attack,0.0f,MAX_flt); C.DerivedStats.Defense=FMath::Clamp(D->BaseDefense+C.ProgressionBonuses.Defense,0.0f,MAX_flt); C.DerivedStats.Speed=FMath::Clamp(D->BaseSpeed+C.ProgressionBonuses.Speed,UE_SMALL_NUMBER,MAX_flt);
	if (!FMath::IsFinite(C.DerivedStats.MaxHealth)||!FMath::IsFinite(C.DerivedStats.Attack)||!FMath::IsFinite(C.DerivedStats.Defense)||!FMath::IsFinite(C.DerivedStats.Speed)) return false;
	Out=C; return true;
}
