#pragma once

#include "CoreMinimal.h"
#include "../Party/HSRPartyTypes.h"
#include "HSRPartyViewModel.generated.h"

class UHSRPartySubsystem;
class UHSRCharacterProfileSubsystem;

UENUM(BlueprintType)
enum class EHSRPartyFrontendStatus : uint8
{
	Ready,
	Empty,
	Unavailable
};

USTRUCT(BlueprintType)
struct HSR_API FHSRPartySlotViewData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") int32 SlotIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") FName CharacterId;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") bool bOccupied = false;
};

USTRUCT(BlueprintType)
struct HSR_API FHSRPartyFrontendSnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") EHSRPartyFrontendStatus Status = EHSRPartyFrontendStatus::Unavailable;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") TArray<FHSRPartySlotViewData> Slots;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") TArray<FName> AvailableCharacterIds;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") int64 Revision = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") int32 ActiveSlot = 0;
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") bool bHasPendingChanges = false;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRPartyFrontendChanged, const FHSRPartyFrontendSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRPartyFrontendBlueprintChanged, const FHSRPartyFrontendSnapshot&, Snapshot);

UCLASS(BlueprintType)
class HSR_API UHSRPartyViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Initialize(UHSRPartySubsystem* InParty, UHSRCharacterProfileSubsystem* InProfiles = nullptr);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "HSR|Party")
	bool GetSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const;
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult SetCandidateSlot(int32 SlotIndex, FName CharacterId);
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult ClearCandidateSlot(int32 SlotIndex);
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult SwapCandidateSlots(int32 FirstSlot, int32 SecondSlot);
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult ConfirmCandidate();
	UFUNCTION(BlueprintCallable, Category = "HSR|Party") EHSRPartyResult CancelCandidate();

	FHSRPartyFrontendChanged& OnChanged() { return Changed; }
	UPROPERTY(BlueprintAssignable, Category = "HSR|Party")
	FHSRPartyFrontendBlueprintChanged OnSnapshotChanged;

private:
	void HandlePartyChanged(int64);
	void Rebuild();
	void ResetCandidateFromAuthority();
	void PublishCandidate();
	bool IsValidCandidateSlot(int32 SlotIndex) const;
	bool IsAvailableCharacter(FName CharacterId) const;
	bool CandidateContains(FName CharacterId, int32 IgnoreSlot = INDEX_NONE) const;

	TWeakObjectPtr<UHSRPartySubsystem> Party;
	TWeakObjectPtr<UHSRCharacterProfileSubsystem> Profiles;
	FDelegateHandle PartyChangedHandle;
	FHSRPartySnapshot Candidate;
	bool bHasPendingChanges = false;
	FHSRPartyFrontendSnapshot Snapshot;
	FHSRPartyFrontendChanged Changed;
};
