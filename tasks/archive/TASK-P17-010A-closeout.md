# TASK-P17-010A - Data-driven Challenge Directory

Status: `PASS / USER ACCEPTED`

The accepted directory slice projects configured encounter definitions into a
deterministic read-only list, exposes stable encounter/enemy/map IDs, filters
locked/duplicate/unknown/invalid entries, and delegates template construction
to BattleTransition. User PIE evidence: `HSR ChallengeDirectory initialized
Sources=3 Entries=2 Result=0`; normal display, selection, Enter, and locked
entry behavior were accepted with no Blueprint Runtime Error, Accessed None,
or UMG ensure.

The follow-up failure matrix and formal P17-010 closeout are intentionally
separate in `TASK-P17-010B`; this file does not authorize further 010A work.
