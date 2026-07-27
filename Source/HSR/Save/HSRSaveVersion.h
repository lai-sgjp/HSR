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
	uint32 SchemaVersion = 6;
	uint32 MinimumCompatibleSchema = 1;
	FGuid SaveId;
	uint64 Generation = 1;
	int64 UtcUnixMilliseconds = 0;
	uint64 SlotIdentity = 0;
};

namespace HSRSaveVersion
{
	constexpr int32 CurrentSchema = 6;
	constexpr uint32 MaxRecordCount = 65535;
	constexpr uint32 MaxTokenBytes = 4096;
	constexpr uint32 PartySlotCount = 2;
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
