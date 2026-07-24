# Audit — UMG Widget Review

Use this when the user points at an existing UMG widget and wants a critique — "audit this widget," "why does this feel cluttered," "review the command panel." This verb does NOT edit; it returns a ranked punch list.

## How to audit

1.  Open the widget blueprint and read its structure (`get_widget_blueprint_info`).
2.  For each widget in the tree, check:
    - Does it have a data source (BindWidgetOptional, RefreshDesignerControls call, or static label)?
    - Is its font size on the project's scale?
    - Is its color from the locked palette?
    - Is its positioning consistent with sibling widgets?
3.  Score against the anti-slop checklist (SKILL.md Step 4).
4.  Return findings ranked by severity:

## Severity levels

| Level | Meaning | Example |
|---|---|---|
| **Critical** | Broken at runtime — widget won't update, compile fails, binding missing | TextBlock showing dynamic HP without BindWidgetOptional UPROPERTY |
| **High** | Visually broken or confusing — wrong color, unreadable text, overlapping widgets | Damage text color on background with < 3:1 contrast |
| **Medium** | Style deviation from locked tokens — wrong font size, inconsistent spacing | Using 18px font when scale says 16px for labels |
| **Low** | Improvement opportunity — could be cleaner, but not broken | Widget could be renamed from TextBlock_37 to TXT_Something |

## Output format

```
## UMG Audit: <widget name>

### Critical
- [ ] <finding> — location: <widget name> — fix: <concrete action>

### High  
- [ ] <finding> — location: <widget name> — fix: <concrete action>

### Medium
- [ ] <finding> — location: <widget name> — fix: <concrete action>

### Low
- [ ] <finding> — location: <widget name> — fix: <concrete action>

### Summary
<one sentence: "3 critical, 2 high, 1 medium — fix critical before PIE testing">
```

## When the user says "fix it" after an audit

Hands off into the build workflow (SKILL.md Steps 1-5) — address critical items first, then high, then medium. Low items are optional unless the user specifically asks for them.
