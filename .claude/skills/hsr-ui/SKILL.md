---
name: hsr-ui
description: Generate and improve Unreal Engine UMG battle UI screens matching the HSR (Honkai: Star Rail) aesthetic — use whenever the user asks to build, design, style, improve, or audit a battle UI widget, command panel, status display, turn order HUD, or any UMG screen in Content/UI/. Triggers on "design battle UI", "make this look like HSR", "style the widget", "audit the UI", "add a display for X", "improve the command panel", or when building a new widget from scratch. Also triggers on "study"/"extract the look of" a reference screenshot, and "audit"/"review"/"why does this look off" for critiquing existing UMG widgets.
---

# HSR UI — Battle UI Skill for Unreal Engine

## The problem this solves

UMG widgets in Unreal Engine game projects tend to accumulate ad-hoc styling — hardcoded font sizes, arbitrary offsets, inconsistent colors — because there's no persistent design system to reach for. Each widget author picks what looks about right, and the result drifts across screens and sessions.

HSR UI works on three principles adapted from tastemaker:

1. **Ground in real game references.** HSR (Honkai: Star Rail) has a distinctive, consistent battle UI language — dark translucent panels, cyan/blue energy accents, compact information density, left-aligned player info and right-aligned action buttons. Extract and formalize these patterns into tokens.
2. **Lock once, reuse every screen.** Once the project's `.tastemaker/style-lock.md` is established, every UMG widget build reads from it — no re-deriving colors, fonts, or positioning conventions.
3. **Scope to what's actually being built.** A battle command panel needs different tokens than an exploration HUD — scope the effort to the screen type being built.

This skill works with the Unreal MCP Python toolchain (`mcp__unreal-mcpython__umg`, etc.) to actually manipulate widget blueprints in the running Editor, not just generate code.

## Modes

| Mode | When | What it does |
|---|---|---|
| **build** *(default)* | User asks to design, build, style, or improve a UMG widget. | The Workflow below (Steps 0-5). |
| **study** | User pastes a screenshot of HSR battle UI (or any reference game UI) — "study this," "what makes this layout work," "match this vibe." | Extract the reusable tokens (color palette, spacing conventions, font hierarchy, panel style) into a diagnosis. Load `references/verbs/study.md` first. |
| **audit** | User points at an existing UMG widget and wants a critique — "audit this widget," "why does this feel cluttered," "review the command panel." | Score against the anti-slop checklist, return a ranked punch list. Does NOT edit. Load `references/verbs/audit.md` first. |

## Workflow

### Step 0 — Load project style memory

Check for `.tastemaker/style-lock.md` in the project root:

- **Exists** → Read it. Reuse those exact tokens for the new widget. Do not re-derive colors or spacing.
- **Doesn't exist** → Cold start. Go to Step 1.

### Step 1 — Determine screen type and scope

Identify what kind of UMG screen is being built:

| Screen type | Characteristics | Example |
|---|---|---|
| **Battle Command Panel** | Bottom-of-screen, skill buttons, target selection, status text | `WBP_BattleCommandPanel` |
| **Battle HUD** | Persistent overlay, HP/Energy bars, turn order, skill cooldowns | `WBP_BattleHUD` |
| **Status Display** | Participant stats panel, buff/debuff icons, weakness type icons | `WBP_StatusPanel` |
| **Exploration HUD** | Minimap, quest tracker, interact prompts, character info | `WBP_ExplorationHUD` |
| **Menu/Dialog** | Full-screen overlay, settings, inventory, character sheet | `WBP_CharacterMenu` |

Scope the design tokens to the screen type — a battle command panel needs different spacing than a menu screen.

### Step 2 — Establish HSR battle UI tokens (cold start only)

Read `references/hsr-style-tokens.md` and apply the HSR battle color palette and conventions:

- **Panel background**: Dark translucent (`#1A1A2E` at ~80% opacity with blur)
- **Primary accent**: Cyan-blue (`#4FC3F7` for energy, selection, active state)
- **Text**: White primary, grey secondary
- **HP**: Red (`#EF5350`)  
- **Shield/Toughness**: Gold-yellow (`#FFD54F`)
- **SP (Skill Points)**: Purple (`#AB47BC`)
- **Font**: Roboto or system default, 12-16px range for battle info

Write the tokens to `.tastemaker/style-lock.md` in the format from `references/style-lock-format.md`.

### Step 3 — Build UMG widget via MCP

Use the Unreal MCP Python tools to:
1. Open the widget blueprint (`open_editor_for_asset`)
2. Add/remove widgets (`add_widget`, `remove_widget`)
3. Set layout positions (`set_slot_layout` with CanvasPanel anchors)
4. Set text styles (`set_text_style` for font size/color/outline)
5. Bind widget events (`bind_widget_event`)
6. Compile and save (`compile_widget_blueprint`, `save_asset`)

