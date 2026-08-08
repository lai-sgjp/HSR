# TASK-P17-RELIC-EQUIPMENT-001 TDD Evidence

Date: 2026-08-07
Source intent: `docs/phase-17-execution-plan.md`, planned P17-007
Authorization: explicit user authorization for `TASK-P17-RELIC-EQUIPMENT-001`

## User journeys

- As a player, I want to choose a relic slot and inspect deterministic bag
  candidates so that I can compare a candidate with the committed relic.
- As a player, I want equip/replace to be submitted as one authority request so
  that the same instance, revisions, and displaced item remain consistent.
- As a player, I want enhancement to consume an authored material cost exactly
  once so that a retry cannot duplicate the upgrade or spend materials twice.
- As a player, I want stale, invalid, or insufficient requests to retain the
  committed state so that the UI can recover without a half-committed page.

## RED evidence

The first authorized build included the new tests before production headers
existed:

```text
Build.bat HSREditor Win64 Development E:\work\unreal_projects\HSR\HSR.uproject -NoHotReload -WaitMutex -NoUBA -MaxParallelActions=1
HSREquipmentEnhancementTests.cpp(5): fatal error C1083:
../Data/Definitions/HSREquipmentEnhancementCatalog.h: No such file or directory
```

The initial sandbox attempt failed earlier because UBT could not write its
external local environment cache; that was an environment permission failure,
not the TDD RED signal. The escalated rerun reached the intended missing
implementation error above.

## GREEN and regression evidence

The final Development Editor build completed UHT, C++ compilation, DLL/lib
link, and metadata generation with `Result: Succeeded`.

Focused final command:

```text
UnrealEditor-Cmd.exe E:\work\unreal_projects\HSR\HSR.uproject
  -unattended -nop4 -nosplash -NullRHI -NoSound
  -ExecCmds="Automation RunTests HSR.Equipment.Enhancement+HSR.UI.RelicEquipment.ViewModel; Quit"
```

Evidence in `Saved/Logs/HSR.log` at `2026.08.07-09.58.31`:

- `HSR.Equipment.Enhancement.ExactlyOnce` — Success
- `HSR.Equipment.Enhancement.FailureMatrix` — Success
- `HSR.UI.RelicEquipment.ViewModel` — Success
- test process exit code `0`

Adjacent final command included Equipment, Equipment Detail, Character Shell,
Frontend Navigation, and the new Relic tests. At `2026.08.07-10.02.49` it
found 31 tests; every listed test completed `Result={Success}` and the process
reported `EXIT CODE: 0`.

## Guarantees covered

| Guarantee | Test | Result |
|---|---|---|
| Material-backed enhancement commits once and advances both revisions once | `HSR.Equipment.Enhancement.ExactlyOnce` | PASS |
| Same OperationId replays the cached result without a second publication; changed payload is rejected | `HSR.Equipment.Enhancement.ExactlyOnce` | PASS |
| Stale revision, stale expected level, projection preflight, and insufficient material preserve old state | `HSR.Equipment.Enhancement.FailureMatrix` | PASS |
| Slot → candidate → comparison exposes real instances and pure stat deltas | `HSR.UI.RelicEquipment.ViewModel` | PASS |
| Replace refreshes the committed current instance, then enhancement options and level refresh | `HSR.UI.RelicEquipment.ViewModel` | PASS |
| ViewModel shutdown removes subscriptions and clears its transient snapshot | `HSR.UI.RelicEquipment.ViewModel` | PASS |
| Existing Equipment movement/detail, Character Shell, and Frontend navigation remain green | final 31-test regression command | PASS |

## Coverage and gaps

The repository uses UE Automation rather than a source-coverage runner. No
coverage percentage is claimed. The tests cover the authorized transaction and
UI state paths; editor asset binding, visual focus, user Save All/reopen, PIE
happy path, failure PIE, and resolution comparison remain outside this
automated evidence.

Per user direction, failure PIE may remain unrun after code/Automation wiring
review, and different-resolution checks are `NOT VERIFIED` and non-blocking.
No User UAsset was created or modified in this implementation pass.
