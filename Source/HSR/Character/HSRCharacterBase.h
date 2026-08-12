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
	// 供 UI/蓝图直接读取 ViewModel：它封装了 ASC 属性的显示快照，UI 不应直接触碰 ASC。
	UHSRAttributeViewModel* GetAttributeViewModel() const
	{
		return AttributeViewModel;
	}
	// 初始属性 GE 是否已成功应用（HasAppliedInitialAttributes 的语义：可安全读取基础数值）。
	bool HasAppliedInitialAttributes() const
	{
		return bInitialAttributesApplied;
	}
	// 当前投影的角色 ID；未设置时为 NAME_None。
	FName GetProjectedCharacterId() const
	{
		return ProjectedCharacterId;
	}

#if WITH_DEV_AUTOMATION_TESTS
	// 仅自动化测试使用：直接投影指定角色，跳过正常入口的权限校验。
	void ProjectEquipmentForAutomation(FName InCharacterId)
	{
		SetProjectedCharacterId(InCharacterId);
	}
#endif

	// Development-only Phase 2 test interfaces
	UFUNCTION(BlueprintCallable, Category = "GAS|Development", meta = (DevelopmentOnly))
	bool RequestApplyPhase2TestEffect(TSubclassOf<UGameplayEffect> TestEffect);

	UFUNCTION(BlueprintCallable, Category = "GAS|Development", meta = (DevelopmentOnly))
	bool RequestPhase2Repossess();

protected:
	// 初始化 ASC 的 ActorInfo（Owner/Avatar 均为自身）。必须在应用任何 GE 之前完成。
	void InitializeAbilityActorInfo();
	// 应用初始属性 GE，并幂等（只执行一次）。
	void ApplyInitialAttributes();
	// 把属性委托绑定到 ViewModel，供 UI 观察属性变化。
	void BindAttributeDelegates();
	/** 把角色已配置的装备负载投影到本 ASC 上，使探索世界的表现数值与角色详情页一致。
	 *  在 ProjectedCharacterId 设置之前为空操作。 */
	void ProjectEquipmentToAbilitySystem();
	// 卸载装备投影：解绑负载变更监听并移除已应用的装备 GE。
	void UnprojectEquipmentFromAbilitySystem();
	// 负载变更回调：只处理属于本角色的变更，并重新投影。
	void HandleEquipmentLoadoutChanged(const FGuid& CharacterId, int32 Revision);
	/** 以角色定义（BaseMaxHealth 等）为唯一权威来源写入 ASC 基础数值，
	 *  使探索面板与角色详情页共享同一份数据。 */
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
	// 设置投影角色 ID 并触发基础属性断言与装备投影。仅 GameMode 等内部调用，故为私有。
	bool SetProjectedCharacterId(FName CharacterId);

	UPROPERTY(Transient)
	TObjectPtr<UHSREquipmentEffectBridge> EquipmentEffectBridge;
	FDelegateHandle EquipmentLoadoutChangedHandle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Character|Identity",
		meta = (AllowPrivateAccess = "true"))
	FName ProjectedCharacterId;
};
