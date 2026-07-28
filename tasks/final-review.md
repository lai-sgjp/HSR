# TASK-P17-PATCH-03C Review

Status: `TASK GATE REVIEW REQUIRED`

## Review inputs

- `tasks/active-task.md`
- `docs/phase-17-patch-03-execution-plan.md`
- `docs/system-operation-flow.md`
- current Interaction, Graybox encounter adapter, Encounter DTO and BattleTransition source

## Verdict

Initial Implementation feasibility review: `REVISE`.

- Blocking mismatch: the draft required reward-bundle zero-pollution preflight while Reward was read-only, but current `RegisterBundle` is mutating and full validators are private.
- Revision 1 adds `HSRRewardSubsystem.h/.cpp` only for a public const `CanRegisterBundle` preflight and preserves atomic definition-metadata registration after all admission checks. Reward grants, receipts, settlement and Inventory source changes remain prohibited.

Re-review: `PENDING`.

## Coordinator source audit revision 2

- Current request DTO has no PlayerCharacterId while BattleGameMode rebuilds from configured `PlayerCharacterId`; this would bypass 03B and violate stable-ID rebuild.
- Revision 2 adds pure-value PlayerCharacterId captured from read-only Party slot 0 and narrowly allows BattleGameMode h/cpp to use the consumed ID. Coordinator and Party/Profile implementations remain read-only.
- The card also freezes null-World travel failures as uncorrelatable and permits only a validation-preserving Automation travel dispatcher/counter.
