#pragma once
#include "CoreMinimal.h"
#include "HSRPartyTypes.generated.h"

UENUM(BlueprintType)
enum class EHSRPartyResult : uint8 { Success, InvalidSlot, ProfileNotFound, DuplicateCharacter, EmptySlot, Full, InvalidCandidate };

USTRUCT(BlueprintType)
struct HSR_API FHSRPartySlot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName CharacterId;
	bool IsEmpty() const { return CharacterId.IsNone(); }
};

USTRUCT(BlueprintType)
struct HSR_API FHSRPartySnapshot
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TArray<FHSRPartySlot> Slots;
	UPROPERTY(BlueprintReadOnly) int64 Revision = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FHSRPartyChanged, int64);
