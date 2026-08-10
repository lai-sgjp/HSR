#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HSRRelicEquipmentTypes.h"
#include "HSRRelicEquipmentWidget.generated.h"

class UHSREquipmentEnhancementCatalog;
class UHSRItemEquipmentMappingCatalog;
class UHSRRelicEquipmentViewModel;
class UButton;

/** Forwards a candidate/option button click back to the relic widget. Dynamic UMG delegates
 *  cannot bind lambdas with captures, so each row gets a small bridge carrying its identity. */
UCLASS()
class HSR_API UHSRRelicListClickBridge : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHSRRelicEquipmentWidget* InOwner, FGuid InInstanceId, int32 InTargetLevel);

	UFUNCTION()
	void HandleClicked();

private:
	TWeakObjectPtr<UHSRRelicEquipmentWidget> Owner;
	FGuid InstanceId;
	int32 TargetLevel = -1;
};

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

	/** Bounds-checked option access. Blueprints must use this instead of indexing
	 *  EnhancementOptions, which logs an out-of-bounds access on empty slots. */
	UFUNCTION(BlueprintPure, Category = "HSR|Relic Equipment")
	bool GetEnhancementOption(int32 Index, FHSRRelicEnhancementOption& OutOption) const;

	UFUNCTION(BlueprintPure, Category = "HSR|Relic Equipment")
	int32 GetEnhancementOptionCount() const;

	UFUNCTION(BlueprintPure, Category = "HSR|Relic Equipment")
	bool HasEnhancementOptions() const;

	static bool ShouldShowSlotAndCandidateLists(EHSRRelicEquipmentStage Stage)
	{
		return Stage == EHSRRelicEquipmentStage::SlotSelection
			|| Stage == EHSRRelicEquipmentStage::CandidateSelection;
	}
	static bool ShouldShowEnhancementOptions(EHSRRelicEquipmentStage Stage)
	{
		return Stage == EHSRRelicEquipmentStage::Enhancement;
	}

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
	void UpdateStatusText(const FHSRRelicEquipmentSnapshot& InSnapshot);
	void ShowOperationResult(EHSRRelicEquipmentResult Result);
	/** C++-driven list population so the relic panel does not depend on fragile BP graph loops. */
	void PopulateCandidates();
	void PopulateEnhancementOptions();
	void ApplyStageVisibility();
	UButton* MakeListButton(const FText& Label, const FLinearColor& Color);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> ListBindings;

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
