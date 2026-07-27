# Save system design

Phase 16 stable disk contract: envelope v1 is exactly 104 little-endian bytes followed by canonical payload codec v1. Header fields are `HSRSAVE\0`, format=1, header-size=104, schema/min-compatible, codec=1, flags=0, payload bytes, GUID, generation, UTC milliseconds, FNV-1a-64 slot identity, then SHA-256. The digest covers the complete file with bytes 72–103 zeroed.

Persisted `FName` IDs are canonical lowercase ASCII UTF-8 `[a-z0-9._-]`; empty is reserved for fields whose DTO semantics permit `NAME_None`. Encoding canonicalizes the logical name and decoding rejects non-canonical spelling, so IDs never depend on display casing in the UE name table. Limits: payload 16 MiB, each array 65,535 records, each token 4,096 UTF-8 bytes.

Required IDs include profile/skill, equipment definition, inventory item/definition, reward definition/item, quest/objective, and every persisted map collection entry. Optional empty IDs are limited to fields whose DTO has a real empty state, including party slots, equipment set ID, and an empty current map location. Ordering uses an explicit `bHasPrevious` state; `NAME_None` is never an ordering sentinel. Header compatibility must satisfy `1 <= MinimumCompatibleSchema <= SchemaVersion`.

Synthetic DTO migration is adjacent and conservative (`v1→v2→v3→v4→v5→v6`). It is not a promise to read historical UE `USaveGame` binary files: non-envelope data is classified as legacy. The v1 codec field order covers Profile, Party, Equipment, Inventory, Reward, Quest, then Map. Schema v6 freezes Party at exactly two slots, matching the production Party authority; encoder and decoder reject any other count. Unordered collections are encoded in ascending stable-ID order and rejected if decoded in a different order or with duplicates; decoder consumption must end exactly at payload end.

P16-001 closed the envelope, canonical codec, classification, hard-limit, migration and cross-process golden contracts. P16-003 activates the schema-v6 envelope for successful writes through the verified-copy transaction below; schema-v5 `USaveGame` remains an explicit legacy import path. Backup candidate selection and recovery-source diagnostics remain gated by P16-004.

## P16-002 validation boundary

Fixtures under `Source/HSR/Tests/Fixtures/Save/` are synthetic schema-v6 envelope inputs with fixed identity and timestamp. They are provenance for canonical bytes only, never historical UE `USaveGame` binary fixtures. Cross-domain validation is a pre-prepare gate: persisted IDs and ownership conflicts must be rejected before Profile/Party/Equipment/Inventory/Reward/Quest/Map `PrepareRestore`, equipment projection, commit, revision mutation, or delegates.

## P16-003 write transaction

The production write path uses UE `SaveDataToSlot` / `LoadDataFromSlot` with logical-slot-bound envelope identity. Physical staging and backup suffixes never participate in the envelope slot hash. Reserved role suffixes are rejected from caller-provided slot names.

The fixed order is capture/validate, encode, staging write and full readback, rotate only a fully decoded and cross-domain-valid old Primary into Backup with full readback, Primary write and full readback, then staging cleanup. A bad Primary never overwrites an existing good Backup. If Primary is untrusted but Backup is independently valid, the new Primary continues the Backup lineage. Cleanup failure is committed success with a diagnostic warning and retained staging; it is not reported as an ordinary save failure.

This is a verified-copy rotation protocol, not an atomic rename or a guarantee against process kill, power loss, disk-full, permission, locking, packaged-platform, or cross-platform failures. Backup candidate selection during Load and recovery-source diagnostics remain P16-004 scope.

## P16-004 load recovery

Load first applies argument, reserved-role, operation-reentry, map-travel, and battle-return gates. Primary is decoded and evaluated with cross-domain validation plus every domain's pure `PrepareRestore`; a valid envelope Primary or valid schema-v1–v5 legacy Primary always wins. Only when Primary is absent or invalid is Backup independently decoded and prepared. Candidate evaluation never projects, commits, changes Current, or broadcasts.

If a Primary header is trusted through checksum validation, Backup must share its SaveId and have a strictly lower generation. A checksum-invalid or otherwise untrusted Primary cannot use its header fields to reject an independently valid Backup. Future Primary headers are trusted after checksum and therefore constrain a compatible older Backup. Exactly one selected candidate proceeds to equipment projection and atomic domain commit; projection failure never falls through to another candidate.

The structured load result records source, Primary and Backup decode/stage reasons, header trust, selected lineage/generation, recovery status, and whether Runtime changed. Successful Backup recovery is read-only with respect to Primary, Backup, and Staging. Repeating the same Load remains a no-op for restore transaction revision and notifications. Load recovery does not repair disk; the next explicit successful Save performs any repair through the P16-003 transaction.

## P16-005 cold-process authority recovery

The closeout gate uses five separately launched Editor-Cmd processes against one dedicated fixed slot: Seed, VerifyPrimary, CorruptPrimary, VerifyBackup, and Cleanup. S1 and S2 differ across Profile, Party, Equipment/Relic, Inventory, Reward ledger, Quest, and Map. Seed proves Primary generation 2 and Backup generation 1 share a SaveId with no Staging. Primary verification restores S2; controlled corruption changes only one Primary payload byte; Backup verification restores S1 from generation 1 without repairing or mutating any disk role. Repeated loads do not increment the restore transaction or rebroadcast the commit delegate.

Cold-process testing exposed that `GetTypeHash(FName)` is process-local and therefore unsuitable for persisted equipment ownership. Character profile IDs now map to owner GUIDs with deterministic FNV-1a over their canonical UTF-8 bytes. Any future persisted derived identifier must follow the same rule: derive from canonical serialized bytes, never UE name-table indices or process-local hashes.

This commandlet evidence verifies authority recovery across real process restarts on the current Windows Editor build. It does not verify graphical Editor operation, packaged/Shipping builds, other platforms, real crash/power loss/disk-full/permissions/locking, concurrent writers, cloud saves, or multiplayer.