Apply tokens from `.tastemaker/style-lock.md` — never hardcode arbitrary values.

**UMG layout conventions (HSR-style):**
- Anchor to edges, not center, for battle UI elements
- Left-anchored: participant info panels
- Right-anchored: action buttons
- Bottom-anchored: command panel container
- Top-anchored: turn order display
- Use Offset (not Size) for responsive positioning within anchors

**Six non-negotiable build rules:**

1. **Every widget must have a bind target.** TextBlocks without BindWidget or programmatic SetText are dead widgets — they never update during battle. Every display TextBlock must either have a matching `UPROPERTY(meta=(BindWidgetOptional))` in the C++ Widget class, or be set in `RefreshDesignerControls()`.

2. **No hardcoded display text in battle widgets.** Placeholder text in the Designer is fine; shipped text that never changes (like "Turn Order" as a label) is fine. But HP values, energy counts, skill names — these must come from the ViewModel.

3. **Font sizes follow the scale.** Per `references/hsr-style-tokens.md`'s font scale: body = 12px, label = 14px, heading = 16px, title = 20px. No arbitrary sizes.

4. **Colors follow the palette.** Every color used in a widget should come from the locked palette. Hardcoded colors in UMG that weren't in the lock are a flag.

5. **Spacing follows anchoring, not arbitrary offsets.** CanvasPanel positioning should use consistent anchor patterns — widgets of the same logical group should share the same anchor. Don't position two sibling text blocks with completely different anchor settings.

6. **Compile and save always.** After any widget modification, run `compile_widget_blueprint` and `save_asset`. Uncompiled changes produce silent breakage in PIE.

### Step 4 — Anti-slop checklist for UMG

Run through these checks before considering the build done:

#### Structure
1. **No orphaned widgets.** Every widget in the tree has a purpose — no copy-pasted TextBlocks with placeholder text that were never wired up.
2. **Hierarchy is flat.** Battle command panels should not be deeply nested. CanvasPanel → direct children is preferred over CanvasPanel → VerticalBox → HorizontalBox → Border → TextBlock for status display.
3. **Widget naming is consistent.** Use `TXT_` prefix for TextBlocks, `BTN_` for Buttons, `CB_` for ComboBoxes, `IMG_` for Images.

#### Visual
4. **No default Unreal grey backgrounds.** Unreal's default panel background is solid grey — HSR uses dark translucent panels. Set `Background Color` with alpha < 1.0.
5. **Text contrast against backgrounds.** White text on the dark HSR panels must have enough contrast. Test with Health/Damage red text against dark backgrounds.
6. **No overlapping widgets.** CanvasPanel children with overlapping bounding boxes must be intentional, not accidental.

#### Functional
7. **Every display field has a data source.** Each TextBlock showing dynamic data must trace back to a `BindWidgetOptional` C++ member or be set in `RefreshDesignerControls()`.
8. **Buttons have event handlers.** Every Button in the widget tree must have a bound OnClicked handler (C++ `AddDynamic` in `BindDesignerEvents()`).
9. **Compile succeeds with no warnings.** Run `compile_widget_blueprint` and check the result — UpToDate or success only.

### Step 5 — Curate and remember

- **Interactive session**: Ask a quick keep/reject question about the layout: "keep this positioning, or adjust?"
- **Log the decision** to `.tastemaker/decisions.log` with status: kept/rejected + reason
- **Periodically promote** durable preferences (e.g. "prefers cyan-blue energy accent over golden") to `~/.tastemaker/profile.md`

## Reference files

| File | Read when |
|---|---|
| `references/hsr-style-tokens.md` | Cold start — HSR battle UI color palette, font scale, spacing conventions |
| `references/style-lock-format.md` | Writing or updating `.tastemaker/style-lock.md` |

## A note on this skill

This skill adapts tastemaker's web-UI design-token philosophy for Unreal Engine UMG widget blueprints. The core insight is the same: a locked, reusable, project-specific design system prevents widget drift. But the implementation surface is different — UMG CanvasPanel anchors instead of CSS flexbox, MCP Python commands instead of direct HTML/CSS generation, and game-specific concerns (real-time updates, C++ ViewModel binding, PIE testing) instead of responsive web design.

When a generated widget doesn't look right in PIE, the fix is usually one of: (a) a missing BindWidgetOptional member, (b) a RefreshDesignerControls call that was never added, or (c) a C++ function that returns FText::GetEmpty() because the ViewModel hasn't populated that field yet. Check these before tweaking layout.
