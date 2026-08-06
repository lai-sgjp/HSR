# TASK-P17-011 - Save UI

Status: `IMPLEMENTATION GREEN / USER EDITOR-PIE PENDING`

## Sole observable outcome

From the existing Pause Hub, the user can inspect the two real Save slots,
submit Save, Load, and Overwrite intents, and see typed results. A successful
Load restores the authoritative runtime and leaves a valid UI/input host; a
failed Load leaves the old World and its input usable. Delete is not part of
this task.

The implementation authorization was granted by the user on 2026-08-06.
Blueprint/UAsset work and final Editor/PIE acceptance remain pending.

## Baseline and existing evidence

- Phase 17 P17-010C is archived as `USER ACCEPTED / INDEPENDENT REVIEW NOT
  RUN` after Build, combined Challenge/Battle/Save Automation `36/36`, and
  user PIE progression evidence. Do not reopen its Challenge, Save schema 8,
  Settlement, Buff, Party, Map, BattleReturn, or FrontendModuleRoot logic.
- P17-PATCH-03G already delivered the Save ViewModel/Widget facade and
  Map-owned integrated restore. P17-PATCH-03H delivered the cold-load
  production-definition repair and accepted successful cross-map restore.
  The restore arrival/failure injection path remains an explicit follow-up.
- Existing Save UI source is:
  `Source/HSR/UI/HSRSaveViewModel.h/.cpp` and
  `Source/HSR/UI/HSRSaveWidget.h/.cpp`.
- Existing Save authority is:
  `Source/HSR/Save/HSRSaveSubsystem.h/.cpp`, `HSRSaveTypes.h`,
  `HSRSaveVersion.h/.cpp`, and `HSRSaveGame.h/.cpp`.
- Existing user-owned Save panel asset:
  `/Game/UI/P17/Frontend/WBP_HSRSavePanel_P17`.
- The accepted physical slot names for this task are `p17_slot_01` and
  `p17_slot_02`. UI must not derive slot identity from display text.
- The accepted exploration Pause shortcut is the numeric `1`; this task must
  not restore the historical `Esc` shortcut or change the existing input map.

## Gate 0 ownership contract

1. `UHSRSaveSubsystem` is the only Save/Load authority. It owns slot
   validation, envelope decode, schema compatibility, Primary/Staging/Backup
   selection, candidate validation, and the global restore transaction.
2. `UHSRMapSubsystem` remains the only owner of restore travel, pending travel,
   arrival placement, and arrival publication. Save UI never calls
   `OpenLevel`, creates a teleport, or applies a transform.
3. `UHSRBattleTransitionSubsystem` remains the owner of Battle Return pending
   state. A Load during Map Travel or Battle Return pending is rejected before
   any restore mutation.
4. Character Profile, Party, Equipment, Inventory, Reward, Quest, Map, and
   Challenge Progression subsystems retain their current state ownership. Save
   only captures/prepares/commits their existing DTO contracts.
5. `UHSRSaveViewModel` owns only transient slot-summary projection, the current
   typed frontend result, a pending overwrite intent, and a pending deferred
   Load presentation state. It never writes `USaveGame` or disk bytes.
6. `UHSRSaveWidget` owns only UMG forwarding and presentation callbacks. It
   does not call a domain subsystem directly and does not invent success,
   generation, map, or party values.
7. `UHSRUIManagerSubsystem`, Frontend Router, Screen Stack, and
   `WBP_FrontendModuleRoot_P17` remain the UI host owners. Save Panel is module
   content under the existing `ModuleContentHost`; it must not call
   `AddToViewport`.

## Frozen Save UI contract

### Slot summary

The authority must provide a read-only summary for exactly `p17_slot_01` and
`p17_slot_02`. A summary query may read and decode Primary/Backup, but it must
not call `LoadSnapshot`, mutate `Current`, publish restore delegates, or write
Staging/Primary/Backup.

The projected summary contains only display-safe values:

