# TASK-P17-PATCH-02 Data-Driven Radius Addendum Review

## Review metadata

- Reviewer: Independent Reviewer / Safety Reviewer
- Reviewed revision: `f6ec65e`
- Baseline Code Gate: `eb67ede`
- User PIE evidence baseline: `4f07f0d`
- Result: `REVISE`
- Date: 2026-07-28

## Production review

- Scope is authorized and allowlisted: the user explicitly requested moving SightRadius, LoseSightRadius, and EncounterRadius into `UHSREnemyDefinition`; no new file, BT behavior, state transition, Move To ownership, or Encounter admission path was added.
- Defaults preserve existing behavior: Definition values are `1000/1500/200`, matching Controller sight defaults and Character sphere-constructor default.
- Controller data flow is correctly located after possession, when the possessed Character and its Definition are available. Sight is clamped nonnegative and LoseSight is normalized to at least Sight. Missing Definition/SightConfig leaves constructor defaults unchanged.
- Character applies EncounterRadius during BeginPlay, after Blueprint/DataAsset properties are available, with a nonnegative clamp. Missing Definition leaves the constructor radius `200` unchanged.
- Dirty user Blueprint, Map, DataAsset, and `Content/AI/**` assets remain outside `f6ec65e`; provenance separation is intact.

## Blocking test finding

Automation does not test the production data flow. It only:

- reads the three Definition defaults; and
- repeats the normalization expression directly in the test with `FMath::Max`.

It never invokes `ApplyDefinitionPerceptionConfig`, never inspects the actual `SightConfig`, never verifies the no-Definition fallback, never applies EncounterRadius to `EncounterCollision`, and never verifies the Character lifecycle/helper result. Therefore the test would pass if the production calls were removed, reversed, or wired to the wrong fields.

## Minimum correction within the existing allowlist

- Extract/reuse a single production helper for applying Controller perception values, called by `OnPossess`, and expose only a `WITH_DEV_AUTOMATION_TESTS` seam/getters for the actual SightConfig radii.
- Extract/reuse a Character helper for applying Definition EncounterRadius, called by BeginPlay, with a dev-only seam/getter for the actual sphere radius.
- Automation must assert actual component/config state for:
  1. fresh no-Definition fallback remains `1000/1500/200`;
  2. Definition `1200/1000/333` produces sight `1200`, lose sight `1200`, encounter `333`;
  3. negative Definition values clamp safely to zero while LoseSight remains at least Sight;
  4. applying these values does not change AI state, epoch, target, active Encounter request, retry state, or enable Tick.
- Ensure the perception listener refresh uses the same configured SightConfig object; retain `RequestStimuliListenerUpdate` (and call `ConfigureSense` again only if required by the actual UE5.6 component contract).
- Rebuild and rerun `BehaviorTreeAdapter`; rerun `MapContract` only if Encounter/Transition production code changes.

## Existing Gate status

The earlier BT/Behavior code Gate and user PIE evidence are not invalidated by this addendum. This `REVISE` applies only to the new radius parameterization evidence. Do not mark the addendum accepted until its production seams are actually exercised.

## Conclusion

`REVISE`: production wiring is plausible and backward-compatible, but current Automation is tautological and cannot detect broken Controller/Character Definition integration. No production code should be changed by Reviewer; return the precise test repair above to Implementation.
