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
class UHSREquipmentEffectBridge;
class AController;
class AHSRGameModeBase;

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
	bool HasAppliedInitialAttributes() const { return bInitialAttributesApplied; }
	FName GetProjectedCharacterId() const { return ProjectedCharacterId; }

#if WITH_DEV_AUTOMATION_TESTS
	void ProjectEquipmentForAutomation(FName InCharacterId) { SetProjectedCharacterId(InCharacterId); }
#endif

	// Development-only Phase 2 test interfaces
	UFUNCTION(BlueprintCallable, Category = "GAS|Development", meta = (DevelopmentOnly))
	bool RequestApplyPhase2TestEffect(TSubclassOf<UGameplayEffect> TestEffect);

	UFUNCTION(BlueprintCallable, Category = "GAS|Development", meta = (DevelopmentOnly))
	bool RequestPhase2Repossess();

protected:
	void InitializeAbilityActorInfo();
	void ApplyInitialAttributes();
	void BindAttributeDelegates();
	/** Projects the character's authored equipment loadout onto this ASC so exploration displays
	 * the same derived stats as the Character detail screen. No-op until ProjectedCharacterId is set. */
	void ProjectEquipmentToAbilitySystem();
	void UnprojectEquipmentFromAbilitySystem();
	void HandleEquipmentLoadoutChanged(const FGuid& CharacterId, int32 Revision);
	/** Sets ASC base stats from the authored character definition (BaseMaxHealth etc.) so the
	 * exploration panel and the Character detail screen share one source of truth. */
	void ApplyCharacterBaseStatsToAbilitySystem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<const UHSRCoreAttributeSet> CoreAttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TSubclassOf<UGameplayEffect> InitialAttributesEffect;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "GAS", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRAttributeViewModel> AttributeViewModel;

	UPROPERTY(VisibleInstanceOnly, Category = "GAS")
	bool bActorInfoInitialized;
	UPROPERTY(VisibleInstanceOnly, Category = "GAS")
	bool bInitialAttributesApplied;
	UPROPERTY(VisibleInstanceOnly, Category = "GAS")
	bool bAttributeDelegatesBound;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "GAS")
	int32 InitialAttributesApplySuccessCount;

private:
	friend class AHSRGameModeBase;
	bool SetProjectedCharacterId(FName CharacterId);

	UPROPERTY(Transient)
	TObjectPtr<UHSREquipmentEffectBridge> EquipmentEffectBridge;
	FDelegateHandle EquipmentLoadoutChangedHandle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Character|Identity",
		meta = (AllowPrivateAccess = "true"))
	FName ProjectedCharacterId;
};