- stable `SlotName`;
- `Empty`, `Ready`, `Recoverable`, or `Unavailable` display state;
- `Generation` and envelope UTC timestamp when a trusted record exists;
- saved `MapId`, party member count, and completed Challenge count when the
  selected record is valid;
- Primary/Backup presence and whether the displayed record came from Backup;
- a typed diagnostic/result for `Unavailable` without exposing raw disk bytes.

Summary state precedence is:

1. neither physical record exists -> `Empty`;
2. valid Primary -> `Ready`;
3. invalid/untrusted Primary with a valid lineage-compatible Backup ->
   `Recoverable`;
4. physical data exists but neither record is safely usable, or the record is
   unsupported/definition-invalid -> `Unavailable`.

The summary is informational. It does not reserve a slot and does not change
the authority's next generation.

### Save and Overwrite

- Save to an `Empty` slot calls `UHSRSaveSubsystem::SaveToSlot` exactly once
  for that frontend intent.
- Any physical Primary or Backup record, including a corrupt or unsupported
  record, requires an explicit Overwrite confirmation. The pending intent stores
  the stable slot name, not a widget pointer or a recomputed display label.
- Overwrite Cancel clears the pending intent and performs zero disk writes,
  zero capture, and zero generation changes.
- Confirm Overwrite calls the existing atomic `SaveToSlot` path exactly once.
  Staging/Primary/Backup ordering, cleanup, and rollback remain Save authority
  behavior and are not duplicated in UI.
- A completed Save/Overwrite is reported from the typed authority result. The
  frontend action-acceptance enum must not be mistaken for a successful disk
  write; `SaveFailed`, `LoadFailed`, and failure-stage diagnostics remain
  visible to the Widget.

### Load

- Loading `Empty` returns typed `SlotNotFound`; loading `Unavailable` returns
  the authority's typed failure and does not mutate the live runtime.
- Load calls only `LoadFromSlot`. It must never call `LoadSnapshot` from UI.
- Primary/Backup recovery is displayed as successful Load with
  `bRecoveredFromBackup=true`, while the summary remains truthful about the
  selected source.
- Same-map Load completes in the current host. Cross-map Load may return an
  accepted pending result while Map owns restore travel; the ViewModel must
  remain pending until Save authority publishes the final arrival success or
  failure.
- Cross-map success rebuilds the normal Exploration HUD/Frontend host through
  the existing UIManager/Map arrival flow. It does not resurrect a stale Save
  Widget or directly re-add the old panel. The user can reopen Save with Pause
  shortcut `1`.
- Restore-travel failure clears the pending presentation, reports typed
  `LoadFailed`, commits no non-Map candidate, and leaves the previous World,
  input mode, and UI session usable.
- A second Save/Load/Overwrite click while the same intent is pending is
  rejected/consumed without a second disk transaction or restore travel. A
  new Save after the previous operation has completed is a new user intent and
  may legitimately advance the slot generation.

## Exact implementation allowlist after separate authorization

The following is the maximum proposed production/test write set. It is not an
implementation grant until the user separately authorizes P17-011 production
work:

- `Source/HSR/Save/HSRSaveTypes.h`
  - typed slot-summary projection and, if required by the existing deferred
    restore seam, a typed Save-operation completion payload/delegate;
  - no Save schema or envelope layout change.
- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
  - read-only slot summary query and final deferred Load completion publication;
  - reuse existing validation/recovery/restore transaction; no new authority.
- `Source/HSR/UI/HSRSaveViewModel.h`
- `Source/HSR/UI/HSRSaveViewModel.cpp`
  - project summaries, gate duplicate pending intents, retain overwrite
    intent, and consume typed completion results.
- `Source/HSR/UI/HSRSaveWidget.h`
- `Source/HSR/UI/HSRSaveWidget.cpp`
  - expose Blueprint-safe summary/result access and forward user intent only.
- `Source/HSR/Tests/HSRSaveFrontendTests.cpp`
  - focused Save UI contract tests listed below.
- `tasks/execution-result.md`
  - implementation and verification evidence only after authorized execution.

