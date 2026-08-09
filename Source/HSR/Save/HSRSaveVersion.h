#pragma once

#include "CoreMinimal.h"
#include "HSRSaveTypes.h"

/** The only stable disk contract introduced by Phase 16.  It is deliberately
 * not a UObject serialization format. */
enum class EHSRSaveDecodeResult : uint8
{
	Success, InvalidArgument, TooShort, BadMagic, UnsupportedFormat, InvalidHeader,
	InvalidSize, SlotMismatch, ChecksumMismatch, TooOld, FutureSchema, NonCanonical,
	InvalidPayload, MigrationFailed, LegacyUeSaveGame
};

struct HSR_API FHSRSaveEnvelopeHeader
{
	static constexpr uint16 FormatVersion = 1;
	static constexpr uint16 HeaderBytes = 104;
	static constexpr uint16 PayloadCodecVersion = 1;
	static constexpr int64 MaxPayloadBytes = 16ll * 1024ll * 1024ll;
	uint32 SchemaVersion = 9;
	uint32 MinimumCompatibleSchema = 1;
	FGuid SaveId;
	uint64 Generation = 1;
	int64 UtcUnixMilliseconds = 0;
	uint64 SlotIdentity = 0;
};

namespace HSRSaveVersion
{
	constexpr int32 CurrentSchema = 9;
	constexpr uint32 MaxRecordCount = 65535;
	constexpr uint32 MaxTokenBytes = 4096;
	/** Party width is part of the disk contract, so it is pinned per schema rather than
	 * tracked against the runtime capacity.  Schema 9 widened the party from 2 to 4.
	 * These stay literal historical values on purpose: a payload written at schema 9 is four
	 * slots wide forever, so they must not follow HSRPartyCapacity if the party later grows --
	 * that would need a new schema plus a migration, not a redefinition of an old one. */
	constexpr uint32 LegacyPartySlotCount = 2;
	constexpr uint32 PartySlotCount = 4;
	constexpr uint32 PartySlotCountForSchema(uint32 Schema) { return Schema >= 9 ? PartySlotCount : LegacyPartySlotCount; }

	/**
	 * The current schema must be able to hold a full runtime party.  Nothing previously tied the
	 * two constants together, so widening HSRPartyCapacity alone would have silently truncated
	 * every save at encode time.  Bumping the party width means adding a schema and a migration.
	 */
	static_assert(PartySlotCountForSchema(CurrentSchema) == static_cast<uint32>(HSRPartyCapacity),
		"Runtime party capacity changed without a save schema bump: add a new schema whose slot "
		"width matches HSRPartyCapacity and a migration that widens existing payloads.");
	bool IsValidSlot(const FString& SlotName, int32 UserIndex);
	bool IsValidPayloadSize(uint64 PayloadBytes);
	bool IsCanonicalIdToken(const FString& Token);
	uint64 MakeSlotIdentity(const FString& SlotName, int32 UserIndex);
	bool EncodeCanonicalPayload(const FHSRSaveData& Data, TArray<uint8>& OutPayload);
	EHSRSaveDecodeResult DecodeCanonicalPayload(const TArray<uint8>& Payload, FHSRSaveData& OutData);
	bool EncodeEnvelope(const FHSRSaveData& Data, const FString& SlotName, int32 UserIndex, const FGuid& SaveId, uint64 Generation, TArray<uint8>& OutBytes);
	bool EncodeEnvelopeAtUtc(const FHSRSaveData& Data, const FString& SlotName, int32 UserIndex, const FGuid& SaveId, uint64 Generation, int64 UtcUnixMilliseconds, TArray<uint8>& OutBytes);
	bool ComputeSha256(const TArray<uint8>& Bytes, TArray<uint8>& OutDigest);
	EHSRSaveDecodeResult DecodeEnvelope(const TArray<uint8>& Bytes, const FString& SlotName, int32 UserIndex, FHSRSaveData& OutData, FHSRSaveEnvelopeHeader* OutHeader = nullptr);
	EHSRSaveDecodeResult MigrateToCurrent(FHSRSaveData& InOutData);
}
