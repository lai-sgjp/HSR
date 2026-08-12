# Phase 20 Demo Content Catalog

Status: FROZEN FOR EDITOR AUTHORING

This catalog is the source of truth for `TASK-P18-DEMO-CONTENT-001`. Runtime
identities use `Demo.*`; display labels are authored in Chinese. Existing test
and phase-numbered assets are not valid substitutes.

## Maps and travel

| Asset path | Stable ID | Display name | Required references |
|---|---|---|---|
| `/Game/Data/VerticalSlice/Maps/DA_Map_ObservationCar` | `Demo.Map.ObservationCar` | 观景车厢 | `/Game/Maps/VerticalSlice/Map_ObservationCar`, region `Demo.Region.DomainRoute`, arrival `Demo.Arrival.ObservationCar.DomainAnchor` |
| `/Game/Data/VerticalSlice/Maps/DA_Map_NewEriduSixthStreetMetro` | `Demo.Map.NewEriduSixthStreetMetro` | 新艾丽都六分街地铁站 | `/Game/Maps/VerticalSlice/Map_NewEriduSixthStreetMetro`, same region, arrival `Demo.Arrival.SixthStreet.DomainAnchor` |
| `/Game/Data/VerticalSlice/Maps/DA_Map_HertaSupportSection` | `Demo.Map.HertaSupportSection` | 黑塔空间站支援舱段 | `/Game/Maps/VerticalSlice/Map_HertaSupportSection`, region `Demo.Region.Battle`, arrival `Demo.Arrival.HertaSupportSection.BattleStart` |
| `/Game/Data/VerticalSlice/Teleports/DA_Teleport_ObservationCarToSixthStreet` | `Demo.Teleport.ObservationCarToSixthStreet` | 介域定标 | ObservationCar -> SixthStreet; initially unlocked |
| `/Game/Data/VerticalSlice/Teleports/DA_Teleport_SixthStreetToObservationCar` | `Demo.Teleport.SixthStreetToObservationCar` | 介域定标 | SixthStreet -> ObservationCar; initially unlocked |

Place `Demo.NPC.Catherine` / 凯瑟琳 in ObservationCar and
`Demo.Chest.WangXiaYiTong` / 王下一桶 in SixthStreet. Both combat encounters
load HertaSupportSection and return through the existing battle-return DTO.

## Characters and skills

Character assets live under `/Game/Data/VerticalSlice/Characters`; skill
assets live under `/Game/Data/VerticalSlice/Skills`. Skill arrays are ordered
Basic, Skill, Ultimate/finisher.

| Character ID | Display | Role / element | HP | ATK | DEF | SPD |
|---|---|---|---:|---:|---:|---:|
| `Demo.Character.EvernightMoon` | 长夜月 | main damage / `Element.Arc` | 125 | 22 | 10 | 104 |
| `Demo.Character.Huohua` | 火花 | DoT support / `Element.Gale` | 105 | 15 | 10 | 116 |
| `Demo.Character.Remiel` | 蕾米埃尔 | break support / `Element.Arc` | 115 | 17 | 12 | 110 |
| `Demo.Character.Verina` | 维林娜 | healing / `Element.Tide` | 110 | 14 | 11 | 112 |

| Asset suffix | SkillId | Display | Category | Target | Multiplier | SP | Energy | Status / toughness |
|---|---|---|---|---|---:|---:|---:|---|
| `EvernightMoon_Basic` | `Demo.Skill.EvernightMoon.Basic` | 月蚀 | BasicAttack | SingleEnemy | 1.00 | +1 | 0 | Arc / 1 |
| `EvernightMoon_Nightfall` | `Demo.Skill.EvernightMoon.Nightfall` | 长夜降临 | Skill | SingleEnemy | 1.55 | -1 | 0 | Arc / 2 |
| `EvernightMoon_FullMoon` | `Demo.Skill.EvernightMoon.FullMoon` | 永夜满月 | Ultimate | SingleEnemy | 2.40 | 0 | 100 | Arc / 3 |
| `Huohua_Basic` | `Demo.Skill.Huohua.Basic` | 焰彩 | BasicAttack | SingleEnemy | 0.80 | +1 | 0 | Gale / 1 |
| `Huohua_BloomingStage` | `Demo.Skill.Huohua.BloomingStage` | 绽放舞台 | Skill | SingleEnemy | 1.10 | -1 | 0 | DamageOverTime / 1 |
| `Huohua_GrandOpening` | `Demo.Skill.Huohua.GrandOpening` | 盛典开幕 | Ultimate | SingleEnemy | 1.70 | 0 | 100 | DamageOverTime / 2 |
| `Remiel_Basic` | `Demo.Skill.Remiel.Basic` | 圣裁 | BasicAttack | SingleEnemy | 0.90 | +1 | 0 | Arc / 2 |
| `Remiel_Riftlight` | `Demo.Skill.Remiel.Riftlight` | 断界之光 | Skill | SingleEnemy | 1.25 | -1 | 0 | Arc / 4 |
| `Remiel_SkyJudgment` | `Demo.Skill.Remiel.SkyJudgment` | 天穹裁决 | Ultimate | SingleEnemy | 1.90 | 0 | 100 | Break / 6 |
| `Verina_Basic` | `Demo.Skill.Verina.Basic` | 蔓生 | BasicAttack | SingleEnemy | 0.70 | +1 | 0 | Tide / 1 |
| `Verina_HealingSprout` | `Demo.Skill.Verina.HealingSprout` | 治愈新芽 | Heal | SingleAlly | 1.00 | -1 | 0 | healing effect |
| `Verina_BreathingGarden` | `Demo.Skill.Verina.BreathingGarden` | 森息花园 | Heal | SingleAlly | 1.80 | 0 | 100 | healing effect; display-only energy |

