# TASK-P17-PATCH-03G - Save UI and Integrated Restore

Status: `COMPLETE / PASS WITH FOLLOW-UP / USER ACCEPTED`

## Sole Outcome

The frontend provides manual Save, Load and overwrite requests with typed results. A Load restores the authoritative Character, Party, Inventory, Equipment, Reward and Map chain through Map-owned travel and arrival.

## Authority Contract

- `UHSRSaveSubsystem` owns slot validation, disk selection, candidate validation and global restore completion.
- MapSubsystem owns restore travel, pending request, arrival placement and Map arrival publication.
- Save UI only forwards typed intents and presents result DTOs.

## Completed Scope

- Save/Load/overwrite-confirmation UI facade.
- Cross-map Map-owned restore travel with saved-transform placement.
- Same-map saved-transform placement.
- Cross-map non-Map commit deferred until matching restore request arrival.
- Focused Build and Automation validation plus successful user PIE restore.

## Preserved Follow-Up

The injected Editor restore-failure/timeout path and its UI presentation were explicitly deferred by the user. They remain `NOT VERIFIED` and do not represent a successful failure-path test.
