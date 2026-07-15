---
name: phase-next-steps
description: Optional HSR phase-gate guidance for deciding what to do next in the UE5.6 C++ and GAS roadmap. Use when the user asks for the next project step, phase transition advice, readiness checks, or a best-practice checklist; this skill only advises and never starts implementation unless the user separately authorizes it.
---

# HSR Phase Next Steps

## Overview

Use this skill as an optional planning aid when HSR is moving between Phase 0-20. Identify the current phase from repository evidence, check its gate, recommend one adjacent small task, and state what the user must do in UE Editor or what Codex can change. Do not treat this skill as mandatory and do not override the user's current-turn scope.

## Workflow-based structure

This skill uses a workflow-based structure because phase transitions have ordered gates:

**1. Workflow-Based** (best for sequential processes)
- Works well when there are clear step-by-step procedures
- Example: DOCX skill with "Workflow Decision Tree" -> "Reading" -> "Creating" -> "Editing"
- Structure: ## Overview -> ## Workflow Decision Tree -> ## Step 1 -> ## Step 2...

**2. Task-Based** (best for tool collections)
- Works well when the skill offers different operations/capabilities
- Example: PDF skill with "Quick Start" -> "Merge PDFs" -> "Split PDFs" -> "Extract Text"
- Structure: ## Overview -> ## Quick Start -> ## Task Category 1 -> ## Task Category 2...

**3. Reference/Guidelines** (best for standards or specifications)
- Works well for brand guidelines, coding standards, or requirements
- Example: Brand styling with "Brand Guidelines" -> "Colors" -> "Typography" -> "Features"
- Structure: ## Overview -> ## Guidelines -> ## Specifications -> ## Usage...

**4. Capabilities-Based** (best for integrated systems)
- Works well when the skill provides multiple interrelated features
- Example: Product Management with "Core Capabilities" -> numbered capability list
- Structure: ## Overview -> ## Core Capabilities -> ### 1. Feature -> ### 2. Feature...

Patterns can be mixed and matched as needed. Most skills combine patterns (e.g., start with task-based, add workflow for complex operations).

The workflow above is guidance only; the safety and response rules below are normative.

## Safety and scope

Use these project-specific safety rules:
- Read the HSR rules and current evidence before making a recommendation.
- Keep the recommendation optional, adjacent to the current phase, and independently verifiable.
- Separate Codex changes, user Editor actions, and user-provided evidence.
- Never create code, assets, Config, or Git changes merely because this skill was invoked.
- Stop and request authorization for deletion, broad refactoring, third-party assets, Git, or a new module.

## Workflow

1. Determine the current phase and the last verified milestone.
2. Check prerequisites, unfinished risks, and documentation freshness.
3. Select the smallest next task that advances the phase without entering the next phase.
4. Separate responsibilities into Codex file work, user Editor work, and user verification.
5. List exact files, assets, manual steps, and evidence required.
6. State what is explicitly out of scope.
7. Offer a validation checklist and documentation entries to make after execution.
8. End with one suggested next task; do not execute it automatically.

## Required response format

1. Current phase and evidence.
2. Gate status: ready, blocked, or not yet verified.
3. One recommended next task.
4. Codex changes, if any, with exact paths.
5. User Editor actions.
6. Compile, PIE, automation, or manual evidence needed.
7. Documentation updates.
8. Risks and explicit non-goals.
9. Optional follow-up after the recommended task.

## Resources (optional)

This skill uses only the references directory; no scripts or assets are required.

### scripts/
Executable code (Python/Bash/etc.) that can be run directly to perform specific operations.

**Examples from other skills:**
- PDF skill: `fill_fillable_fields.py`, `extract_form_field_info.py` - utilities for PDF manipulation
- DOCX skill: `document.py`, `utilities.py` - Python modules for document processing

**Appropriate for:** Python scripts, shell scripts, or any executable code that performs automation, data processing, or specific operations.

**Note:** Scripts may be executed without loading into context, but can still be read by Codex for patching or environment adjustments.

### references/
Documentation and reference material intended to be loaded into context to inform Codex's process and thinking.

**Examples from other skills:**
- Product management: `communication.md`, `context_building.md` - detailed workflow guides
- BigQuery: API reference documentation and query examples
- Finance: Schema documentation, company policies

**Appropriate for:** In-depth documentation, API references, database schemas, comprehensive guides, or any detailed information that Codex should reference while working.

### assets/
Files not intended to be loaded into context, but rather used within the output Codex produces.

**Examples from other skills:**
- Brand styling: PowerPoint template files (.pptx), logo files
- Frontend builder: HTML/React boilerplate project directories
- Typography: Font files (.ttf, .woff2)

**Appropriate for:** Templates, boilerplate code, document templates, images, icons, fonts, or any files meant to be copied or used in the final output.

---

**Not every skill requires all three types of resources.**