Damage skills use `Damage.Type.Physical` and bind existing reusable Ability,
damage rule and GameplayEffect assets. Ultimate skills bind existing cost and
refund effects. Verina's finisher remains Heal-category; actual 100-energy
authority needs a separate gameplay task.

## Relics and item mapping

Create sets under `/Game/Data/VerticalSlice/Relics/Sets`, pieces under
`/Game/Data/VerticalSlice/Relics/Pieces`, unique item definitions under
`/Game/Data/VerticalSlice/Items/Relics`, and same-ID mappings in a
VerticalSlice equipment mapping catalog.

| Set ID / display | Threshold | Slots |
|---|---:|---|
| `Demo.RelicSet.ShiningMagicalGirl` / 闪耀功勋的魔法少女 | 4 | Head, Hands, Body, Feet |
| `Demo.RelicSet.HeavenLiveRoom` / 天国@直播间 | 2 | PlanarSphere, LinkRope |

Piece IDs:

- `Demo.Relic.ShiningMagicalGirl.Head`
- `Demo.Relic.ShiningMagicalGirl.Hands`
- `Demo.Relic.ShiningMagicalGirl.Body`
- `Demo.Relic.ShiningMagicalGirl.Feet`
- `Demo.Relic.HeavenLiveRoom.PlanarSphere`
- `Demo.Relic.HeavenLiveRoom.LinkRope`

All item definitions are Unique. The four-piece set binds an existing attack
effect; the two-piece set binds an existing speed or attack effect. Do not add
a second threshold or a set-specific C++ branch.

## Quest, encounters and rewards

Create these definitions:

- `/Game/Data/VerticalSlice/Quests/DA_Quest_DomainEcho`
- `/Game/Data/VerticalSlice/Enemies/DA_Enemy_SupportSectionInspector`
- `/Game/Data/VerticalSlice/Enemies/DA_Boss_Laigushi`
- `/Game/Data/VerticalSlice/Encounters/DA_Encounter_SupportSectionInspector`
- `/Game/Data/VerticalSlice/Encounters/DA_Encounter_Laigushi`
- `/Game/Data/VerticalSlice/Rewards/DA_Reward_WangXiaYiTong`
- `/Game/Data/VerticalSlice/Rewards/DA_Reward_SupportSectionInspector`
- `/Game/Data/VerticalSlice/Rewards/DA_Reward_Laigushi`
- `/Game/Data/VerticalSlice/Rewards/DA_Reward_DomainEcho`

QuestId is `Demo.Quest.DomainEcho`; its intended display title is 界域回响.
The first two objective events may arrive in either order:

| ObjectiveId | EventId |
|---|---|
| `Demo.Objective.DomainEcho.SurveyObservationCar` | `Demo.Event.DomainEcho.SurveyObservationCar` |
| `Demo.Objective.DomainEcho.SurveySixthStreet` | `Demo.Event.DomainEcho.SurveySixthStreet` |
| `Demo.Objective.DomainEcho.OpenChest` | `Demo.Event.DomainEcho.OpenWangXiaYiTong` |
| `Demo.Objective.DomainEcho.DefeatInspector` | `Demo.Event.DomainEcho.DefeatSupportSectionInspector` |
| `Demo.Objective.DomainEcho.DefeatLaigushi` | `Demo.Event.DomainEcho.DefeatLaigushi` |

Rewards:

- WangXiaYiTong: HeavenLiveRoom PlanarSphere x1.
- Inspector first victory: HeavenLiveRoom LinkRope x1.
- Laigushi first victory: all four ShiningMagicalGirl pieces x1.
- DomainEcho completion: an existing character-growth stack item only.

Reward ClaimId remains the exactly-once authority. Quest and reward state
remain Save authorities; this catalog creates no parallel ledger.

## Editor acceptance

1. Create the exact assets, compile and Save All.
2. Close/reopen Editor and run `HSR.Demo.ContentCatalog`.
3. PIE both investigation orders and both travel directions.
4. Complete and repeat chest, Inspector, Laigushi and quest reward; confirm no
   second grant.
5. Save, exit and reload; confirm objectives, travel, encounters and receipts.
6. Record any Blueprint Runtime Error, Accessed None, ensure or invalid soft
   reference. Do not report PIE PASS until both routes complete.

## Known schema gaps

- Quest Definition/ViewModel expose `QuestId`, not a title. 界域回响 cannot
  appear in Quest UI until a separate presentation-field task is approved.
- Heal and Ultimate are mutually exclusive skill categories. Energy 100 on
  Verina's finisher is informative until a separate authority task supports it.
