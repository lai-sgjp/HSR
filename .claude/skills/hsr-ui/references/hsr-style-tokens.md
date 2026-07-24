# HSR Battle UI Style Tokens

Token definitions for the HSR (Honkai: Star Rail) battle UI system. These tokens apply to UMG widget blueprints in `Content/UI/`. Adapted from the semi-design token organization pattern.

## Color Palette — HSR Battle

### Semantic Colors (Dark-Theme Native)

HSR battle UI is dark-theme-native: all panels use dark backgrounds with light text.

| Token | Hex | UMG Linear | Role |
|---|---|---|---|
| `--hsr-bg-primary` | `#0D1117` | (0.051, 0.067, 0.090, 1.0) | Deepest background behind translucent panels |
| `--hsr-bg-panel` | `#161B22` | (0.086, 0.106, 0.133, 1.0) | Panel/card backgrounds |
| `--hsr-bg-surface` | `#1A1A2E` | (0.102, 0.102, 0.180, 1.0) | Elevated surfaces, command panel |
| `--hsr-bg-overlay` | `#21262D` | (0.129, 0.149, 0.176, 1.0) | Modal/popover backgrounds |

### Accent Colors

| Token | Hex | UMG Linear | Role |
|---|---|---|---|
| `--hsr-accent-energy` | `#4FC3F7` | (0.310, 0.765, 0.969, 1.0) | Energy gauge fill, selected skill, active state |
| `--hsr-accent-hp` | `#66BB6A` | (0.400, 0.733, 0.416, 1.0) | HP bar fill, healing numbers |
| `--hsr-accent-damage` | `#EF5350` | (0.937, 0.325, 0.314, 1.0) | Damage numbers, danger states |
| `--hsr-accent-shield` | `#FFD54F` | (1.000, 0.835, 0.310, 1.0) | Shield/Toughness bar fill, break indicator |
| `--hsr-accent-sp` | `#AB47BC` | (0.671, 0.278, 0.737, 1.0) | Skill Point display, ultimate gauge |
| `--hsr-accent-selection` | `#42A5F5` | (0.259, 0.647, 0.961, 1.0) | Selected target highlight, focus ring |

### Element Colors (Weakness / Damage Type)

| Token | Hex | Element |
|---|---|---|
| `--hsr-element-physical` | `#B0BEC5` | Physical (grey-silver) |
| `--hsr-element-fire` | `#EF5350` | Fire (red) |
| `--hsr-element-ice` | `#81D4FA` | Ice (light blue) |
| `--hsr-element-lightning` | `#AB47BC` | Lightning (purple) |
| `--hsr-element-wind` | `#66BB6A` | Wind (green) |
| `--hsr-element-quantum` | `#5C6BC0` | Quantum (indigo) |
| `--hsr-element-imaginary` | `#FFD54F` | Imaginary (gold) |

### Text Colors

| Token | Hex | UMG Linear | Role |
|---|---|---|---|
| `--hsr-text-primary` | `#E6EDF3` | (0.902, 0.929, 0.953, 1.0) | Primary body text |
| `--hsr-text-secondary` | `#8B949E` | (0.545, 0.580, 0.620, 1.0) | Secondary labels, descriptions |
| `--hsr-text-dim` | `#484F58` | (0.282, 0.310, 0.345, 1.0) | Disabled text, placeholder |
| `--hsr-text-inverse` | `#0D1117` | (0.051, 0.067, 0.090, 1.0) | Text on light/white backgrounds |

### State Colors

| Token | Hex | Role |
|---|---|---|
| `--hsr-state-buff` | `#66BB6A` | Buff active indicator |
| `--hsr-state-debuff` | `#EF5350` | Debuff active indicator |
| `--hsr-state-disabled` | `#484F58` | Disabled button fill/text |
| `--hsr-state-pending` | `#FFD54F` | Command pending indicator |
| `--hsr-state-ready` | `#4FC3F7` | Action ready to submit |

### Translucent Overlays

HSR panels are translucent, not solid. These provide the base with alpha:

| Token | Color + Alpha | Use |
|---|---|---|
| `--hsr-panel-command` | `#1A1A2E` @ 85% | Command skill button panel |
| `--hsr-panel-status` | `#161B22` @ 80% | Participant status panel |
| `--hsr-panel-turn-order` | `#0D1117` @ 75% | Turn order display bar |
| `--hsr-panel-tooltip` | `#21262D` @ 90% | Hover tooltips, detail popups |

---

## Typography

### Font Stack (UMG Compatible)

```
Primary: "Roboto" (Google Fonts, OFL — embed in Content/Fonts/)
Fallback: system default sans-serif
CJK: "Noto Sans SC" for Chinese text
Mono: "Roboto Mono" for stat numbers, damage values
```

### Font Size Scale

Follows semi-design's named-level approach, adapted for game UI readability at 1080p:

