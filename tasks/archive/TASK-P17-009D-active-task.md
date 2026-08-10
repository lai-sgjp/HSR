# TASK-P17-009D - Pre-Battle Buff GameplayEffect and Resource Authority

Status: `COMPLETE / ENGINE AUTOMATION PASS / USER PIE PASS`

## Phase and mapping

Phase 17 UI integration, canonical `P17-009 Party and pre-battle Party/Buff`.
009A supplied the read-only Party projection, 009B supplied permanent-party
candidate editing, and 009C supplied the local pre-battle candidate, Buff ID
metadata, pure Encounter Request, and BattleTransition admission. 009D is the
next isolated gameplay-authority slice; it must not reopen those accepted UI
flows.

## Sole observable outcome

For a valid pre-battle Buff selection, the authoritative battle admission
path validates the stable Buff ID against the current Encounter/Stage, applies
the configured GameplayEffect exactly once to the battle player ASC, and
consumes the configured resource exactly once. Invalid, locked, unknown,
duplicate, stale, repeated, insufficient-resource, or failed-application
requests leave the permanent Party, Inventory/resource state, Reward,
completion state, and BattleTransition state unchanged.

## Authority and data flow

- The Pre-Battle ViewModel remains a pure local candidate and may only submit
  stable IDs; it is not the Buff or resource authority.
- A runtime Buff definition/registry is the source of Buff validity, allowed
  Encounter/Stage, GameplayEffect class, and resource cost. The exact owner
  must be frozen before implementation; no existing Status or Equipment GE may
  be silently reused as a Stage Buff.
- BattleTransition owns cross-map admission and duplicate admission guards.
- Battle Coordinator owns battle-local ASC application and the exactly-once
  application receipt.
- Inventory/resource authority owns the actual resource mutation. A failed GE
  application or travel/admission failure must not leave a consumed resource.
- Widget/Blueprint must not call `OpenLevel`, apply a GE, consume resources,
  mutate Party, or mark completion.

## Gate 0: resolved implementation contract

1. `UHSRStageBuffDefinition` owns Buff ID, enabled state, GE class, resource
   item ID, and non-negative resource cost. Each `UHSREncounterDefinition`
   owns its allowed definitions.
2. `UHSRStageBuffAuthority`, owned by `UHSRBattleTransitionSubsystem`,
   registers Encounter definitions and rejects empty, unknown, locked,
   duplicate, malformed, and GE-less selections before pending/travel.
3. The existing `UHSRInventorySubsystem` is the resource authority:
   admission aggregates costs for preflight and `RemoveStack` performs the
   battle-local debit. `AddStack` is used only to refund a failed build.
4. `UHSRBattleCoordinator` stores the consumed request's selected IDs and
   records one active GE handle plus one debit receipt per stable Buff ID.
   Duplicate IDs are rejected at admission, so a repeated confirm cannot add
   a second receipt.
5. Coordinator applies the GE before its corresponding debit. Any build
   failure removes applied Stage Buff GEs and refunds recorded debits; normal
   `Reset` removes only battle-local GEs and does not refund a completed
   expenditure.

## RED/GREEN acceptance matrix

1. Unknown/empty/duplicate Buff ID is rejected with no state mutation.
2. Locked or Encounter-incompatible Buff is rejected with no state mutation.
3. Valid Buff produces a pure request carrying only stable metadata.
4. Valid admission applies one GameplayEffect and one resource debit.
5. Repeated Confirm, repeated admission, or repeated receipt cannot apply or
   debit a second time.
6. Insufficient resource rejects before any GE or transition mutation.
7. Injected GE creation/application failure restores the resource and leaves
   Party, Inventory, Reward, completion, and transition snapshots unchanged.
8. Cancel restores the local candidate and never mutates permanent state.

## Exact implementation allowlist (after Gate 0 and explicit authorization)

### Existing files

- `Source/HSR/UI/HSRPreBattleCandidateViewModel.h`
- `Source/HSR/UI/HSRPreBattleCandidateViewModel.cpp`
- `Source/HSR/Battle/HSREncounterTypes.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.h`
- `Source/HSR/Battle/HSRBattleTransitionSubsystem.cpp`
- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.h`
- `Source/HSR/Data/Definitions/HSREncounterDefinition.cpp`
- `Source/HSR/Inventory/HSRInventorySubsystem.h`
- `Source/HSR/Inventory/HSRInventorySubsystem.cpp`
- `Source/HSR/Tests/HSRPreBattleCandidateTests.cpp`
- `Source/HSR/Tests/HSRInteractionBattleTests.cpp`

### New files, only if Gate 0 selects this contract

- `Source/HSR/Data/Definitions/HSRStageBuffDefinition.h`
- `Source/HSR/Data/Definitions/HSRStageBuffDefinition.cpp`
- `Source/HSR/Battle/HSRStageBuffAuthority.h`
- `Source/HSR/Battle/HSRStageBuffAuthority.cpp`
- `Source/HSR/Tests/HSRStageBuffAuthorityTests.cpp`

### Coordination files

- `tasks/active-task.md`
- `tasks/execution-result.md` (implementation agent only)

Closeout files may be changed only after evidence exists:
`tasks/final-review.md`, `tasks/archive/TASK-P17-009D-*`, `worklog.md`,
`PROJECT_STATE.md`, and `todo_plan.md`.

## Explicit non-goals and stop conditions

- No permanent Party rewrite, party expansion, reward/save schema change,
  unlock progression, network/RPC work, Phase 18 presentation, or full Buff
  editor.
- No new or modified UAsset is authorized for Codex. The user owns creation,
  binding, compile/save/reopen, and provenance of any Buff DataAsset or
  GameplayEffect asset.
- Do not make Widget or Blueprint the authority and do not treat arbitrary
  `Buff.Test` metadata as a real Buff definition.
- Stop if the resource API, Buff definition owner, or required asset cannot be
  identified without a new cross-domain contract.

## User Editor work required for final acceptance

Codex did not create or modify any UAsset. The user must create a
`UHSRStageBuffDefinition` DataAsset, an active/infinite GameplayEffect asset,
and a stackable resource `UHSRItemDefinition`; configure the stable IDs and
cost, add the Buff definition to the target Encounter's
`StageBuffDefinitions`, then Compile/Save All and reopen the Editor. Final
PIE evidence must cover valid apply/debit, Cancel, duplicate confirm,
insufficient resource, forced application failure/refund, and post-travel
state.

## Required verification

- Preserve the first RED failure log.
- Fresh `HSREditor Win64 Development` build.
- Focused 009D Automation and existing PreBattle/Admission/Challenge/
  FrontendNavigation/Map/TravelRestore regressions.
- `git diff --check`.
- User Editor/PIE evidence for valid apply/debit, cancel, duplicate confirm,
  insufficient resource, injected application failure, and post-travel state.

## Execution authorization

The C++ implementation, commandlet gates, user-owned asset configuration, and
PIE flow are complete. No asset provenance is claimed by this task card.
