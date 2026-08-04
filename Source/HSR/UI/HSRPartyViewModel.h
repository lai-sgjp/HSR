#pragma once

#include "CoreMinimal.h"
#include "HSRPartyViewModel.generated.h"

class UHSRPartySubsystem;

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
	UPROPERTY(BlueprintReadOnly, Category = "HSR|Party") int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRPartyFrontendChanged, const FHSRPartyFrontendSnapshot&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHSRPartyFrontendBlueprintChanged, const FHSRPartyFrontendSnapshot&, Snapshot);

UCLASS(BlueprintType)
class HSR_API UHSRPartyViewModel : public UObject
{
	GENERATED_BODY()

public:
	virtual void BeginDestroy() override;
	void Initialize(UHSRPartySubsystem* InParty);
	void Shutdown();

	UFUNCTION(BlueprintPure, Category = "HSR|Party")
	bool GetSnapshot(FHSRPartyFrontendSnapshot& OutSnapshot) const;

	FHSRPartyFrontendChanged& OnChanged() { return Changed; }
	UPROPERTY(BlueprintAssignable, Category = "HSR|Party")
	FHSRPartyFrontendBlueprintChanged OnSnapshotChanged;

private:
	void HandlePartyChanged(int64);
	void Rebuild();

	TWeakObjectPtr<UHSRPartySubsystem> Party;
	FDelegateHandle PartyChangedHandle;
	FHSRPartyFrontendSnapshot Snapshot;
	FHSRPartyFrontendChanged Changed;
};
