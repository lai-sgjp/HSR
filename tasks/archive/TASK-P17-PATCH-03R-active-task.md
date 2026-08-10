# TASK-P17-PATCH-03R - Regression Compatibility for Automation Seams

Status: `IMPLEMENTATION AUTHORIZED / TDD RED-GREEN IN PROGRESS`

## Sole outcome

Restore compilation of existing Automation tests by repairing only missing development-test API declarations/guards and their existing implementations. No gameplay behavior, production transaction, Content, Config, Blueprint, map, UI or Save schema behavior may change.

## Allowlist

- Existing headers/cpps that declare or implement missing `*ForDevelopmentTest` seams named by the Build log.
- Existing Automation test files only when required to correct a stale call signature without changing assertions.
- `tasks/active-task.md`, `tasks/execution-result.md`, `tasks/final-review.md`.

## Prohibited

- No 03E2 production transaction implementation.
- No mapping catalog changes beyond the already committed checkpoint.
- No Content, Config, Blueprint, map, UI asset or business-rule changes.

## Verification

- TDD RED is the recorded UE5.6 Build failure from the pre-existing seam mismatch.
- GREEN requires HSREditor Build and affected Automation families to compile/pass.
- Any remaining unrelated failure is recorded, not hidden.
