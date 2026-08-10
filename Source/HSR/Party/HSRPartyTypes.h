#pragma once
#include "CoreMinimal.h"
#include "HSRPartyTypes.generated.h"

/**
 * Party width, owned here because it is a party-domain concept that the save layer merely persists.
 * It stays compile-time on purpose: the save encoder treats slot width as part of the canonical
 * format and rejects any payload that disagrees, so a runtime-configurable width would make the
 * on-disk format ambiguous. Changing this value is a schema change and needs a migration step.
 */
constexpr int32 HSRPartyCapacity = 4;

UENUM(BlueprintType)
enum class EHSRPartyResult : uint8 { Success, InvalidSlot, ProfileNotFound, DuplicateCharacter, EmptySlot, Full, InvalidCandidate, RevisionConflict };

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