Everything else is read-only unless a failing compile/test proves a directly
necessary seam and the task is explicitly reopened. In particular, the
following are excluded from the write set: `HSRSaveVersion.*`, `HSRSaveGame.*`,
all Save schema DTO migration code, `HSRUIManagerSubsystem.*`, Frontend Router,
Screen Stack, Map, Battle, Reward, Inventory, Party, Challenge, Input, Config,
Build.cs, plugins, new modules, and all existing accepted production logic.

## User-owned Editor asset allowlist

No new asset is required. If implementation is authorized, the user may edit
only these existing assets for Save presentation and route wiring:

- `Content/UI/P17/Frontend/WBP_FrontendShell_P17.uasset`
- `Content/UI/P17/Frontend/WBP_FrontendModuleRoot_P17.uasset`
- `Content/UI/P17/Frontend/WBP_HSRSavePanel_P17.uasset`

The already accepted `ModuleContentHost` binding must remain present. The
Save panel must be created/attached as ModuleRoot content in the existing
`OnModuleChanged`/route path; no panel may independently use
`AddToViewport`. No InputAction/IMC asset is in scope, and the Pause shortcut
remains numeric `1`.

## User Editor procedure after implementation authorization

1. Open `WBP_FrontendShell_P17`; verify the Save Hub entry submits
   `EHSRFrontendModule::Save` through the existing Frontend intent. Do not add
   a direct SaveSubsystem call.
2. Open `WBP_FrontendModuleRoot_P17`; keep `ModuleContentHost` as the existing
   Overlay/CanvasPanel. For the Save module, create or display the existing
   `WBP_HSRSavePanel_P17` under that host. Preserve Character, Inventory, Map,
   Challenge, Quest, Back/X, and Battle Return branches unchanged.
3. Open `WBP_HSRSavePanel_P17`; verify its C++ parent is `UHSRSaveWidget` and
   bind Slot 1/2 to the stable names `p17_slot_01`/`p17_slot_02`. Bind Save and
   Load buttons to the Widget forwarding functions, and bind overwrite Confirm
   and Cancel to the existing confirmation controls.
4. Bind the slot summary and `OnSaveResultChanged` projections to text/state
   presentation. Empty, Ready, Recoverable, Unavailable, Pending, Success,
   and typed failure must be visible without Blueprint-side business rules.
5. Disable or guard conflicting Save/Load controls while a deferred Load or
   Overwrite confirmation is pending. Cancel must restore the previous enabled
   state without writing.
6. Compile each WBP, Save All, close and reopen the Editor, then compile again.
   Confirm the three asset references and `ModuleContentHost` survive reopen.
7. In PIE, open Pause with numeric `1`, enter Save, complete the matrix below,
   and after cross-map Load confirm the rebuilt HUD, input, Back/X, and numeric
   `1` Pause path.

## Required Automation and evidence matrix

### Codex Build/Automation

| Check | Required result |
|---|---|
| Fresh `HSREditor Win64 Development` Build | UHT, compile, link, metadata all pass |
| `HSR.UI.SaveFrontend` | Focused summary, intent, overwrite, pending completion and failure tests pass |
| `HSR.Save` | Existing Save tests remain green; no schema count or migration regression |
| `HSR.Save.WriteTransaction` / `HSR.Save.LoadRecovery` | Existing Primary/Staging/Backup and recovery matrix remains green |
| `HSR.ColdSave.*` | Existing cold primary/backup/idempotence tests remain green |
| `HSR.UI.FrontendNavigation` | Existing 11 navigation tests remain green |
| `HSR.UI.ScreenLifecycle.TravelRestore` and `HSR.Save.MapTravelMutualExclusion` | Existing integrated restore/UI lifecycle remains green |
| `git diff --check` | Pass; `.claude/**` remains untracked and excluded |

Minimum focused test cases:

