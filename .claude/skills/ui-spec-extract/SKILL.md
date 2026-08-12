---
name: ui-spec-extract
description: Extract structured UI specifications from reference screenshots using a two-model pipeline (multimodal weak model for layout analysis + strong model for merging and color correction). Use when given a screenshot of any game UI, website, or app interface that needs to be recreated as a UMG widget blueprint. Triggers on "extract this UI", "study this screenshot", "turn this screenshot into a widget", "extract the spec", "analyze this layout".
---

# UI Spec Extraction Pipeline

## The problem this solves

You have a reference screenshot that needs to be turned into a UMG widget. The screenshot has colors that don't match your game's palette, a different resolution than your target canvas, and too many details for one model pass to extract cleanly.

The solution is a **three-stage pipeline** that splits the work by model capability:

```
Screenshot
    │
    ▼
┌─────────────────────────────┐
│ Python PIL:                  │
│  • Crop image into panels   │
│  • Sample colors per region │
└──────────┬──────────────────┘
           │  4 cropped panel images
           ▼
┌─────────────────────────────┐
│ Multimodal model (×4):      │
│  • One agent per panel      │
│  • Describe layout/controls │
│  • Write compact JSON files │
└──────────┬──────────────────┘
           │  4 desc_*.json files
           ▼
┌─────────────────────────────┐
│ Strong model (merge):       │
│  • Read all 4 JSONs         │
│  • Apply color correction   │
│  • Scale to target res      │
│  • Output comprehensive spec│
└──────────┬──────────────────┘
           │  spec.json
           ▼
    Ready for UMG generation
```

## Principles

| Principle | Why |
|---|---|
| **Split, don't bulk** | A full screenshot overwhelms model output tokens. Split into panels → each agent sees a clean, focused task. |
| **Weak model = eyes, not hands** | Multimodal model extracts structure (spacing, hierarchy, controls). Don't rely on it for exact colors or sizes. |
| **Colors come from reference, not screenshot** | Screenshot colors are often compressed / washed out / color-shifted. Override with known game palette. |
| **Ratios, not pixels** | Weak model outputs `width_ratio` / `margin_ratio`. Strong model multiplies by target resolution. |
| **File-based handoff** | Each desc_*.json is written to disk. No token limits, no session truncation. |
| **Repeat template for grids** | 10 identical cards = 1 card template + `repeat: 10`. Never enumerate duplicates. |

## Stage 0 — Setup

Before starting, determine:

1. **Target canvas size** — e.g. `1920×1080` or `1280×720`. All sizes scale from this.
2. **Color override palette** — the project's known color system (HSR palette in `hsr-style-tokens.md`)
3. **Screenshot directory** — where the reference images live

## Stage 1 — Crop and Sample (Python)

Use this Python script to crop the screenshot into panels and sample key colors.

```python
"""Crop reference screenshot into panels for analysis."""
from PIL import Image

img = Image.open("C:/path/to/screenshot.png")
w, h = img.size

# Estimate split positions — adjust these per screenshot
nav_end = 75                     # top nav bar bottom
bottom_start = h - 45            # bottom bar top
split_x = int(w * 0.64)          # left/right content split

crops = {
    "nav_bar":       (0, 0, w, nav_end),
    "left_list":     (0, nav_end, split_x, bottom_start),
    "right_detail":  (split_x, nav_end, w, bottom_start),
    "bottom_bar":    (0, bottom_start, w, h),
}

for name, (x1, y1, x2, y2) in crops.items():
    cropped = img.crop((x1, y1, x2, y2))
    cropped.save(f"C:/path/to/crop_{name}.png")
    print(f"  {name}: {x1},{y1}-{x2},{y2} → {cropped.size}")
```

Also sample 3-5 representative colors per panel to validate against the reference palette:
```python
samples = {
    "nav_bg": img.getpixel((w//2, 10)),
    "panel_bg": img.getpixel((50, 200)),
    "card_bg": img.getpixel((100, 300)),
    "text_bright": img.getpixel((200, 50)),
    "text_dim": img.getpixel((50, 180)),
}
for k, v in samples.items():
    print(f"  {k}: #{v[0]:02x}{v[1]:02x}{v[2]:02x}")
```

