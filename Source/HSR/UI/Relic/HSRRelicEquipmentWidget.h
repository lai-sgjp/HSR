#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRRelicEquipmentTypes.h"
#include "HSRRelicEquipmentWidget.generated.h"

class UHSREquipmentEnhancementCatalog;
class UHSRItemEquipmentMappingCatalog;
class UHSRRelicEquipmentViewModel;

UCLASS(Blueprintable)
class HSR_API UHSRRelicEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	void InitializeForCharacter(const FGuid& InCharacterId,
		UHSRItemEquipmentMappingCatalog* InMappingCatalog = nullptr,
		UHSREquipmentEnhancementCatalog* InEnhancementCatalog = nullptr);

	UFUNCTION(BlueprintCallable, Category = "HSR|Relic Equipment")
	void InitializeForCharacterProfile(FName CharacterProfileId);

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
	bool GetCurrentSnapshot(FHSRRelicEquipmentSnapshot& OutSnapshot) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Relic Equipment")
	void OnRelicSnapshotChanged(const FHSRRelicEquipmentSnapshot& Snapshot);

	UFUNCTION(BlueprintImplementableEvent, Category = "HSR|Relic Equipment")
	void OnRelicUnavailable(EHSRRelicEquipmentResult Result);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void InitializeRuntimeContext();
	void HandleSnapshot(const FHSRRelicEquipmentSnapshot& InSnapshot);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HSR|Relic Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSRItemEquipmentMappingCatalog> MappingCatalog;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "HSR|Relic Equipment", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHSREquipmentEnhancementCatalog> EnhancementCatalog;

	UPROPERTY(Transient)
	TObjectPtr<UHSRRelicEquipmentViewModel> ViewModel;

	FGuid CharacterId;
	FDelegateHandle SnapshotHandle;
	FHSRRelicEquipmentSnapshot CurrentSnapshot;
	bool bHasSnapshot = false;
};
