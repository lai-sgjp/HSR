# TASK-P17-PATCH-03F - Map Frontend and Travel Rebuild

Status: `IMPLEMENTATION AUTHORIZED / TDD REQUIRED`

## Sole outcome

The Map Frontend reads the authoritative MapSubsystem snapshot and submits one typed teleport intent. Legal A <-> B travel rebuilds the UI host exactly once after its matching arrival; rejection or travel failure preserves the committed map state and a usable exploration host.

## Authority contract

- `UHSRMapSubsystem` owns definitions, unlocks, current location, travel validation, `OpenLevel`, pending requests, arrival commit and failure cleanup.
- A Map ViewModel is a read projection of committed Map state. A Map Widget submits only a TeleportId intent and presents the result.
- Widgets must not call `OpenLevel`, set map location, unlock destinations, consume arrivals or write Save data.
- `UHSRUIManagerSubsystem` owns host teardown/rebuild, input/focus restoration and stale-host rejection. It restores only after the matching arrival generation and a valid new host.
- Locked/unknown/wrong-source/pending/invalid-package requests, failed travel, arrival mismatch/ambiguity and stale host callbacks must not change committed map state.

## Frozen write allowlist

- `Source/HSR/Map/HSRMapSubsystem.h`
- `Source/HSR/Map/HSRMapSubsystem.cpp`
- `Source/HSR/Map/HSRMapTypes.h`
- `Source/HSR/Map/HSRMapArrivalConsumer.h`
- `Source/HSR/Map/HSRMapArrivalConsumer.cpp`
- `Source/HSR/Map/HSRMapArrivalPoint.h`
- `Source/HSR/Map/HSRMapArrivalPoint.cpp`
- `Source/HSR/Data/Definitions/HSRMapDefinition.h`
- new `Source/HSR/UI/HSRMapViewModel.h/.cpp`
- new `Source/HSR/UI/HSRMapWidget.h/.cpp`
- `Source/HSR/UI/HSRUIManagerSubsystem.h`
- `Source/HSR/UI/HSRUIManagerSubsystem.cpp`
- `Source/HSR/Tests/HSRMapSubsystemTests.cpp`
- `Source/HSR/Tests/HSRMapSaveIntegrationTests.cpp`
- one new focused UI/travel Automation test
- `tasks/execution-result.md`

Everything else is read-only. Save schema, Battle travel implementation, Inventory/Equipment, Config, Blueprint, map and Content files are excluded.

## Required TDD coverage

- Snapshot maps unlock/location state into one read model with no duplicate subscriptions after host replacement.
- A legal A -> B -> A intent invokes MapSubsystem once per travel and consumes one matching arrival.
- Locked, unknown, invalid-source and no-op intents have no travel, location or UI-host mutation.
- Pending ordinary travel and pending Battle return reject the request without overwriting transition context.
- Travel failure preserves the prior snapshot and restores a usable host exactly once.
- Wrong map, missing/duplicate arrival and stale arrival generation cannot commit location or restore a stale host.
- Existing `HSR.Map`, affected `HSR.Save` and affected `HSR.UI` regressions stay green.

## User Editor boundary

User-owned assets are limited to the exact Map WBP and existing A/B map, teleport and arrival references. Blueprint may bind read-only destinations, submit the typed intent and present the result. It may not call `OpenLevel`, commit map state, unlock a destination or consume arrivals. `Map_Phase1_Exploration` remains excluded.

After C++ GREEN: Save All/reopen, PIE A -> B -> A and one locked or invalid destination. Return Output Log lines containing request ID, arrival commit, host generation and result.

## Non-goals and stops

No new Map authority, retry loop, Save schema change, Battle-return rewrite, frontend redesign, Config update, asset creation, map mutation or Blueprint implementation. Stop for any needed file outside this allowlist, including a task-selected Map WBP or DataAsset modification.
