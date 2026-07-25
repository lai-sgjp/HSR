# TASK-P12-004: Save v2銆佽澶囨潵婧愬揩鐓т笌鍙璇︽儏 UI

Status: `COMPLETE / PASS WITH FOLLOW-UP / ARCHIVED`

## 鍞竴楠屾敹缁撴灉

Equip -> 鍥哄畾寮哄寲 -> Save -> 鏀瑰彉/娓呯┖ -> Load 鍚庯紝瑁呭鏄犲皠銆佸瑁呯姸鎬併€佹潵婧?breakdown 鍜?ASC 鎶曞奖鎭㈠锛涢噸澶?Load/Rebuild 涓嶅彔灞傦紝鍧忔暟鎹笉姹℃煋鏃?Runtime锛岃鎯?UI 鍙鏄剧ず涓旀棤 Tick銆?
## 瑙勫垝瑕佹眰

鍏堝畬鎴愬洓瑙掕壊 Gate 0锛屽喕缁?Save DTO銆丩oad candidate-first銆佹潵婧愬揩鐓у拰 UI 鍛戒护杈圭晫銆侰odex 涓嶅垱寤烘垨淇敼鐢ㄦ埛 Content 璧勪骇銆?
## 棰勬湡 Editor Gate

棰勮闇€瑕佺敤鎴峰垱寤?`Content/UI/WBP_EquipmentDetail_P12.uasset`锛岀粦瀹氬彧璇?ViewModel锛涘叿浣撳瓧娈靛拰甯冨眬蹇呴』鍦?Gate 0 鍚庣粰鍑恒€?
## Frozen Save/UI Contract

- SchemaVersion becomes `2`; DTO stores only stable `DefinitionId`, `InstanceId`, `CharacterId`, slot, `EnhancementLevel`, authored modifiers, SetId and authority revision. Never store UObject/DataAsset, Actor, ASC, GE Handle, Widget, final Attribute or transient pointer.
- Equipment uses a deterministic character key derived from the existing profile `FName`: `FGuid(0, GetTypeHash(CharacterId), 0, 1)`. This mapping is the sole P12-004 bridge; Save DTO stores the resulting stable `FGuid`, and UI uses the same helper. No random runtime GUID is generated for a character.
- The mapping is implemented by one shared pure helper. Candidate validation must reject two distinct profile names mapping to one equipment GUID; it may not silently overwrite a collision. The explicit GUID field remains an upgrade path for later Save versions.
- Schema v1 remains loadable with an empty equipment/relic section and is upgraded in memory to v2 on the next successful save. Unknown schema versions are rejected without mutation.
- Load is candidate-first: validate all IDs, definitions, slots, characters, finite values, caps and schema before committing any authority. On failure old runtime remains unchanged. Commit order is Profile -> Equipment -> Relic/Set -> runtime projection; repeated Load/Rebuild is idempotent.
- Source breakdown is a read-only pure snapshot with stable source IDs and authored/effective values. UI receives snapshots/events, never applies GE or owns business state, and has no Tick.

## Exact Codex allowlist

- `Source/HSR/Save/HSRSaveTypes.h`
- `Source/HSR/Save/HSRSaveGame.h`
- `Source/HSR/Save/HSRSaveGame.cpp`
- `Source/HSR/Save/HSRSaveSubsystem.h`
- `Source/HSR/Save/HSRSaveSubsystem.cpp`
- `Source/HSR/Equipment/HSREquipmentTypes.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.h`
- `Source/HSR/Equipment/HSREquipmentSubsystem.cpp`
- `Source/HSR/UI/HSREquipmentDetailTypes.h`
- `Source/HSR/UI/HSREquipmentDetailViewModel.h`
- `Source/HSR/UI/HSREquipmentDetailViewModel.cpp`
- `Source/HSR/UI/HSREquipmentDetailWidget.h`
- `Source/HSR/UI/HSREquipmentDetailWidget.cpp`
- `Source/HSR/Tests/HSREquipmentSaveTests.cpp`
- `Source/HSR/Tests/HSREquipmentDetailViewModelTests.cpp`
- `Source/HSR/Tests/HSRSaveSubsystemTests.cpp`
- `tasks/active-task.md`
- `tasks/execution-result.md`
- `tasks/final-review.md`

## Authorized A2 Allowlist Expansion

User authorized the second-review correction on 2026-07-25. In addition to the original allowlist, A2 may modify:

- `Source/HSR/Battle/HSRBattleCoordinator.h`
- `Source/HSR/Battle/HSRBattleCoordinator.cpp`
- narrowly required Equipment runtime-projection coordination interfaces under `Source/HSR/Equipment/`
- one narrowly scoped Save/ASC projection failure Automation file under `Source/HSR/Tests/`

The expansion is limited to atomic Save Load projection rebuild/rollback and its tests; it does not authorize new gameplay or Content asset changes.

## Editor Gate

After C++ Build/Automation passes, user creates and reopens `Content/UI/WBP_EquipmentDetail_P12.uasset`; it binds selected character, six slots, enhancement, set threshold, ordered source rows, aggregate stats and loading/empty/error state. No equipment action buttons, GE application, or Tick.

## Current Editor Instructions

The revised C++ Build and the final Save 4/4, Equipment 4/4 and Equipment Detail UI 1/1 Automation runs passed. The user Editor asset gate also passed; only Independent Re-review remains.

- Create `Content/UI/WBP_EquipmentDetail_P12.uasset` from `UHSREquipmentDetailWidget`.
- Add read-only text/rows for selected CharacterId, six slots, DefinitionId/InstanceId, EnhancementLevel, SetId/Threshold, source labels, MaxHealth/Attack/Defense/Speed breakdown and totals.
- Add loading, empty and restore-error visual states.
- Bind only to the Equipment Detail ViewModel snapshot/change event.
- Do not add Equip/Unequip/Enhance buttons, GE application nodes, direct ASC/Actor reads or Widget Tick.
- Compile, Save, close and reopen Editor, then verify all bindings remain and no Blueprint compile/runtime errors appear.

The user has already reported the asset path and successful reopen result.

## User UI Asset Gate Evidence

`USER PROVIDED PASS`: `/Game/UI/WBP_EquipmentDetail_P12` exists at the instructed path, survived Editor restart, and produced no Blueprint compile or runtime errors. The remaining gate is Build/Automation plus Independent Review; Codex does not modify the asset.

## 闈炵洰鏍?
涓嶈繘鍏ュ畬鏁?Save 杩佺Щ/澶囦唤銆佽儗鍖呫€佸鍔便€佺粺涓€ Screen Stack銆佺綉缁滄垨姝ｅ紡缇庢湳銆?
