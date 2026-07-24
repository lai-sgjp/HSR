# TASK-P11-005: 最小 Save、ASC 重建与角色详情闭环

Status: `ACTIVE / P11-004 code gate passed; P11-004 PIE smoke pending`

## Scope

- 保持 Profile、Party、Definition、ASC/GameplayEffect 的边界。
- 设计并实现最小 Save DTO/Subsystem、加载后 Profile/Party 恢复与 ASC 重建入口。
- 不把 Actor、ASC、AttributeSet、GameplayEffect 或 Handle 写入 Save。
- 角色详情只读取纯值快照，不成为规则真源。

## Entry evidence

- P11-001/P11-002/P11-003 Build and Automation passed.
- P11-003 real PIE progression success observed.
- P11-004 Party code, Build, `HSR.Party`, and `HSR.Progression` passed.
- `HSR.PartyTest` exists; its real PIE run remains an explicit evidence task.

## Gates

1. Freeze Save v1 schema and migration/error semantics.
2. Implement Save/Load as candidate-first pure DTO flow.
3. Restore Profile and Party by stable CharacterId only.
4. Rebuild ASC attributes through the single P11-003 entry; no duplicate layers.
5. Add automation for round-trip, missing definition, corrupt/partial data, duplicate load, and failed rollback.
6. Run two cold-start PIE cycles and archive evidence before P11-006.

## Out of scope

Equipment, Inventory, Quest, network replication, full UI, and the deferred Phase 0-20 improvement list.