## Stage 2 — Panel Description (Multimodal model)

Launch **one agent per cropped panel image** in parallel. Each agent writes a compact JSON file to disk.

### Agent Prompt Template (per panel)

```markdown
Look at the image "[panel_path]" — this is the [PANEL_NAME] of [SCREEN_TYPE].

Write a structured JSON description to file "[output_path]".

The target canvas is [WIDTH]×[HEIGHT]. This panel spans:
- Layout: [horizontal|vertical|grid]
- Width ratio: [~0.XX] of total screen
- Height: [~YYY px]

Describe these elements precisely:
[list key elements to look for]

Output compact JSON format (keys shortened):
{"purpose":"","layout":"","w_ratio":0,"h":0,"bg":"",
 "ch":[{"type":"text|image|button|card|input|divider|chip|progress_bar|spacer|tab|stat_row|icon",
        "text":"","fs":0,"c":"","bg":"","w":0,"h":0,"mb":0,"radius":0,"active":false,
        "repeat":1}],
 "obs":[""]}
```

### Per-Panel Focus Areas

| Panel | Key elements to describe |
|---|---|
| **nav_bar** | Game title, sub-title, tab labels (which is active?), right-side icons, separator style, colors |
| **left_list** | Search bar (shape, placeholder text), filter chips (which active?), divider, card grid (columns × rows, card layout, thumbnail size) |
| **right_detail** | Image/artwork, title, rating/rarity info, stat rows (with colored bars if any), skill description, action buttons |
| **bottom_bar** | Player avatar, name/level, progress bar, currency icons + values, spacer behavior |

## Stage 3 — Merge and Correct (Strong model)

Read all `desc_*.json` files. Merge into a single comprehensive JSON with color correction and resolution scaling.

### Color Correction Reference

When screenshot colors are washed out/compressed, override with project palette:

