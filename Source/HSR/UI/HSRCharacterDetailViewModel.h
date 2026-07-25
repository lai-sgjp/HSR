#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "HSRCharacterDetailTypes.h"
#include "HSRCharacterDetailViewModel.generated.h"
class UHSRCharacterProfileSubsystem; class UHSRSaveSubsystem; class UHSRPartySubsystem; struct FHSRRestoreCommitInfo;
UCLASS(BlueprintType)
class HSR_API UHSRCharacterDetailViewModel:public UObject { GENERATED_BODY()
public:
	/** Call from the widget after obtaining the three GameInstance subsystems. Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable,Category="HSR|Character Detail") void Initialize(UHSRCharacterProfileSubsystem* InProfiles,UHSRSaveSubsystem* InSave,UHSRPartySubsystem* InParty=nullptr);
	UFUNCTION(BlueprintCallable,Category="HSR|Character Detail") void Uninitialize();
	UFUNCTION(BlueprintCallable,Category="HSR|Character Detail") EHSRCharacterDetailResult SelectCharacter(FName CharacterId);
	UFUNCTION(BlueprintCallable,Category="HSR|Character Detail") EHSRCharacterDetailResult SelectPartySlot0();
	UFUNCTION(BlueprintCallable,Category="HSR|Character Detail") bool GetSnapshot(FHSRCharacterDetailSnapshot& Out) const { if(!bHasSnapshot)return false;Out=Snapshot;return true; }
	FHSRCharacterDetailChanged& OnChanged(){return Changed;}
	UPROPERTY(BlueprintAssignable,Category="HSR|Character Detail") FHSRCharacterDetailBlueprintChanged OnSnapshotChanged;
private:
	EHSRCharacterDetailResult BuildSnapshot(FName CharacterId,FHSRCharacterDetailSnapshot& Out) const; void BroadcastSnapshot(); void RefreshSelected(); void HandleProfile(FName Id,int64 Revision); void HandleRestore(const FHSRRestoreCommitInfo& Info); void HandleParty(int64 Revision);
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;TWeakObjectPtr<UHSRSaveSubsystem> Save;TWeakObjectPtr<UHSRPartySubsystem> Party;FDelegateHandle ProfileHandle,RestoreHandle,PartyHandle;FName SelectedId;FHSRCharacterDetailSnapshot Snapshot;bool bHasSnapshot=false;FHSRCharacterDetailChanged Changed;
};
