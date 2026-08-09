#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRCharacterShellTypes.h"
#include "HSRCharacterShellViewModel.generated.h"

class UHSRCharacterDetailViewModel;
class UHSRCharacterProfileSubsystem;
class UHSREquipmentDetailViewModel;
class UHSREquipmentSubsystem;
class UHSRPartySubsystem;
class UHSRSaveSubsystem;

UCLASS(BlueprintType)
class HSR_API UHSRCharacterShellViewModel : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(UHSRCharacterProfileSubsystem* InProfiles, UHSRSaveSubsystem* InSave,
		UHSRPartySubsystem* InParty, UHSREquipmentSubsystem* InEquipment);
	void Uninitialize();

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult Refresh();

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult SelectCharacter(FName CharacterId);

	UFUNCTION(BlueprintCallable, Category = "HSR|Character Shell")
	EHSRCharacterShellResult SelectTab(EHSRCharacterShellTab Tab);

	UFUNCTION(BlueprintPure, Category = "HSR|Character Shell")
	bool GetSnapshot(FHSRCharacterShellSnapshot& OutSnapshot) const
	{
		if (!bHasSnapshot) return false;
		OutSnapshot = Snapshot;
		return true;
	}

	FHSRCharacterShellChanged& OnChanged() { return Changed; }
	UPROPERTY(BlueprintAssignable, Category = "HSR|Character Shell")
	FHSRCharacterShellBlueprintChanged OnSnapshotChanged;

private:
	bool BuildEntries(TArray<FHSRCharacterShellEntrySnapshot>& OutEntries) const;
	bool ContainsCharacter(const TArray<FHSRCharacterShellEntrySnapshot>& Entries, FName CharacterId) const;
	FName SelectInitialCharacter(const TArray<FHSRCharacterShellEntrySnapshot>& Entries) const;
	EHSRCharacterShellResult RebuildSelected(const TArray<FHSRCharacterShellEntrySnapshot>& Entries);
	EHSRCharacterShellResult PublishFailure(EHSRCharacterShellResult Result);
	void UpdateSelectedTabState();
	void Broadcast();
	void HandleProfileChanged(FName CharacterId, int64 Revision);
	void HandleCharacterDetailChanged(const FHSRCharacterDetailSnapshot& InSnapshot);
	void HandleEquipmentDetailChanged(const FHSREquipmentDetailSnapshot& InSnapshot);
	static EHSRCharacterShellResult MapCharacterResult(EHSRCharacterDetailResult Result);

	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	TWeakObjectPtr<UHSRSaveSubsystem> Save;
	TWeakObjectPtr<UHSRPartySubsystem> Party;
	TWeakObjectPtr<UHSREquipmentSubsystem> Equipment;

	UPROPERTY(Transient) TObjectPtr<UHSRCharacterDetailViewModel> CharacterDetailViewModel;
	UPROPERTY(Transient) TObjectPtr<UHSREquipmentDetailViewModel> EquipmentDetailViewModel;

	FDelegateHandle ProfileHandle;
	FDelegateHandle CharacterDetailHandle;
	FDelegateHandle EquipmentDetailHandle;
	FName SelectedCharacterId;
	EHSRCharacterShellTab SelectedTab = EHSRCharacterShellTab::Detail;
	FHSRCharacterShellSnapshot Snapshot;
	bool bHasSnapshot = false;
	bool bUpdating = false;
	FHSRCharacterShellChanged Changed;
};
