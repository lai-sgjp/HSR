#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSREquipmentDetailTypes.h"
#include "HSREquipmentDetailViewModel.generated.h"
class UHSREquipmentSubsystem;
UCLASS(BlueprintType) class HSR_API UHSREquipmentDetailViewModel:public UObject { GENERATED_BODY() public:
 virtual void BeginDestroy() override;
 void Initialize(UHSREquipmentSubsystem* InEquipment,const FGuid& InCharacterId);
 void Shutdown();
 UFUNCTION(BlueprintPure,Category="HSR|Equipment Detail") bool GetSnapshot(FHSREquipmentDetailSnapshot& Out) const { if(!bHas)return false; Out=Snapshot; return true; }
 FHSREquipmentDetailChanged& OnChanged(){return Changed;} UPROPERTY(BlueprintAssignable) FHSREquipmentDetailBlueprintChanged OnSnapshotChanged;
private: void Rebuild(const FGuid& ChangedId,int32 Revision); TWeakObjectPtr<UHSREquipmentSubsystem> Equipment; FGuid CharacterId; FDelegateHandle Subscription; FHSREquipmentDetailSnapshot Snapshot; bool bHas=false; FHSREquipmentDetailChanged Changed;
};
