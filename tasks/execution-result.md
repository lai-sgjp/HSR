# TASK-P17-PATCH-03E2 Task Gate Evidence

Status: `IMPLEMENTATION AUTHORIZED / TDD RED-GREEN IN PROGRESS`

- 03E1 is archived with Build, Automation, Independent Review and user Editor/PIE compatibility evidence.
- No 03E2 Source, test, Content, Build, Automation or PIE action has started.
- The draft now records the real Inventory (`AddUnique/RemoveUnique`, capacity, revision), Equipment (Registry/placement, replace/unequip, revision) and EffectBridge boundaries, plus candidate-first transaction, OperationId replay, net-capacity replace and post-commit projection requirements.
- Read-only definition audit found no explicit Item-to-Equipment mapping: `UHSRItemDefinition` has `ItemId/StorageKind/MaxStack`, and `UHSREquipmentDefinition` has `DefinitionId/Slot/EnhancementCap`. 03E2 cannot implement cross-domain movement until the Gate freezes an authoritative mapping owner and its persistence/Editor contract.

## Role handoff: Teacher pre-review

Status: `PASS WITH PLANNING FOLLOW-UP`

- Teacher confirms five decisions must be frozen before implementation: mapping owner, stable key, one-to-one validation, static-catalog versus Save persistence behavior, and exact Editor ownership/boundary.
- Recommended direction is a dedicated static Item-to-Equipment/Relic mapping catalog. Inventory remains membership/capacity authority; Equipment remains complete payload/placement authority; `InstanceId` remains runtime ownership identity; ASC and UI remain derived/read-only.
- Missing, duplicate, conflicting, wrong-storage-kind, missing-target or incompatible-slot mappings must reject before either domain mutates. Name, prefix, display text or DefinitionId similarity inference remains prohibited.
- Recommendation is not treated as user authorization; the Gate remains `REVISE / PLANNING BLOCKER` until the mapping contract is independently reviewed and accepted.

## User authorization and RED checkpoint

- User confirmed the dedicated static Item-to-Equipment/Relic mapping catalog scheme and authorized implementation.
- RED test added at `Source/HSR/Tests/HSREquipmentMovementTests.cpp` for explicit mapping insertion, duplicate rejection and lookup.
- Mapping catalog implementation is now limited to `Source/HSR/Data/Definitions/HSRItemEquipmentMappingCatalog.h/.cpp`; no Content asset has been created.

## Role handoff: Independent Reviewer

Verdict: `REVISE`

- Reviewer confirms the definition and subsystem seam evidence and agrees that no authoritative Item-to-Equipment mapping path exists.
- Reviewer confirms the execution packet is truthful: no 03E2 Source, test, Content, Build or PIE work has started.
- Reviewer recommends freezing mapping owner, stable key, validation, Save impact and Editor boundary before implementation or Gate PASS.
