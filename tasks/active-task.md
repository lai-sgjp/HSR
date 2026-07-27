# TASK-P16-005 — Full-authority Cold Recovery Slice

Status: `COMPLETE / AGENT VERIFIED / INDEPENDENT REVIEW PASS`

Outcome: independent UE Editor commandlet processes prove a distinguishable S1/S2 snapshot across Profile, Party, Equipment/Relic, Inventory, Reward ledger, Quest, and Map. A fresh process restores valid Primary S2; after controlled corruption of only the dedicated temporary Primary, another fresh process restores Backup S1; repeated Load remains no-op; final cleanup removes Primary/Staging/Backup.

Allowed files: Phase 16 Save files and tests, minimal existing domain test fixtures/seams, `Source/HSR/Tests/HSRSaveColdRecoveryTests.cpp`, `docs/save-system-design.md`, and task evidence files.

Non-goals: production Content/Config/UI/Gameplay changes, new authorities for chest/enemy, arbitrary user save corruption, new modules/dependencies, packaged/Shipping/cross-platform/cloud/multiplayer verification, atomic rename or real power-loss guarantees.

Acceptance: five independently launched test processes (`Seed`, `VerifyPrimary`, `CorruptPrimary`, `VerifyBackup`, `Cleanup`) pass in order; S1/S2 are distinguishable across every existing authority; SaveId/generation/source/reasons are correct; Reward/Equipment/RestoreTx/delegates do not duplicate on repeated Load; Map Load never travels; cleanup proves all physical roles absent; fresh build, full `HSR.Save`, and independent review pass. User-operated graphical Editor evidence remains separately labeled if not executed.
