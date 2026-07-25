#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSREquipmentDetailTypes.h"
#include "HSREquipmentDetailWidget.generated.h"
class UHSREquipmentDetailViewModel;
UCLASS(Abstract,Blueprintable) class HSR_API UHSREquipmentDetailWidget:public UUserWidget { GENERATED_BODY() public:
 UFUNCTION(BlueprintPure) bool GetCurrentSnapshot(FHSREquipmentDetailSnapshot& Out) const {if(!bHas)return false;Out=Current;return true;}
 UFUNCTION(BlueprintCallable,Category="HSR|Equipment Detail") void SetViewModel(UHSREquipmentDetailViewModel* InViewModel);
 UFUNCTION(BlueprintImplementableEvent) void OnDetailSnapshotChanged(const FHSREquipmentDetailSnapshot& Snapshot);
protected: virtual void NativeConstruct() override; virtual void NativeDestruct() override;
private: void Handle(const FHSREquipmentDetailSnapshot& In); UPROPERTY(Transient) TObjectPtr<UHSREquipmentDetailViewModel> ViewModel; FDelegateHandle HandleId; FHSREquipmentDetailSnapshot Current; bool bHas=false;
};
