# HSR Phase Workflow Reference

This reference is an optional checklist for `$phase-next-steps`. The detailed project source is [`docs/phase-execution-workflow.md`](../../../docs/phase-execution-workflow.md).

## Universal phase loop

1. Read `agents.md`, `todo_plan.md`, `worklog.md`, `README.md`, the roadmap, and the current phase document.
2. Define one independently verifiable, coherent vertical outcome; list exact allowed files and explicit non-goals.
3. Describe ownership, data flow, Editor manual work, risks, and evidence before editing.
4. Check UE reflection, GC, Tick, GAS, UI, DataAsset/Runtime/SaveGame, copyright, and future-networking boundaries as applicable.
5. Implement the smallest coherent vertical slice: tightly related C++, Blueprint configuration, Editor work, PIE, and evidence may share one work package while retaining separate author handoffs and commits.
6. Tell the user exactly what to create or bind in UE Editor.
7. Compile the relevant target, run the smallest automated or PIE path, and test one failure path.
8. Update `worklog.md` with evidence, `todo_plan.md` only for real progress, and learning/design docs for reusable knowledge.
9. Review the gate before recommending the next adjacent task. Planning is not implementation evidence.

## Decision rules

- If prerequisites are missing, recommend the missing prerequisite rather than jumping ahead.
- Default to 2–5 engineering work packages plus one closeout package per Phase, adjusted to real complexity rather than forced to a quota.
- Do not split mechanically by class, file, or asset. Split for different authors/permissions, dangerous Git/Config, third-party assets, new modules, external dependencies, independent failure/rollback boundaries, or scope beyond a low-level model's safe capacity.
- If the next task needs files outside the allowlist, stop and request authorization.
- If an API or Editor behavior is uncertain, inspect UE5.6 evidence or ask for the error/configuration; do not invent an API.
- If the task is optional polish, present it as optional and protect MVP and phase gates.
- If the user asks for implementation, switch to the applicable low-level task template and preserve the same gate.

## Phase 0–20 quick matrix

| Phase | Gate focus | Next-step question |
|---|---|---|
| 0 | Blank C++ project, toolchain, modules, plugins, tags and docs baseline | Is Development Editor and empty PIE actually verified? |
| 1 | Character, Controller, Enhanced Input, camera and exploration HUD | Does movement work without duplicated bindings or polling? |
| 2 | ASC, AttributeSet, Actor Info, initialization GE and delegates | Can an attribute change update UI and survive respawn? |
| 3 | Interaction interface/component and one graybox object | Does one interaction use the shared path without Tick scanning? |
| 4 | Encounter definition, exploration enemy and stable transition context | Can an encounter cross maps without an Actor pointer? |
| 5 | Battle Map, participant rebuild, TurnManager and 1v1 loop | Can victory and defeat return exactly once? |
| 6 | Ability, target, Cost, Energy, team resource and resolution | Does an invalid action leave resources and turn state unchanged? |
| 7 | Attribute captures, ExecutionCalculation and damage breakdown | Can a seeded result be explained and reproduced? |
| 8 | Weakness, Toughness, Break and Turn Delay | Is queue delay owned by TurnSystem rather than Ability or Cue? |
| 9 | Status runtime, GE handles, stacking, immunity and turn triggers | Can a status expire by turn count rather than seconds? |
| 10 | Event-driven battle UI and screen lifecycle | Does UI only read state and submit commands? |
| 11 | Character definitions, progression, party and minimal Save | Can load rebuild attributes from saved IDs? |
| 12 | Equipment/relic instances and source-tracked Infinite GE | Can unequip remove only its own effects? |
| 13 | Inventory, reward transactions and idempotent drops | Can a reward transaction be replayed safely? |
| 14 | Quest/dialogue definitions, typed events and reward claims | Can one branching quest complete without duplicate reward? |
| 15 | Map definitions, teleport and return contexts | Can invalid travel fail without corrupting map state? |
| 16 | Versioned SaveGame, migration, backup and recovery | Can a save round-trip and reject missing definitions? |
| 17 | UI screen stack, input mode and focus restoration | Can screens restore input after travel? |
| 18 | Authorized presentation assets, cues, animation, audio and VFX | Can presentation be absent without changing rules? |
| 19 | Debug snapshots, automation and reproducible diagnostics | Can a bug be diagnosed from evidence rather than guesses? |
| 20 | Vertical slice, clean-save demo, performance and portfolio evidence | Can a new user follow the demo and understand contribution? |

## Optional status language

- **Ready**: prerequisites and evidence exist; one small next task may be proposed.
- **Blocked**: a concrete dependency or user decision prevents progress.
- **Not verified**: work may exist, but accepted compile/PIE/Editor evidence is missing.
- **Optional**: useful polish or learning work that must not be treated as a gate.
