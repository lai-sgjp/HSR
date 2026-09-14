#include "HSRBattleAttributeInitialization.h"
#include "HSRBattleStage.h"
#include "../Data/Definitions/HSRCharacterDefinition.h"
#include "../Data/Definitions/HSREnemyDefinition.h"
#include "../GAS/Attribute/HSRCoreAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

FHSRBattleBaseAttributes FHSRBattleBaseAttributes::FromCharacter(const UHSRCharacterDefinition& Definition)
{
	return {Definition.BaseMaxHealth, Definition.BaseMaxEnergy, Definition.BaseAttack, Definition.BaseDefense, Definition.BaseSpeed};
}

FHSRBattleBaseAttributes FHSRBattleBaseAttributes::FromEnemy(const UHSREnemyDefinition& Definition, float ExistingMaxEnergy)
{
	return {Definition.BaseMaxHealth, ExistingMaxEnergy, Definition.BaseAttack, Definition.BaseDefense, Definition.BaseSpeed};
}

bool FHSRBattleBaseAttributes::IsFormalArena(const UWorld* World)
{
	if (!World) return false;
	const FString Package = World->GetOutermost()->GetName();
	if (!Package.StartsWith(TEXT("/Game/Maps/VerticalSlice/")) || !Package.EndsWith(TEXT("Map_HertaSupportSection"))) return false;
	for (TActorIterator<AHSRBattleStage> It(const_cast<UWorld*>(World)); It; ++It) return true;
	return false;
}

bool FHSRBattleBaseAttributes::Apply(UAbilitySystemComponent& ASC) const
{
	if (!ASC.GetSet<UHSRCoreAttributeSet>()) return false;
	for (float Value : {MaxHealth, MaxEnergy, Attack, Defense, Speed})
		if (!FMath::IsFinite(Value) || Value < 0.f) return false;
	if (MaxHealth <= 0.f || Speed <= 0.f) return false;

	// Assigning the base is idempotent and does not absorb current growth/equipment bonuses.
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxHealthAttribute(), MaxHealth);
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetMaxEnergyAttribute(), MaxEnergy);
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetAttackAttribute(), Attack);
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetDefenseAttribute(), Defense);
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetSpeedAttribute(), Speed);
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetEnergyAttribute(),
		FMath::Clamp(ASC.GetNumericAttribute(UHSRCoreAttributeSet::GetEnergyAttribute()), 0.f,
			ASC.GetNumericAttribute(UHSRCoreAttributeSet::GetMaxEnergyAttribute())));
	return true;
}

void FHSRBattleBaseAttributes::FillHealth(UAbilitySystemComponent& ASC)
{
	ASC.SetNumericAttributeBase(UHSRCoreAttributeSet::GetHealthAttribute(),
		ASC.GetNumericAttribute(UHSRCoreAttributeSet::GetMaxHealthAttribute()));
}
