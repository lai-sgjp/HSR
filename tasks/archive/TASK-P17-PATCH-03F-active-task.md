# TASK-P17-PATCH-03F - Map Frontend and Travel Rebuild

Status: `ARCHIVED / PASS WITH FOLLOW-UP / USER PIE ACCEPTED`

## Sole outcome

The Map Frontend reads the authoritative MapSubsystem snapshot and submits one typed teleport intent. Legal A <-> B travel rebuilds the UI host exactly once after its matching arrival; rejection or travel failure preserves the committed map state and a usable exploration host.

## Authority contract

- `UHSRMapSubsystem` owns definitions, unlocks, current location, travel validation, `OpenLevel`, pending requests, arrival commit and failure cleanup.
- A Map ViewModel is a read projection of committed Map state. A Map Widget submits only a TeleportId intent and presents the result.
- Widgets must not call `OpenLevel`, set map location, unlock destinations, consume arrivals or write Save data.
- `UHSRUIManagerSubsystem` owns host teardown/rebuild, input/focus restoration and stale-host rejection. It restores only after the matching arrival generation and a valid new host.

## Frozen implementation scope

- Map subsystem and map definition/arrival types were retained as the authority surface.
- Added `Source/HSR/UI/HSRMapViewModel.h/.cpp`, `Source/HSR/UI/HSRMapWidget.h/.cpp`, one focused Map Frontend Automation test, and travel preparation in `UHSRUIManagerSubsystem`.
- User-owned assets were committed separately in `b9d992a`: HUD/frontend wiring, map panel and intentional removal of the obsolete Pause WBP.

## Acceptance evidence

- C++ Build and focused Automation evidence is recorded in `TASK-P17-PATCH-03F-execution-result.md`.
- User PIE confirms Map.A -> Map.B -> Map.A, exactly one freeze/restore/arrival per leg and a working Pause screen after travel.

## Follow-up

HUD EndPlay emits a stale-host teardown error after the deliberate pre-travel freeze. It is behaviorally harmless in the accepted run and remains explicitly uncorrected.
