// HSRProgressionGameplayTags.cpp
//
// 全局 GameplayTag 的唯一定义点。UE 的宏 UE_DEFINE_GAMEPLAY_TAG 会在链接期把
// 声明的 FGameplayTag 常量绑定到对应名称的全局标签上，因此这些标签必须恰好定义一次
// （不能放到头文件里，否则多个 TU 会重复定义）。
//
// 这里定义了两组语义完全对称的标签：
//   - HSRProgressionTags::Bonus*   —— 角色成长（Progression）系统产出的属性加成，
//                                     由升级/突破带来的基础属性增长组成。
//   - HSREquipmentTags::Bonus*     —— 装备（Equipment）系统产出的属性加成，
//                                     由武器/遗器及其套装效果提供。
// 两者都用于 GAS 的 SetByCaller 机制：UHSREquipmentEffectBridge / 其他 Effect 桥接层
// 通过 SetSetByCallerMagnitude(标签, 数值) 把这些加成写进 GameplayEffect，供
// 属性集（UHSRCoreAttributeSet）的修饰符读取。
#include "HSRProgressionGameplayTags.h"

// 成长系统属性加成标签：MaxHealth / Attack / Defense / Speed。
namespace HSRProgressionTags
{
	UE_DEFINE_GAMEPLAY_TAG(BonusMaxHealth, "Progression.Bonus.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(BonusAttack, "Progression.Bonus.Attack");
	UE_DEFINE_GAMEPLAY_TAG(BonusDefense, "Progression.Bonus.Defense");
	UE_DEFINE_GAMEPLAY_TAG(BonusSpeed, "Progression.Bonus.Speed");
}

// 装备系统属性加成标签：MaxHealth / Attack / Defense / Speed。
namespace HSREquipmentTags
{
	UE_DEFINE_GAMEPLAY_TAG(BonusMaxHealth, "Equipment.Bonus.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(BonusAttack, "Equipment.Bonus.Attack");
	UE_DEFINE_GAMEPLAY_TAG(BonusDefense, "Equipment.Bonus.Defense");
	UE_DEFINE_GAMEPLAY_TAG(BonusSpeed, "Equipment.Bonus.Speed");
}