| Screenshot appears as | Likely actual (HSR) |
|---|---|
| Near-white (#F0F0F0) background | #0D1117 |
| Light gray (#C0C0C0) panel | #161B22 |
| Washed gold (#FCE5B3) | #C89B3C |
| Washed purple (#D0A0E0) | #9B59B6 |
| Washed blue (#A0C0E0) | #4FC3F7 |
| Medium gray text (#808080) | #9AA4B5 |
| Near-white text (#F5F5F5) | #FFFFFF |

**Rule**: If the sampled color's RGB average > 180, the actual HSR color is likely dark (avg < 30). Apply the palette override.

### Resolution Scaling

```
target_w = 1920 (or 1280)
target_h = 1080 (or 720)

scale_x = target_w / screenshot_w
scale_y = target_h / screenshot_h

# Width ratios (w_ratio) stay as ratios
# Heights in pixels get multiplied by scale_y
# Font sizes get multiplied by min(scale_x, scale_y)
# Paddings/margins get multiplied by scale_x or scale_y
```

### Output Schema (merged)

```json
{
  "file_name": "source screenshot name",
  "canvas_size": "1920×1080",
  "overall_layout": "vertical_stack|horizontal_split|tab",

  "style_tokens": {
    "theme": "dark",
    "background_primary": "#RRGGBB",
    "background_secondary": "#RRGGBB",
    "text_primary": "#RRGGBB",
    "text_secondary": "#RRGGBB",
    "text_disabled": "#RRGGBB",
    "accent": "#RRGGBB",
    "accent_secondary": "#RRGGBB",
    "divider": "rgba(255,255,255,0.08)",
    "border": "#RRGGBB",
    "corner_radius_pill": 20,
    "corner_radius_card": 10,
    "corner_radius_button": 6
  },

  "panels": [
    {
      "purpose": "中文用途说明",
      "panel_type": "navigation|list|detail|stats|content_area",
      "layout": "horizontal|vertical",
      "width_ratio": 1.0,
      "height": 56,
      "background": "#RRGGBB",
      "padding_horizontal": 20,
      "children": [
        {
          "type": "text|image|button|card|input|divider|chip|progress_bar|spacer|tab_group|icon|stat_row|section_header|button_row|stats_section",
          "text": "精确文本内容",
          "font_size": 14,
          "color": "#RRGGBB",
          "background": "#RRGGBB|transparent",
          "width_ratio": 1.0,
          "height": 40,
          "corner_radius": 6,
          "margin_top": 0,
          "margin_bottom": 0,
          "margin_left": 0,
          "margin_right": 0,
          "icon": "icon_name|empty",
          "repeat": 1,
          "state": "normal|active|selected",
          "border": "border_spec|empty"
        }
      ],
      "sub_panels": {}  // for nested layouts (left_panel, right_panel)
    }
  ],

  "spacing": {
    "section_gap": 0,
    "row_gap": 8,
    "column_gap": 10
  },

  "interactive_elements": [
    {"type": "tab|input|chip|card|button", "text": "", "state": "", "action": ""}
  ],

  "observations": ["整体布局观察", "颜色特殊说明", "交互注意事项"]
}
```

## Node Types Reference

When writing the merged JSON, use these type values consistently:

| type | Purpose | Special fields |
|---|---|---|
| `text` | Labels, headings, values | `font_size`, `color`, `font_weight`, `alignment` |
| `tab_group` | Navigation tabs row | `children[tab]` with `active`, `underline` |
| `icon` | Icon/image glyph | `icon` (name), `icon_color` |
| `input_search` | Search/input box | `placeholder`, `border`, `icon` |
| `chip_row` | Filter chip bar | `chips[...]` with `active` state |
| `chip` | Individual filter pill | `background`, `radius` |
| `divider` | Horizontal separator | `height`, `background` |
| `card_grid` | Card grid layout | `columns`, `column_gap`, `row_gap` |
| `card` | List item/card | `state`, `border`, `corner_radius` |
| `image` | Image/artwork area | `border`, `border_width` |
| `stat_row` | Attribute row with bar | `label_color`, `sublabel`, `bar_color`, `bar_ratio` |
| `stats_section` | Group of stat rows | `rows[...]` |
| `section_header` | Section title | accent color |
| `progress_bar` | Horizontal progress | `fill_percent`, `fill_color` |
| `button` | Action button | `background`, `border` for outline |
| `button_row` | Button pair/trio | `buttons[...]`, `gap` |
| `spacer` | Flexible space | `flex: true` |
| `text_group` | Layout group for texts | `layout`, `children` |

## Workflow Summary

```
┌─ Receive screenshot ─────────────────────────┐
│  "Here's a reference, turn this into a widget" │
└──────────────────┬───────────────────────────┘
                   ▼
┌─ Step 1: Crop ───────────────────────────────┐
│  Python PIL crops into nav/left/right/bottom   │
│  Sample 10-15 color regions                    │
└──────────────────┬───────────────────────────┘
                   ▼
┌─ Step 2: Describe (parallel) ────────────────┐
│  Launch N agents (model=haiku or multimodal):  │
│  • Each gets one cropped panel                │
│  • Each writes desc_*.json to disk            │
│  • Output compact JSON only (no prose)        │
└──────────────────┬───────────────────────────┘
                   ▼
┌─ Step 3: Merge & Correct ────────────────────┐
│  Strong model (opus):                         │
│  • Read all desc_*.json                       │
│  • Override colors with project palette       │
│  • Scale dimensions to target resolution      │
│  • Write combined spec.json                   │
└──────────────────┬───────────────────────────┘
                   ▼
┌─ Step 4: Generate ───────────────────────────┐
│  Use spec.json → UMG widget creation           │
│  (see hsr-ui skill for UMG generation steps)   │
└──────────────────────────────────────────────┘
```

## Related skills

- **hsr-ui** — takes the spec.json output from this skill and generates UMG widgets, applies style tokens, binds events
- **dataviz** — for chart/dashboard UI extraction (follows similar split-describe-merge pattern)
