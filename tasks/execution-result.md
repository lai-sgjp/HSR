# TASK-P17-PATCH-03E2 Task Gate Evidence

Status: `TASK GATE REVISE / PLANNING BLOCKER / IMPLEMENTATION NOT AUTHORIZED`

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
