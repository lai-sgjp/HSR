#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "HSRCoreAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

UCLASS()
class HSR_API UHSRCoreAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UHSRCoreAttributeSet();

	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, Health);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, MaxHealth);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, Energy);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, MaxEnergy);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, Speed);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, Attack);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, Defense);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, CritRate);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, CritDamage);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, Toughness);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, MaxToughness);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, IncomingToughnessDamage);
	ATTRIBUTE_ACCESSORS(UHSRCoreAttributeSet, IncomingDamage);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Health;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Energy;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData MaxEnergy;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes")
	FGameplayAttributeData Speed;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Attack;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData Defense;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData CritRate;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Combat")
	FGameplayAttributeData CritDamage;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Toughness")
	FGameplayAttributeData Toughness;
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Toughness")
	FGameplayAttributeData MaxToughness;
	/** P8 meta attribute; consumed immediately and never changes Health. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingToughnessDamage;
	/** Meta attribute: written only by P7 damage executions and consumed immediately. */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Meta")
	FGameplayAttributeData IncomingDamage;
};
