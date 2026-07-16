---
name: phase-next-steps
description: Optional HSR phase-gate guidance for deciding what to do next in the UE5.6 C++ and GAS roadmap. Use when the user asks for the next project step, phase transition advice, readiness checks, or a best-practice checklist; this skill only advises and never starts implementation unless the user separately authorizes it.
---

# HSR Phase Next Steps

## Overview

Use this skill as an optional planning aid when HSR is moving between Phase 0-20. Identify the current phase from repository evidence, check its gate, recommend one adjacent coherent vertical slice, and state what the user must do in UE Editor or what Codex can change. Do not treat this skill as mandatory and do not override the user's current-turn scope.

## Safety and scope

Use these project-specific safety rules:
- Read the HSR rules and current evidence before making a recommendation.
- Keep the recommendation optional, adjacent to the current phase, independently verifiable, and centered on one user-visible outcome.
- Prefer a coherent work package that can include tightly coupled C++, Editor configuration, PIE, and evidence. Do not split mechanically by class, file, or asset.
- Split only for different authors or permissions, dangerous Git/Config, third-party assets, new modules, external dependencies, independent failure/rollback boundaries, or scope beyond a low-level model's safe capacity.
- Separate Codex changes, user Editor actions, and user-provided evidence.
- Never create code, assets, Config, or Git changes merely because this skill was invoked.
- Stop and request authorization for deletion, broad refactoring, third-party assets, Git, or a new module.

## Workflow

1. Determine the current phase and the last verified milestone.
2. Check prerequisites, unfinished risks, and documentation freshness.
3. Select the smallest coherent vertical slice that advances the phase without entering the next phase.
4. Separate responsibilities into Codex file work, user Editor work, and user verification.
5. List exact files, assets, manual steps, and evidence required.
6. State what is explicitly out of scope.
7. Offer a validation checklist and documentation entries to make after execution.
8. End with one suggested next task; do not execute it automatically.

## Required response format

1. Current phase and evidence.
2. Gate status: ready, blocked, or not yet verified.
3. One recommended coherent vertical slice with one independently verifiable outcome.
4. Codex changes, if any, with exact paths.
5. User Editor actions.
6. Compile, PIE, automation, or manual evidence needed.
7. Documentation updates.
8. Risks and explicit non-goals.
9. Optional follow-up after the recommended task.

## Reference routing

Read [phase-workflow.md](references/phase-workflow.md) for the phase gate checklist and complete Phase 0-20 matrix. Read the project canonical document [phase-execution-workflow.md](../../../docs/phase-execution-workflow.md) when detailed rationale or phase-specific evidence is needed.
