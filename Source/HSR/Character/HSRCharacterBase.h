#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "HSRCharacterBase.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UHSRAbilitySystemComponent;
class UHSRCoreAttributeSet;
class UHSRAttributeViewModel;

UCLASS(Abstract)
class HSR_API AHSRCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AHSRCharacterBase();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintPure, Category = "GAS")
	UHSRAttributeViewModel* GetAttributeViewModel() const { return AttributeViewModel; }

protected:
	void InitializeAbilityActorInfo();
	void ApplyInitialAttributes();
	void BindAttributeDelegates();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<const UHSRCoreAttributeSet> CoreAttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "GAS")
	TObjectPtr<UHSRAttributeViewModel> AttributeViewModel;

	UPROPERTY(VisibleInstanceOnly, Category = "GAS")
	bool bActorInfoInitialized;
	UPROPERTY(VisibleInstanceOnly, Category = "GAS")
	bool bInitialAttributesApplied;
	UPROPERTY(VisibleInstanceOnly, Category = "GAS")
	bool bAttributeDelegatesBound;
};
