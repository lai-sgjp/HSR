# Style lock — HSR Battle UI

Established: 2026-07-24. Source: HSR (Honkai: Star Rail) battle UI reference + tastemaker skill adaptation + semi-design token structure.

## Palette

HSR battle UI is **dark-theme-native** — no light-mode companion needed.

- Background: `#0D1117` (role: deepest background behind translucent panels)
- Panel: `#161B22` (role: card/panel fill — participant status, turn order bar)
- Surface: `#1A1A2E` (role: elevated panels — command panel, skill buttons)
- Overlay: `#21262D` (role: modal, tooltip, detail popup)
- Primary accent: `#4FC3F7` (role: energy gauge, selected skill, active state, Cyan 300)
- HP: `#66BB6A` (role: health bar fill, healing text, Green 400)
- Damage: `#EF5350` (role: damage text, danger state, Red 400)
- Shield/Toughness: `#FFD54F` (role: toughness bar fill, break indicator, Amber 300)
- SP/Ultimate: `#AB47BC` (role: skill point display, ultimate gauge, Purple 400)
- Text primary: `#E6EDF3` — contrast vs background: ~15:1 (WCAG AAA pass)
- Text secondary: `#8B949E` — contrast vs background: ~5:1 (WCAG AA pass)
- Text dim: `#484F58` — decorative only, do not use for readable text
- Dark mode: native palette; no light companion needed for battle UI

## Color contract

Every color pairing introduced in a UMG widget must come from the legal pairings below.

- Text-safe (>=4.5:1 against background): text-primary/bg-primary, text-primary/bg-panel, text-primary/bg-surface, accent-energy/bg-primary, hp/bg-primary, damage/bg-primary, shield/bg-primary, sp/bg-primary
- UI-safe (>=3.0:1, non-text only): accent-energy/bg-surface as button fill, hp/bg-surface, shield/bg-surface
- Decorative (<3.0, never for text or state-carrying borders): text-dim with any background, sp/bg-panel

## Typography

- Font family: Roboto — primary and body; Roboto Mono for stat numbers
- Heading weight: Bold (700)
- Body weight: Regular (400)
- Label weight: Medium (500)
- Scale: caption 10px, body-small 12px, body 14px, label 16px, heading 20px, title 26px, display 32px
- Chinese/Korean: Noto Sans SC

## Shape language

- Panel radius: 8px (medium) for skill buttons and command panels
- HR/Energy bar radius: 9999px (pill) for progress bars
- Button corners: 4px (small) for execute/target buttons  
- Panel fill: translucent dark with alpha 0.80-0.90
- Border: no visible border; separation by color contrast
- Shadow: none for flat battle UI

## Density & spacing

- Base unit: 4px
- Section padding: space-section (32px) — major panel separation
- Content card internal padding: space-loose (24px) — command panel
- Compact card internal padding: space-base (12px) — participant status row
- Button internal padding: space-comfortable (16px)
- Skill button size: 120×60px minimum
- Overall density: compact, information-dense — this is a real-time battle UI, not a reading experience

## Positioning conventions

- Player status: top-left anchored
- Enemy status: top-right anchored
- Turn order: top center, full-width
- Skill buttons: bottom center, horizontal row
- Current actor info: bottom-left
- Execute button: bottom-right
- Target selector: beside skill buttons or above execute

## Mood descriptors

clean, confident, sci-fi, tactical

## Assets

- No external illustration assets needed — battle UI is text + bar + button native
- Icons: use Unreal built-in or skip — battle UI in HSR uses text labels primarily
- No photography for battle screens

## Motion

- HP/Energy bar fills: smooth interpolation (UMG Progress Bar built-in)
- Turn order change: instant (no animation — battle state updates)
- Skill selection highlight: immediate toggle
- Damage numbers: instant appearance, no entrance animation needed

## Do not

- No hardcoded hex values in UMG Blueprint graphs — use C++ bindings
- No Unreal default grey panel backgrounds — always set explicit dark translucent fill
- No arbitrary font sizes outside the scale
- No widgets with placeholder text that look like real data (confuses PIE testing)
- No deeply nested widget trees for status displays — keep CanvasPanel children flat
- No light-mode battle panels (HSR battle is dark throughout, and the contrast system is only verified for dark)