- both accepted slot summaries initially `Empty`;
- valid Primary summary exposes generation/map/party/challenge projection;
- valid Backup fallback is `Recoverable` and marks the displayed source;
- corrupt/unsupported/definition-invalid data is `Unavailable`;
- Save to Empty writes once and reports the authority result;
- existing physical slot requires confirmation;
- Overwrite Cancel performs no write and leaves generation/bytes unchanged;
- Overwrite Confirm advances generation exactly once;
- missing/invalid Load preserves the live snapshot and returns typed failure;
- Map Travel Pending and Battle Return Pending reject before mutation;
- deferred Load success/failure produces one final frontend result;
- repeated pending Load/Save/Confirm produces no duplicate transaction or travel;
- Widget receives result updates without direct SaveGame or Map calls.

### User Editor/PIE

| Case | Expected evidence |
|---|---|
| Fresh Pause -> Save open | numeric `1` opens Pause; Save panel appears under ModuleContentHost; no direct viewport host |
| Empty slots | Slot 1/2 show `Empty`; no fabricated generation/map values |
| Save Slot 1 | `p17_slot_01` becomes `Ready`; generation and saved summary appear |
| Save Slot 2 | `p17_slot_02` independently saves and displays its own summary |
| Overwrite Cancel | confirmation closes; no generation/result success and old UI remains usable |
| Overwrite Confirm | one generation increment and one typed success; no duplicate write |
| Missing/invalid Load | typed failure is visible; old World remains movable and Pause can reopen with `1` |
| Same-map Load | saved runtime projection and UI result are coherent; no stale duplicate panel |
| Cross-map Load | one Map-owned restore travel, arrival, host rebuild, saved transform/runtime; rebuilt HUD/input works |
| Cross-map failure | old World/host/input remain usable and no partial Profile/Party/Inventory/Reward/Quest/Challenge commit occurs |
| Editor reopen | Save All + close/reopen preserves parent classes, ModuleContentHost, route and slot bindings |
| Regressions | Character, Inventory, Map, Challenge, Back/X, Battle Return and post-return numeric `1` Pause remain unchanged |

The user-provided PIE log is evidence for visible Editor/PIE behavior; Codex
must not relabel it as Automation. Standalone, Packaged, Shipping, physical
controller, two-resolution visual coverage, and network behavior remain
`NOT VERIFIED` unless separately demonstrated.

## Explicit non-goals and stop conditions

- No Delete button/API, cloud save, schema version, migration, serialization
  layout, slot count expansion, new SaveGame class, or Config change.
- No new reward grant, resource deduction, Buff, battle, party, challenge,
  inventory, map travel, or UI root architecture.
- No direct `USaveGame`/`UGameplayStatics::SaveGameToSlot` call from Widget or
  Blueprint. The existing Save authority is the only disk path.
- No restoration of Character/Inventory direct `AddToViewport`.
- Stop and request a new Gate if implementation needs UIManager route changes,
  a new cross-domain event not owned by Save, schema migration, new assets,
  input changes, deletion, or a change to accepted P17-009/010 behavior.

## Current Gate 0 decision

The scope is adjacent to Phase 17, independently verifiable, and has a
bounded implementation allowlist. The existing Save UI is a useful facade but
does not yet satisfy real slot summaries and deferred failure presentation.
The user authorized the implementation allowlist on 2026-08-06. The C++
implementation and focused Automation are GREEN; final user-owned WBP wiring,
Editor reopen, and PIE happy/failure evidence are still required before this
task can be archived.

## Implementation checkpoint

- RED checkpoint: `8b75f86` (`test: add P17-011 save frontend red contract`).
  The new test target failed only because the slot-summary and deferred-load
  interfaces did not yet exist.
- GREEN implementation is limited to the exact Source/test allowlist above:
  read-only Primary/Backup summary, typed deferred Load completion, and
  ViewModel/Widget projection. No Save schema or accepted domain logic was
  reopened.
- User Editor/PIE acceptance is not yet complete. Do not mark this task
  archived until the existing Save Panel is wired under ModuleContentHost and
  the required matrix is evidenced.
