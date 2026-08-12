#include "HSREquipmentDetailWidget.h"
#include "HSREquipmentDetailViewModel.h"

// 构造完成：订阅 ViewModel 的变化事件。
void UHSREquipmentDetailWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (ViewModel)
	{
		HandleId = ViewModel->OnChanged().AddUObject(this, &ThisClass::Handle);
	}
}

// 析构：取消订阅。
void UHSREquipmentDetailWidget::NativeDestruct()
{
	if (ViewModel)
	{
		ViewModel->OnChanged().Remove(HandleId);
	}
	Super::NativeDestruct();
}

// 换绑 ViewModel：先解除旧订阅，再绑定新订阅（仅在已构造时绑定）。
void UHSREquipmentDetailWidget::SetViewModel(UHSREquipmentDetailViewModel* In)
{
	if (ViewModel)
	{
		ViewModel->OnChanged().Remove(HandleId);
	}
	ViewModel = In;
	if (ViewModel && IsConstructed())
	{
		HandleId = ViewModel->OnChanged().AddUObject(this, &ThisClass::Handle);
	}
}

// 快照变化回调：缓存最新快照并通知子类刷新展示。
void UHSREquipmentDetailWidget::Handle(const FHSREquipmentDetailSnapshot& In)
{
	Current = In;
	bHas = true;
	OnDetailSnapshotChanged(In);
}
