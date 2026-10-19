# Ultimate reference and baseline source audit

Date: 2026-10-19 (Asia/Shanghai). Owner: Codex Agent A, read-only production-source researcher. This is the **pre-change audit**, not final integrated verification. Root owns the final implementation and evidence status. No commercial-game source architecture is asserted here.

## Reference confidence and access

The requested public references were actually browsed this round:

- [HoYoWiki requested combat page](https://wiki.hoyolab.com/pc/hsr/entry/1618?lang=en-us): desktop and mobile versions returned an empty JavaScript shell. No substantive rule is marked official-verified from this page.
- [KQM beginner guide](https://hsr.keqingmains.com/misc/beginner-guide/): readable, especially Combat Screen / Action Order / Energy. Community gameplay documentation, not developer source.
- [KQM speed guide](https://hsr.keqingmains.com/misc/speed-guide/): readable, especially Action Gauge, Speed Buffs, Ultimates. Community mechanics analysis.
- [Prydwen combat guide](https://www.prydwen.gg/star-rail/guides/combat-and-exploration/): readable, Player Actions. Community corroboration of visible gameplay.
- [Ultimate](https://honkai-star-rail.fandom.com/wiki/Ultimate) and [Extra Turn](https://honkai-star-rail.fandom.com/wiki/Extra_Turn): direct reads failed with 402. Extra Turn's indexed search extract was available. It is secondary, partial evidence; its current character list and tooltip exceptions were not exhaustively checked.
- [Follow-Up Attack](https://honkai-star-rail.fandom.com/wiki/Follow-Up_Attack): indexed search extract describes follow-up actions above Ultimates / extra actions. This is a community boundary rule, not an official universal scheduling guarantee.

No live commercial-game battle was executed or video watched by Agent A. Agreement between readable guides verifies what those references describe; it does not independently prove every current patch or character exception.

## Reference / Implemented / Deliberate Difference

“Implemented” below describes the audited baseline. The integrated implementation is documented separately by Root.

| Rule / confidence | Reference | Implemented baseline evidence | Deliberate project contract / difference |
|---|---|---|---|
| Off-turn Ultimate and safe attack boundary; core behavior corroborated | KQM beginner: a requested Ultimate runs after the current attack finishes; Prydwen: off-turn use interrupts normal order | `UHSRBattleCoordinator::RequestActionCore` rejects `NotCurrentActor` before resolving skill category. `QueuePresentedAction` also requires `View.CurrentActorId` | Accept legal off-turn requests; wait for rule settlement and presentation completion; do not cancel an in-flight atomic attack |
| Own-turn Ultimate preserves normal action; core corroborated | Prydwen Player Actions describes selecting another action after Ultimate | Both formal damage and heal branches call `ResolveActionOrWait`; `FinishPresentedAction` ultimately calls `TurnManager::ResolveAction` | Ultimate completion must bypass normal turn finalization; resume original actor selection or deferred turn end |
| Multiple Ultimates can chain; core corroborated, exact FIFO is community boundary | Prydwen describes chaining; Fandom Extra Turn indexed extract describes trigger-order queue | Only one `PresentedAction`; different input while occupied is rejected. No Ultimate request arbiter found | Single GameThread acceptance sequence gives deterministic FIFO. Same-frame inputs follow actual acceptance events; this is the project's explicit determinism rule, not a claim about proprietary implementation |
| Ultimate energy is personal; core corroborated | KQM beginner separates Energy from shared Skill Points; Prydwen lists action, hit, kill and effect gains | ASC's `Energy` / `MaxEnergy` are authoritative. `UHSRUltimateAbility` uses Cost GE and `CommitAbility` | Do not reserve shared skill points as substitute for energy. Threshold may differ from energy cost and maximum |
| Resource types and thresholds; special-role extension | Beginner guides simplify standard Ultimates to charged Energy. Special resource characters are outside these generic guides | `IsValidUltimateDefinition` requires SingleEnemy, cost, refund and effect classes; no independent release threshold | Standard Energy only this round; configurable threshold / cost. No special character resource framework implied |
| No normal AV consumption; core corroborated | KQM speed distinguishes Ultimate from Action Order | `MakeBaseActionDistance` computes `10000 / max(Speed,1)`; `ResolveAction` recharges and advances | Never call `ResolveAction` solely because Ultimate finished; GE-induced speed, advance and delay still use existing authoritative entries |
| Speed changes preserve action progress; community formula corroborated | KQM speed gives `NewAV = CurrentAV * OldSPD / NewSPD` | `HandleSpeedChanged` rescales non-current participant remaining distance; current actor updates base only. `RequestActionDistanceAdjustment` is generation-aware and operation-idempotent | Preserve existing rules, including current-actor deferred advance/delay. Do not restore a saved AV array after Ultimate, which would overwrite legitimate effects |
| Buff lifetime and insertion timing; ordinary rule corroborated, special effects vary | KQM speed discusses using Ultimate before turn completion to retain buffs; Extra Turn index says status counters do not tick for extra turns | `UHSRStatusComponent::HandleTurnEnded` decreases only the status owner's turns with epoch and sequence deduplication | Existing statuses use owner ordinary-turn end. Extra actions emit no ordinary `TurnEnded`. Start-turn/action-specific/timed effects require explicit independent semantics if added later |
| Follow-up priority; community boundary | Fandom indexed Follow-Up and Extra Turn: follow-up actions above Ultimate / extra turn, above ordinary turns | Scoped `rg` of `Source/HSR` found no FollowUp / ExtraTurn / CounterAttack / ExtraAction implementations | P0 priority is current atomic action > pending Ultimate FIFO > pending ordinary finalization / next normal action. No new follow-up system; reserve a clear future safe-point priority hook |
| Ultimate cannot be freely withdrawn; user contract plus historical community reports | Requested task explicitly requires accepted requests to remain committed to intent; no accessible official current statement obtained | No accepted request lifecycle exists yet | No player cancel after acceptance. Death, lost target, removed spec, battle end and world teardown are system cancels with clear reason |
| Ultimate during another Ultimate; deliberate project policy | General chaining references do not establish every nested special-action input window | One global presented action rejects subsequent input | Accept other actors' legal requests; no nested execution; drain at completion boundary. Treat source-specific tooltip exceptions as unimplemented special rules |

## Baseline source audit and ownership recommendation

All paths are relative to `E:/work/unreal_projects/HSR`.

| Surface | Current implementation / real symbols | Gap | Modification owner |
|---|---|---|---|
| Ultimate input | `Source/HSR/UI/HSRBattleCommandWidget.cpp`: `HandleUltimateClicked -> SelectSkill`; `Source/HSR/Battle/HSRBattleCoordinatorPresentation.cpp`: `SubmitBattleCommand -> QueuePresentedAction` | One actor's commands; current-actor and busy gates also swallow Ultimate intent | UI Agent for Widget; Root for Coordinator |
| Energy / cost | `Source/HSR/GAS/Ability/HSRUltimateAbility.cpp`: `ConfigureFromSkillDefinition`, `GetPreActivationFailureReason`, `ActivateAbility`; `Source/HSR/GAS/Attribute/HSRCoreAttributeSet.cpp`: `PreAttributeChange`, `PostGameplayEffectExecute` | Cost already commits through GAS and failure can compensate via equal positive GE. No accepted-request reservation, independent threshold or support Ultimate. Need distinguish successful post-cast gain from failure refund | GAS Agent |
| AV / normal turns | `Source/HSR/Battle/HSRTurnManager.cpp`: `Initialize`, `ResolveAction`, `ApplyCurrentPendingAfterRecharge`, `AdvanceToNextValidTurn`, `HandleSpeedChanged`, `RequestActionDistanceAdjustmentInternal` | No extra-action model; invoking normal resolve for Ultimate emits TurnEnded, recharges AV and advances | Root only |
| GAS damage | `HSRBattleCoordinator.cpp`: `RequestActionCore` prepares formal request/spec, activates existing spec, processes toughness/break/status and defeat. `HSRGameplayAbilityBase.cpp`: `PrepareFormalDamage`, `ApplyPreparedFormalDamage` clears prepared payload after one use | Reuse transaction pipeline; distinguish execution authorization from normal action ownership. Audit stale per-activation diagnostics when adding early failure paths | Root coordinator / GAS Agent Ability files |
| Presentation safe point | `HSRBattleCoordinatorPresentation.cpp`: `QueuePresentedAction` schedules `.4s` windup, `ApplyPresentedAction` settles synchronously then schedules `1s` finish; `ResolveActionOrWait` defers normal resolution; `FinishPresentedAction` resumes turn. `HSRCombatMotionComponent.cpp`: dynamic montage and motion | Existing bounded timer avoids missing AnimNotify deadlock but lacks request/generation-specific completion guards. Must drain Ultimate before deferred ordinary finalization | Root presentation / UI Agent independent visuals |
| Buff / death | `Source/HSR/Status/HSRStatusComponent.cpp`: `BindToTurnManager`, `HandleTurnEnded`; coordinator `HandleHealthChanged`, `ResolveDefeat`, `Reset`, `CleanupBattleActors` | Ordinary end event is already idempotent. Must keep Ultimate out of this event; clear requests and timers with battle cleanup; revalidate dead queued actor/target | Root; preserve Status ownership unless explicit new requirement |
| ASC lifetime | `Source/HSR/Character/HSRCharacterBase.cpp`: constructor creates one `UHSRAbilitySystemComponent`; `InitAbilityActorInfo(this,this)` | Actor ASC is already correct single-player runtime authority; no new ASC or persistent pointer queue | Preserve existing owner |
| Tests | `Source/HSR/Tests/HSRCombatPatchTests.cpp`: action distance, lifecycle, speed binding and forecast suites; `HSRBattleCasualtyTurnTests.cpp`: partial defeat progression and replay; `HSRHealEnergyPresentationTests.cpp`: existing cost/refund fixtures; `HSRDemoContentCatalogTests.cpp`: authored Ultimate content contracts | No U01-U19 arbitration coverage identified in baseline; old tests must remain real regressions | Independent test/review Agent, then Root integrated run |

`CommitActionEnergyGain` currently changes the ASC Energy attribute via `ApplyModToAttribute` after successful formal damage. This is existing Core authority, not Widget authority, but the requested new energy-effect paths should use a coherent GE pipeline. Baseline scoped search did not establish existing hit/kill energy grants; those must be audited/implemented rather than claimed inherited.

## Recommended minimum internal contract

Keep the existing Coordinator as the only battle action authority and TurnManager as ordinary AV/lifecycle authority. A value request can hold BattleId / generation, ActionId as RequestId, stable participant ID, skill ID, confirmed target IDs, monotonic sequence and lifecycle state. Resolve all UObject/ASC references fresh at execution. One incomplete request per actor prevents repeated key presses from reserving or spending twice.

Accepting intent performs validation and logical reservation only. It must not call `CommitAbility`. At the safe point revalidate battle identity, alive actor, blocking tags, owned ability spec, configured threshold/cost and confirmed targets. Then run the existing Ability exactly once. The Ability remains responsible for actual GAS cost and effect. Invalid targets should cancel with a visible reason; do not silently substitute another target. Execution completion consumes the request before callbacks can re-enter. A failed candidate must not strand later candidates.

Suspended flow requires only an existing normal actor identity plus whether its atomic action still needs finalization; it must not contain an authoritative AV snapshot. Selection interruption returns to selection; completed-action interruption drains pending Ultimates and only then emits normal end once. If the suspended ordinary actor died, use a dedicated skip/finalize path so a dead actor cannot strand `ResolveAction`'s validity guard.

## High-value extreme cases for independent tests

1. B then C are accepted from one UE tick; completion callback fires twice. Cost and formal GE remain once per ID; FIFO remains B then C; original ordinary end remains exactly once.
2. A's one-turn damage buff is active. A ordinary hit resolves; B and C Ultimate resolve; A end removes the buff once. Add a same-owner Ultimate to verify A's own Ultimate can still use the pre-end buff.
3. B queues while enemy attack winds up. Damage kills B before safe point: cancel B without cost; continue C. Separately, B kills the next ordinary enemy and forecast skips it.
4. Confirm enemy X for queued B; C kills X first. B cancels instead of selecting enemy Y. Clear logical reservation and keep later valid requests moving.
5. During B damage settlement a Health delegate ends battle. Do not continue toughness/status work into cleaned arrays, schedule old finish into the next battle or refund an already successful effect.
6. Removed AbilitySpec, failing Cost, failing prepared effect and missing animation each leave no busy state. Commit failure spends nothing; post-commit effect failure uses one compensation; missing visuals do not suppress authoritative damage.
7. B Ultimate applies speed/advance/delay to A: assert existing new AV survives resumed flow and only the normal end performs recharge. Current-actor adjustments remain deferred per existing TurnManager contract.
8. System reset between `.4s` impact and finish, then begin another battle. Old timers/delegates fail generation checks. New battle counters begin clean without old request state.
9. Pause while B is queued / executing and resume. World timer pausing and explicit battle pause policy must be consistent; no polling or timeout based on real wall-clock should consume gameplay behind pause.
10. Existing ordinary skill repeat input stays Reject while Ultimate inputs stay accept-and-wait. The new queue must not enqueue Basic / Skill.

This report is research and static audit evidence. It does not claim compilation, automation, PIE, packaged play, user interaction or final acceptance passed.
