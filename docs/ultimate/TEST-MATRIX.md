# Ultimate test matrix

Final battle/UI/content runs: PASS. Graphical interaction independently captured. Final package verification is tracked in HANDOFF.md; human acceptance remains pending.

| ID | Real test group / assertion | Current final status |
|---|---|---|
| U01 | RequestContract: insufficient Energy rejects, no spend/effect | PASS — Automation03 |
| U02 | RequestContract: own normal selection survives Ultimate | PASS — Automation03 |
| U03 | RequestContract: different owner's normal selection survives | PASS — Automation03 |
| U04 | EnemyBoundary / AtomicFIFO: full original impact before extra action | PASS — Automation03 |
| U05 | AtomicFIFO: two actors execute in accepted order | PASS — Automation03 |
| U06 | AtomicFIFO: two submissions in one engine frame receive ordered sequences | PASS — Automation03 |
| U07 | RequestContract / ReentrantSettlement: duplicate owner/id has one Cost and effect | PASS — Automation03 |
| U08 | AtomicFIFO: C submitted during B waits until B resolves | PASS — Automation03 |
| U09 | Invalidation: queued actor dies, no debit | PASS — Automation03 |
| U10 | Invalidation / AOE: confirmed target/set dies, cancel with no silent retarget | PASS — Automation03 |
| U11 | EnemyBoundary / AOE: killed future actor skipped, all casualties registered | PASS — Automation03 |
| U12 | AuthoritativeAV: real Speed GE and official advance operation survive extra action | PASS — Automation03 |
| U13 | RequestContract / AuthoritativeAV: no ordinary recharge, time advance or synthetic turn | PASS — Automation03 |
| U14 | AtomicFIFO: real one-turn Infinite GE survives chain, decrements at original TurnEnded | PASS — Automation03 |
| U15 | Failures / CostTriggeredEnergy / ReentrantSettlement / AtomicDelegateReset / revalidation | PASS — Automation03 |
| U16 | Generation: team wipe clears queue, no resource debit for unexecuted requests | PASS — Automation03 |
| U17 | Generation / AtomicDelegateReset: rebuild, old timer and synchronous teardown | PASS — Automation03 |
| U18 | HeadlessOrdinaryRegression / CasualtyTurn: actual Basic, Skill, AI and control restore | PASS — Automation03 |
| U19 | Stress.U19_20Battles: 20 generations, rapid repeated inputs, enemy interleave, kill/gain | PASS — Automation03 |

Additional real tests: AOE.AllEnemiesCostAndCasualties, TeamHeal.AllAlliesCostOnce, SupportAndThreshold (max200 / threshold120 / cost100), PauseResume, and four HSR.UI.Ultimate tests. UI automation validates DTO/VM/intent contracts; physical input and graphical animation require separate runtime evidence.

## Preserved first failures and corrections

- `Saved/Ultimate/editor-build-01.log`, `editor-build-02.log`: Exit999, sandbox prevented the batch directory checks; scoped authorized execution reached actual UBT/UHT. The first invocation also lacked the correct working directory. No source workaround or toolchain replacement was used.
- `editor-build-06.log`: Exit6, C++ auto pointer deduction from TObjectPtr in the newly added practice roster; corrected to a const reference. Subsequent build07/build08 succeeded.
- `automation-01.log` / `Automation01/index.json`: Exit255; 6/9 groups succeeded. Buff fixture had an invalid status ID, enemy-animation fixture had not actually started an enemy presentation and assumed one random recipient, pressure energy assertions omitted legitimate kill gains. Production sequence assertions were retained; fixtures now drive the real enemy animation and count separate negative Cost and positive gains.
- `automation-02.log` / `Automation02/index.json`: 15/16 new groups succeeded; real stress 20/20. Headless fixture tried a Skill with SP0; corrected to Basic-gain then Skill-spend. Existing EnemyCatalog test assumed a historical 2-entry list, but current catalog has Inspector/Patrol/Boss; now asserts all three stable IDs. Existing casualty test assumed uniform AI hits its named leader in two draws; private enemy definition now enables wounded-priority, then verifies actual surviving-team damage after the leader dies.
- Actual runtime diagnostic issue: Automation02 showed real HP loss while old EffectContext-derived presentation read zero. ApplyOnePreparedFormalDamage now uses the authoritative before/after HP delta for that target's AppliedDamage, preserving other diagnostic fields. This is a code fix, not a relaxed test.
- Independent review found a headless pump guard scope issue, ordinary impact reentry, resource net-delta Cost misclassification, synchronous delegate Reset, and mixed input-entry busy checks. Dedicated real ASC tests cover these paths; final results belong below.

## Final evidence

- Editor final code build: `editor-build-10.log` Exit0; final test-only build `editor-build-11.log` Exit0.
- `Automation03/index.json`: 45/45, 41 Success + 4 SuccessWithWarnings, zero failures/not-run; process Exit0. Includes 20 Ultimate groups and U19 actual 20/20 battles.
- `UIAutomation01/index.json`: 81/81, 71 Success + 10 SuccessWithWarnings, zero failures/not-run; Exit0. Four new Ultimate owner/input/group/locking tests included.
- `DemoAutomation02/index.json`: 4/4 Success, zero warnings/failures/not-run; Exit0. First Demo run failed due to stale Remiel EnergyGain, battle map and chest contents expectations. Tests now assert existing authored values exactly; production rewards/save were not edited.
- `Runtime/runtime-verification.json` + `practice-02.log`: two real graphical battles, six accepted/completed requests, exactly one Cost/effect each, enemy impact precedes queued Ultimate, own-selection and post-normal-finalization recovery. Physical inputs and GIFs under Runtime.
- First Windows package UAT Exit0, but actual startup exposed missing string-loaded bootstrap assets. `packaged-practice-03.log` preserves the original errors. Added only three Cook directories (Items/Drops/Rewards), keeping save schema/contracts untouched; final package/runtime follow-up recorded in HANDOFF.
- Graphical evidence and generated reports live under `Saved/Ultimate/`, deliberately excluded from Git. Human acceptance remains pending until the user plays.
