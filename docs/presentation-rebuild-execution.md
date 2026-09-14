# Presentation rebuild — authorized 2026-09-12

The user's approved plan authorizes C++, configuration, Blender source/export, UMG and the three formal VerticalSlice map assets. It supersedes the old user-only asset authoring task. No Git commit/push or unrelated VRM4U binary modification is authorized.

## Deliverables
- 250 m city with six areas, 70 m observation/train hub, 65 m arena.
- Editable modular models and repeatable editor authoring scripts.
- Existing UI restyled with functional battle portrait/target/skill presentation.
- Authored formations/cameras; quest display text; optional GAS heal energy cost.
- Safe arrival validation, content integration and explicit validation report.

## Ownership
Battle stage owns world-local formation/camera configuration. Coordinator remains action authority. Widgets project ViewModel values and submit stable IDs. GAS owns cost/effect/refund. Existing save, reward and travel authorities remain authoritative.

## Verification ledger
Baseline, build, automation, editor reopen, PIE, visual multi-resolution and packaged performance are separate evidence levels. Pending checks must remain pending. Asset backups live under Saved/Presentation/backups, never replace unrelated user edits. No coverage percentage will be claimed without measurement.

Status: PLAYABLE LOCAL BUILD DELIVERED — functional repairs and recorded integration checks passed; reference-level art fidelity and first-human-play pacing are not fully accepted. Start with `docs/presentation-rebuild-delivery.md` for the final snapshot; chronological intermediate evidence remains below.

## Latest acceptance checkpoint — 18:10 local
- Editor and Windows Development builds passed. The fresh editor run completed **74/74 Automation tests successfully at 10:06:17 UTC**; retained log: `Saved/Presentation/automation_final_74.log`. A subsequent display-only forecast ordering correction is awaiting rebuild.
- Fixed canonical quest restore (objective ordering and registered FName casing), then loaded the completed on-disk QA save in newly started editors at 720p, 1080p and 1440p.
- Fixed a separately reproduced cross-restart encounter reward duplication. Admission now checks persisted completion at world interaction, pre-battle template and final submission. Successful old-save restore clears transient completion history; failed restore preserves it. The regression covers actual victory/return followed by same-instance rollback.
- `restart_reward_replay.json`: after real disk load, repeated chest/Inspector/boss interactions keep inventory and quest snapshots identical; encounters reject with a readable completed message.
- `battle_heal_pie_validation.json`: real four-versus-three battle UI, ally heal succeeds for 25 HP, finisher energy changes 100 -> 0, later unavailable skill state is observed, and battle completes.
- Seven module routes and five character tabs pass at 720p/1080p/1440p; actual viewport-sized screenshots live in `Saved/Presentation/Captures`. Latest cleanup fixes inherited minimap opacity, dialogue placement, fixed challenge columns and battle hint overlap. Final captures are still being refreshed.
- Pre-battle real Blueprint selection/confirmation enters the arena; global Back and Cancel remove the popup and restore the underlying challenge page. Character relic equip/enhance/cancel/unequip and duplicate-action rejection pass through public UI intents.
- The first Windows package succeeded at 17:29. It predates the final persistence/UI fixes; final incremental packaging and standalone frame-time/VRAM measurement remain pending. Do not distribute the first package as the final build.
- City route and stair/ramp checks, both investigation orders, four-versus-single/multiple/boss combat and partial-party-death continuation passed earlier and remain recorded below.

Known limits: first-play 10–15 minute pacing has not been measured with a new human player. Modular visuals remain stylized and simpler than the commercial references. Per-adapter GPU memory measurements include other local applications; do not label them process-exclusive VRAM.

## Final build and performance — 18:18 local
- Final editor native build succeeded at 18:11. After restart, **74/74 tests passed again at 10:17:31 UTC**, including final forecast/relic UI corrections; `automation_final_74.log` now contains this run.
- Final incremental Windows Development BuildCookRun completed successfully in 27 seconds, ExitCode 0 (`Saved/Presentation/package_final.log`). Playable package: `Saved/Presentation/Package/Windows/HSR.exe`.
- Standalone packaged city at a verified 1920×1080 client size: 10 s warmup then 60.003 s ordinary movement, 19,659 frames, **327.64 FPS average / 3.89 ms P95**, 8 waypoints. CSV mean GameThread 1.61 ms, RenderThread 3.04 ms, GPU 2.35 ms. No editor or cook process ran during this measurement.
- The test window was **hidden**. These are real game/render/GPU timings but do not measure visible-window presentation latency. The GPU is RTX 5070, 12,227 MiB; sampled whole-adapter memory peaked at 2,124 MiB, including system/other apps. Do not claim exclusive game VRAM or a universal hardware guarantee.
- First traversal encountered an uncached graphics PSO; the log records a 100 ms creation wait. Average performance exceeds the 60 FPS target, but first-use shader/PSO hitch reduction remains an optimization opportunity.
- `Saved/Presentation/Performance/summary.json` links the collected route/window/CSV evidence. `delivery.html` is the visual index; `change_manifest.json` separates this work from original VRM4U DLL changes and Blender recovery backups.
- Final 1440p relic public-intent flow, battle healing, cleaned result screen, reward confirmation/return and subsequent hub dialogue passed. The last native run again passed 74/74 tests at 10:23:43 UTC. Final UMG recook completed at 18:27 with ExitCode 0 in 17.37 seconds. No commit or push was performed.
- The recorded performance route predates the last result-HUD cleanup; no exploration movement/rendering changes followed. Final-binary startup smoke testing is recorded separately in `standalone_final_smoke.log`.

