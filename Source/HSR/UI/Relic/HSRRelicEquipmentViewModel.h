#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRRelicEquipmentTypes.h"
#include "../../Inventory/HSRItemTypes.h"
#include "HSRRelicEquipmentViewModel.generated.h"

class UHSREquipmentEnhancementCatalog;
class UHSREquipmentSubsystem;
class UHSRInventorySubsystem;
class UHSRItemEquipmentMappingCatalog;

UCLASS(BlueprintType)
class HSR_API UHSRRelicEquipmentViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;

	void Initialize(UHSREquipmentSubsystem* InEquipment, UHSRInventorySubsystem* InInventory,
		UHSRItemEquipmentMappingCatalog* InMappingCatalog,
		UHSREquipmentEnhancementCatalog* InEnhancementCatalog, const FGuid& InCharacterId);
	void Shutdown();

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	EHSRRelicEquipmentResult SelectSlot(EHSRRelicSlot InSlot);

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	EHSRRelicEquipmentResult SelectCandidate(const FGuid& InInstanceId);

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	EHSRRelicEquipmentResult OpenEnhancement();

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	EHSRRelicEquipmentResult CommitSelectedMovement();

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	EHSRRelicEquipmentResult CommitEnhancement(int32 TargetLevel);

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	EHSRRelicEquipmentResult Back();

	UFUNCTION(BlueprintPure, Category = "HSR|Relic Equipment")
	bool GetSnapshot(FHSRRelicEquipmentSnapshot& OutSnapshot) const
	{
		if (!bHasSnapshot) return false;
		OutSnapshot = Snapshot;
		return true;
	}

	FHSRRelicEquipmentChanged& OnChanged() { return Changed; }

	UPROPERTY(BlueprintAssignable, Category = "HSR|Relic Equipment")
	FHSRRelicEquipmentBlueprintChanged OnSnapshotChanged;

private:
	void Rebuild();
	void Broadcast();
	void PublishFailure(EHSRRelicEquipmentResult Result);
	bool IsInitialized() const;
	bool IsValidRelicSlot(EHSRRelicSlot InSlot) const;
	bool BuildSlotRows(FHSREquipmentLoadout& OutLoadout);
	void BuildCandidateRows(const FHSREquipmentLoadout& Loadout);
	bool BuildComparison();
	bool BuildEnhancementOptions();
	int32 FindStackQuantity(const FHSRInventorySnapshot& InventorySnapshot, FName ItemId) const;
	int32 GetHeldMaterialQuantity(FName ItemId) const;
	void HandleEquipmentChanged(const FGuid& CharacterId, int32 Revision);
	void HandleInventoryChanged(int64 Revision);
	static float GetStatValue(const FHSREquipmentInstance& Instance, EHSREquipmentStat Stat);
	static EHSRRelicEquipmentResult MapMovementResult(EHSREquipmentMovementResultCode Code);
	static EHSRRelicEquipmentResult MapEnhancementResult(EHSREquipmentEnhancementResultCode Code);

	TWeakObjectPtr<UHSREquipmentSubsystem> Equipment;
	TWeakObjectPtr<UHSRInventorySubsystem> Inventory;
	TWeakObjectPtr<UHSRItemEquipmentMappingCatalog> MappingCatalog;
	TWeakObjectPtr<UHSREquipmentEnhancementCatalog> EnhancementCatalog;
	FDelegateHandle EquipmentHandle;
	FDelegateHandle InventoryHandle;
	FGuid CharacterId;
	EHSRRelicEquipmentStage Stage = EHSRRelicEquipmentStage::SlotSelection;
	EHSRRelicSlot SelectedSlot = EHSRRelicSlot::Head;
	FGuid SelectedCandidateId;
	FHSRRelicEquipmentSnapshot Snapshot;
	bool bHasSnapshot = false;
	FHSRRelicEquipmentChanged Changed;
};
