#include "HSREquipmentDetailWidget.h"
#include "HSREquipmentDetailViewModel.h"
void UHSREquipmentDetailWidget::NativeConstruct(){Super::NativeConstruct(); if(ViewModel) HandleId=ViewModel->OnChanged().AddUObject(this,&ThisClass::Handle);}
void UHSREquipmentDetailWidget::NativeDestruct(){if(ViewModel) ViewModel->OnChanged().Remove(HandleId); Super::NativeDestruct();}
void UHSREquipmentDetailWidget::SetViewModel(UHSREquipmentDetailViewModel* In){if(ViewModel)ViewModel->OnChanged().Remove(HandleId);ViewModel=In;if(ViewModel&&IsConstructed())HandleId=ViewModel->OnChanged().AddUObject(this,&ThisClass::Handle);}
void UHSREquipmentDetailWidget::Handle(const FHSREquipmentDetailSnapshot& In){Current=In;bHas=true;OnDetailSnapshotChanged(In);}
