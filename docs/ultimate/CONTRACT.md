# Ultimate integration contract (2026-10-19)

Status: ENGINEERING READY / HUMAN ACCEPTANCE PENDING. Frozen contract integrated and verified; final evidence in HANDOFF.md.

## Ownership

| Owner | Exclusive writes |
|---|---|
| Root | `Battle/HSRBattleCoordinator*`, `HSRTurnManager*`, `HSRAbilityTypes.h`, integration, builds, runtime, delivery, practice GameMode, BattleStage/CombatMotion cosmetics |
| Agent A mechanics_audit | `docs/ultimate/REFERENCE-AUDIT.md` (production code read only) |
| Agent B gas_energy | `GAS/Ability/HSRUltimateAbility*`, `Data/HSRSkillDefinition.h`, new isolated effects/demo authoring |
| Agent C ui_presentation | `UI/HSRBattleCommandTypes.h`, `HSRBattleCommandWidget*`, `HSRBattleCommandViewModel*` |
| Agent D (after a slot frees) | new Ultimate tests and independent review; no production gameplay writes |

All writers preserve unrelated edits. UE processes, assets, builds and runtime are serialized by Root. Baseline: `cf73c5123cbbb0fbae73dfa8c4e370de93a339a1`, branch `codex/character-preview`; eight inherited VRM4U DLL changes captured under `Saved/Ultimate/Baseline/` with SHA256 and binary patch. No save-schema/equipment contract changes.

## Frozen interface and behavior

- Existing `SubmitBattleCommand(FHSRBattleActionCommand)` / `RequestAction` remain intent entry points. Ultimate commands carry their actual owner ActorParticipantId and confirmed target IDs.
- Existing Coordinator owns FIFO extra actions. GameThread accepts and assigns monotonically increasing uint64 sequence; ActionId is request/execution identity; BattleId plus generation guards stale callbacks. No new manager/ASC.
- Ordinary actions Reject while busy. Ultimate requests accept once per owner, reserve that owner's request slot (no speculative Energy deduction), queue while an atomic action runs, and revalidate before starting and at impact. No player withdrawal or silent retarget.
- Request accepted does not guarantee execution. Death, invalid target/spec/cost, battle finish/reset cancel without charging. Ability owns one CommitAbility cost; failure compensation stays in GAS. Successful settled effects survive later cosmetic interruption.
- A synchronous GAS rule transaction settles at impact. A bounded world timer completes presentation independently of animation notifies. Only after both phases can another ultimate run. No animation nesting. World pause pauses timers; reset invalidates generation and clears timers.
- Pending ordinary finalization is retained across the entire ultimate FIFO. Ultimate never calls ordinary ResolveAction. After FIFO, finalize the suspended ordinary action once, or return to the same selection. A dead current actor is skipped through a dedicated non-action turn-manager path.
- Base distance remains existing 10000/Speed; no elapsed animation time advances it. Existing GE-driven speed/advance/delay paths remain authoritative. Only normal lifecycle events decrement existing status turns.
- Definition additions: `UltimateReleaseThreshold` (0 means real static Cost), `bUltimateRestoresHealth`; predicates continue to select existing damage/heal paths. Final target types: SingleEnemy, SingleAlly, Self, AllEnemies, AllAllies; group commands confirm the exact full candidate set.
- UI DTO: `UltimateSkills: TArray<FHSRBattleCommandSkillView>`; skill adds `ActorParticipantId`, `bPending`, `bExecuting`, `AcceptedSequence:int64`; state adds `ExecutingUltimateActorId`, `SelectedUltimateActorId`. Coordinator fills business state, VM selection is presentation only. Normal Skills remain current normal actor.

## Verification boundary

Run current-round Editor/Game build, real world/ASC/GE automation including U01-U19, graphical player input and repeated battles, and necessary Cook/package. Record every failure and fix. HUMAN ACCEPTANCE remains pending until the user plays. A missing check is NOT RUN, never PASS.