## Evidence collected (2026-09-12)
- Baseline actor/UI inventory: `Saved/Presentation/baseline/`; original map and UI backups retained.
- Modular Blender kit: 15 FBX models and `ArtSource/Presentation/NightCityKit.blend`; original equipment icon sculpts in `ItemIcons/EquipmentIcons.blend`.
- Three formal maps saved with instanced architecture, interactions, content registration and game modes.
- Editor build passed before the latest UI/GAS follow-up changes; 48 related Automation cases passed in `Saved/Logs/HSR_2.log` at 05:26 UTC. This is an intermediate result, not final acceptance.
- PIE: stable hub spawn, hub investigation, hub-to-city travel and chest opening through the actual interaction component succeeded.
- PIE inventory intent validation: weapon equip, relic enhancement before equip, relic equip succeeded; repeated confirmation rejected. Full snapshots: `Saved/Presentation/inventory_pie_validation.json`.
- PIE battle: four players versus Inspector, 10 submitted actions, terminal result and confirmation return succeeded. Full snapshots: `Saved/Presentation/battle_pie_validation.json`.
- Real UI inspection found compressed inventory from the shared root placeholder/spacer, inherited tint, missing icons and English prototype strings. Repairs are in progress and must be rechecked after the next build.
- Formal battle was still receiving legacy test base attributes; authority-level correction is in progress with regression tests.
- PNG Interchange import re-entered the UE task graph and crashed the editor. Assets saved beforehand remain on disk. The import script now explicitly selects TextureFactory; recovery/reopen verification is pending.

## Pending acceptance
Latest editor build and related Automation; post-reopen asset compile; all map routes including stairs/ramp; full investigation-order and reward/save replay matrix; single/multiple/boss combat and heal failure paths; every UI navigation and 720p/1080p/1440p captures; Development package; standalone fixed-route frame-time/VRAM measurement. None of these unrun checks are claimed passed.

## Integration update — 16:55 local
- Editor build succeeded at 16:43. The subsequent 64-test run passed 62; two SafePlacement fixture failures were diagnosed (missing controller registration and locked destination region), repaired and await rerun. Prior passing tests include real GAS heal cost/refund, equipment transactions, UI focus/preview and character presentation.
- Three maps/content and 29 UI assets were saved through the project's UnrealMCPython bridge. TextureFactory import and catalog writeback are verified; eight original item icons persist. Retargeted Remiel/Evernight animation assets are stored under Presentation/Animation.
- `route_clearance.json`: all 1,250 samples across six city routes pass. `walking_validation.json`: actual CharacterMovement traversed west stairs, east stairs and ramp successfully (16.70 / 7.11 / 12.73 seconds).
- Fresh PIE inventory: equip weapon, enhance unequipped relic, equip relic all succeed; duplicate confirms reject without spending again.
- `battle_single_validation.json`: four versus one, 12 actions, victory/return. `battle_multi_validation.json`: four versus three, 18 actions, victory/return. These use authored base attributes.
- Boss battle exposed a real partial-death turn progression defect: killing one party member skipped ResolveAction while survivors remained. Repair and regression coverage are in progress. Boss completion is NOT passed.
- Seven UI modules opened through actual frontend routes. Five character tabs returned Success. Visual checks found and are repairing stretched portraits, canvas alignment, oversized party controls and relic enum labels. Current UIAudit captures are intermediate evidence, not approved final visuals.
- Packaging config now explicitly includes the three formal maps, their data, Presentation assets and existing UI. No packaged build/performance result yet.

## Integration update — 17:28 local
- Editor build at 17:08 succeeded; all 68 selected Automation tests passed at 09:10 UTC. The SafePlacement fixture repairs and partial-death turn progression regression are included.
- Boss PIE completed in 29 actions with the leader defeated and surviving members continuing; result confirmation returned to exploration (`battle_boss_validation.json`).
- Both investigation orders and two-way travel completed. Repeated chest opening and previously resolved encounters left inventory and quest snapshots unchanged (`repeat_rewards.json`, `quest_reverse_complete.json`).
- Real disk reload of the completed QA slot revealed invalid quest restore: canonical serialization sorts objective IDs and changes FName casing. Restore now matches objectives by stable ID and validates claim identity using registered definitions. Latest native build/restart verification is in progress; the original QA save is retained unchanged.
- Pre-battle selection now uses four real profiles, supports clearing non-leader slots, hides unconfigured buffs, rejects repeated submission and restores the active frontend focus after cancellation. Added candidate/presentation tests await this build.
- Windows Development cook is progressing through uncached VRM materials. The earlier overbroad Vulkan/mobile-target cook was deliberately stopped; only DX12 SM6 and DX11 SM5 are now targeted. No packaged performance result is claimed.
- Latest UI authoring saved 29 blueprints, including readable editable-text backgrounds, bottom dialogue layout and pause menu spacing. Multi-resolution visual acceptance remains pending.
