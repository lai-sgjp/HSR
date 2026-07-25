# TASK-P12-004 Independent Review

Status: `REVISE` (first review, 2026-07-25)

The first review found that SaveSubsystem did not yet connect Equipment export/restore, DTO state and revision coverage were incomplete, v1 normalization and collision validation were incomplete, the UI ViewModel had no authority subscription, specialty tests were absent, and `git diff --check` failed.

All first-review findings were returned to implementation. The revised Build and final Save/Equipment/UI Automation evidence are recorded in `tasks/execution-result.md`.

## Second Review

Status: `REVISE` (blocking)

- Save restores Profile/Party/Equipment authority but no BattleCoordinator/GameMode consumer rebuilds ASC equipment projections, and there is no projection-result/rollback path.
- Existing Save tests never instantiate ASC/Coordinator/EffectBridge or inject projection apply/remove failures.
- Equipment change detection omits authored modifiers and SetId.
- Restore revision validation incorrectly uses revision zero as an uninitialized sentinel.
- UI tests do not cover explicit not-initialized/error/empty states or Widget injection/rebuild.

The user explicitly authorized the required allowlist expansion. A2 correction is active; no passing verdict is claimed until fresh verification and independent re-review.

## A2 Re-review Submission

The authorized A2 correction now has successful Build, Save 6/6, Equipment 4/4, UI 1/1 and diff-check evidence. It adds a Coordinator-owned candidate ASC projection transaction invoked before Save authority commit, forced projection-failure coverage, old GE source preservation, and the second-review data/UI corrections. Third independent review is active.

Before the third review, the production identity mapping was corrected: saved equipment is keyed by the GUID derived from `PlayerCharacterId`, while the live battle participant remains `Player`. A targeted Automation fixture now proves that distinction; the post-correction Build and `HSR.Save` 6/6 run both passed.

## Third Review

Status: `REVISE` (projection source granularity)

The reviewer accepted the production identity mapping and prior A2 corrections, but required restore to preserve independent per-InstanceId handles and a separate SetSourceId, plus a real mid-transaction rollback test.

## A3 Re-review Submission

Restore now projects two independent relic InstanceId sources plus an independent threshold SetSourceId, loads the confirmed equipment and relic-set GE assets separately, and transactionally restores old sources after an injected failure that occurs after one successful runtime operation. Fresh Build, Save 6/6, Equipment 4/4 and diff-check evidence is recorded in `tasks/execution-result.md`. Fourth independent review is active.

## Fourth Review

Status: `REVISE` (read-only set UI contract)

The reviewer accepted all runtime projection corrections and required the Detail ViewModel to expose restored set state and set-source breakdown rather than only item modifiers.

## A4 Re-review Submission

The read-only snapshot now includes SetId, equipped count, threshold, active state and stable SetSourceId, with active set sources included in ordered breakdown. Automation covers 0/1/2/1 and restore notification refresh. Fresh Build, UI 1/1, Save 6/6 and diff-check evidence is recorded in `tasks/execution-result.md`. Fifth independent review is active.

## Fifth Review

Status: `PASS WITH FOLLOW-UP`

No blocking findings remain. The reviewer accepted the read-only relic-set authority snapshot, Blueprint fields, ordered active set-source breakdown, 0/1/2/1 and restore-notification UI coverage, along with all prior identity, per-instance projection, set GE, rollback, precise removal and Reset corrections.

Non-blocking follow-ups: future authored/effective set-bonus values may be exposed from pure Definition/DTO data; closeout may strengthen Widget reinjection and rollback fingerprint/ASC-value assertions.
