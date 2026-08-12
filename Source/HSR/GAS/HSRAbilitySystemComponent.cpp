#include "HSRAbilitySystemComponent.h"

// 项目自定义的 AbilitySystemComponent 子类。
// 目前只需关闭 Tick（战斗数值完全由事件驱动，不需要每帧轮询）。
UHSRAbilitySystemComponent::UHSRAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