| Level | Size (px) | UMG Equivalent | Use |
|---|---|---|---|
| `font-caption` | 10 | Font Size 10 | Tiny labels, stack counts, cooldown numbers |
| `font-body-small` | 12 | Font Size 12 | Body text, participant stats, buff/debuff descriptions |
| `font-body` | 14 | Font Size 14 | Standard body text, skill descriptions |
| `font-label` | 16 | Font Size 16 | Section labels, button text, skill names |
| `font-heading` | 20 | Font Size 20 | Panel titles, character names |
| `font-title` | 26 | Font Size 26 | Major headers |
| `font-display` | 32 | Font Size 32 | Damage numbers, ultimate names |

### Font Weight

| Weight | Family | Use |
|---|---|---|
| Regular (400) | Roboto | Body text, labels, descriptions |
| Medium (500) | Roboto | Navigation, section headers |
| Bold (700) | Roboto | Headings, damage numbers, ultimate names |

---

## Spacing Scale

Adapted from semi-design's named spacing scale for CanvasPanel offsets:

| Token | Pixels | Use |
|---|---|---|
| `space-none` | 0 | No gap |
| `space-tight` | 4 | Hairline separation: icon-to-label |
| `space-compact` | 8 | Tightly related elements: label+value pairs |
| `space-base` | 12 | Standard internal padding in compact panels |
| `space-comfortable` | 16 | Default widget padding, button internal |
| `space-loose` | 24 | Group separation, card internal padding |
| `space-section` | 32 | Section-to-section gap |
| `space-spacious` | 48 | Major panel separation |

### CanvasPanel Positioning Conventions

| Zone | Anchor | Offset | Use |
|---|---|---|---|
| Top bar | (0,0) → (1,0) | Y: 2-8 | Turn order, battle phase indicator |
| Top-left panel | (0,0) → (0,0) | X: 4, Y: 30 | Participant status |
| Top-right panel | (1,0) → (1,0) | X: -4, Y: 30 | Settings, auto-battle toggle |
| Center info | (0.5, 0) → (0.5, 0) | Y: 10 | Current turn indicator, SP display |
| Bottom-left panel | (0,1) → (0,1) | X: 4, Y: -4 | Current actor info |
| Bottom-right panel | (1,1) → (1,1) | X: -4, Y: -4 | Action buttons |
| Bottom-center panel | (0,1) → (1,1) | Y: -2 | Skill buttons, command panel |
| Full-width bottom | (0,1) → (1,1) | Y: -2 | Complete command panel |

---

## Shape Language

### Border Radius

| Token | Pixels | Use |
|---|---|---|
| `radius-none` | 0 | Sharp-edged panels, table cells |
| `radius-small` | 4 | Buttons, input fields, tags |
| `radius-medium` | 8 | Cards, skill buttons, status panels |
| `radius-large` | 12 | Modal dialogs, large panels |
| `radius-pill` | 9999 | HP/Energy bars, progress indicators |

### Panel Style

HSR panels use:
- **Dark translucent fill** with alpha 0.75-0.90
- **No visible border** — separation comes from color difference, not borders
- **Subtle inner glow** for elevated panels (optional, hard in UMG — skip for now)
- **Sharp corners** for battle UI, rounded for menu UI

### Bar Style (HP, Energy, Toughness)

- **Height**: 8-12px for status bars, 4-6px for turn order indicators
- **Fill direction**: Left-to-right
- **Background**: `--hsr-bg-primary` (darker than panel)
- **Fill**: Semantic color based on bar type
- **Animation**: Use UMG progress bar with smooth interpolation

---

## Component Templates

### Skill Button (from HSR battle UI)

```
Button (BTN_*)
├── Background: --hsr-bg-surface @ 85%, radius-medium (8px)
├── Size: ~120×60px
├── Normal state: dark surface fill
├── Hover state: lighter surface + accent border left (2px, --hsr-accent-energy)
├── Selected state: accent border left (2px, --hsr-accent-selection)
├── Disabled state: dimmed (--hsr-state-disabled fill)
└── Child:
    VerticalBox
    ├── TextBlock (skill name) — font-label, primary text
    └── TextBlock (cost label) — font-caption, secondary text
```

### Participant Status Panel

```
TextBlock (TXT_Participants)
├── Multi-line
├── AutoWrap: enabled
├── Font: font-body-small (12px)
├── Color: --hsr-text-primary
└── Format per line:
    "ParticipantId | HP xxx/xxx | Energy xx/xx | Toughness xx/xx"
```

### Turn Order Display

```
TextBlock (TXT_TurnOrder)
├── Single line, horizontal
├── Font: font-body (14px)
├── Color: --hsr-text-primary
├── Anchor: top, full width
└── Format:
    "Current: ActorId → Next: ActorId → ..."
```

---

## When to deviate from these tokens

- **Performance-critical HUD elements** (real-time update every frame): use simpler widgets, fewer translucency layers
- **Non-battle screens** (menus, inventory): use a different palette (lighter, more opaque) — define in a separate lock section
- **Accessibility**: increase font sizes and contrast ratios if targeting console/TV play at distance
- **Mobile/tablet**: scale up touch targets (minimum 44px for buttons)

## Sources

- Color palette inspired by HSR (Honkai: Star Rail) battle UI visual analysis
- Spacing scale adapted from semi-design's named token system
- Font scale adapted for game readability at 1080p resolution
