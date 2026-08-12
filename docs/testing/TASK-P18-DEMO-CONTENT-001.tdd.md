# TASK-P18-DEMO-CONTENT-001 TDD Evidence

Status: BUILD PASS / TDD RUNTIME RED CONFIRMED / GREEN PENDING USER ASSETS

## Source and journeys

Source: user-approved Demo content plan normalized into
`docs/phase-20-demo-content-catalog.md`.

- Travel freely between two newly authored exploration maps.
- Either investigation order converges on chest, normal encounter, Boss and
  quest completion.
- Four new character configurations and two relic sets use only Demo IDs.
- Reward and Save behavior continue through existing Authorities.

## Test specification

| Guarantee | Automation test | Result |
|---|---|---|
| Exact map and directional teleport graph | `HSR.Demo.ContentCatalog.MapsAndTravel` | RED: formal assets missing |
| Four characters and twelve valid skills | `HSR.Demo.ContentCatalog.CharactersAndSkills` | RED: formal assets missing |
| Exact 4+2 relic slots and thresholds | `HSR.Demo.ContentCatalog.Relics` | RED: formal assets missing |
| Quest, encounter and reward graph | `HSR.Demo.ContentCatalog.Flow` | RED: formal assets missing |

## Expected RED boundary

The tests are read-only. Before formal assets exist under
`/Game/Data/VerticalSlice`, the suite must compile and fail on missing-asset
assertions. GREEN requires user Editor creation, Save All/reopen and a focused
rerun; old test assets cannot satisfy it.

## Build attempt evidence

The user explicitly approved retrying the UE5.6 UBT command. Two escalated
attempts ended before `Build.bat` started because the approval review service
disconnected while overloaded. This is an external tool-approval blocker, not
a compiler result. Build, test discovery and runtime RED therefore remain
`NOT RUN`; no alternative command was used to bypass the approval boundary.

The user's local UBT run then reached the new test source and reported one
test-adapter compile error: `TestNotNull` could not deduce a raw pointer from
`TObjectPtr<UHSRRewardDefinition>`. The allowlisted test now passes `.Get()`;
this setup failure is not counted as the intended content RED. A fresh Build is
required before focused Automation.

After the `.Get()` correction, the user reran the same UE5.6 Development
Editor Build. `HSRDemoContentCatalogTests.cpp` compiled, the HSR Editor module
linked, metadata was written, and UBT returned `Result: Succeeded`. MSVC 14.51
emitted the repository's known non-preferred-toolchain warning. The subsequent
Codex attempt to start focused Automation did not launch because the approval
service disconnected; runtime RED remains pending and is not inferred from the
successful Build.

## Known gaps

- Editor asset creation and PIE are user-owned and not yet verified.
- Quest title projection and authoritative energy spend for a Heal-category
  finisher are separate follow-ups.
- UE Automation exposes no useful line coverage for this content-only gate;
  focused assertions cover every frozen asset category.

## Runtime RED evidence

The user ran the focused UE5.6 commandlet after the successful Build. It found
exactly four tests under `HSR.Demo.ContentCatalog`. All four completed with
`Result={Fail}` because the exact `/Game/Data/VerticalSlice` packages did not
exist:

- `CharactersAndSkills`
- `Flow`
- `MapsAndTravel`
- `Relics`

Representative errors were `LoadPackage: SkipPackage ... package ... does not
exist on disk` followed by the intended `formal Demo asset exists ... to be not
null` assertion. The commandlet ended with `TEST COMPLETE. EXIT CODE: -1`.
This is the intended runtime RED. Test discovery, loading and assertion paths
are operational; GREEN now depends on user Editor authoring at the frozen paths.
