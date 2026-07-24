# `.tastemaker/style-lock.md` format — HSR Edition

Adapted from tastemaker's style-lock format for Unreal Engine UMG projects.

```markdown
# Style lock — <project name>

Established: <date>. Source: <"HSR reference screenshots + tastemaker adaptation" | "user-specified palette" | "existing project style">

## Palette
- Background: #hex (role: e.g. "deepest background")
- Panel: #hex (role: e.g. "card/panel fill")
- Surface: #hex (role: e.g. "elevated panels, buttons")
- Overlay: #hex (role: e.g. "modal, tooltip")
- Primary accent: #hex (role: e.g. "energy, selection, active state")
- HP: #hex (role: e.g. "health bar fill")
- Damage: #hex (role: e.g. "damage numbers, danger")
- Shield/Toughness: #hex (role: e.g. "toughness bar fill")
- SP/Ultimate: #hex (role: e.g. "skill point, ultimate gauge")
- Text primary: #hex — contrast vs background: X.XX
- Text secondary: #hex — contrast vs background: X.XX
- Text dim: #hex — decorative only
- Dark mode: <"native — no light companion" | "dual mode — see companion below"> — if dual mode, list light palette here

## Color contract
Required ratios by pairing (same format as tastemaker parent):
- Text-safe (>=4.5): <list pairings>
- UI-safe (>=3.0 and <4.5): <list pairings>
- Decorative (<3.0): <list pairings that must never carry text or state>

## Typography
- Font: <name> — use Roboto for battle UI, Noto Sans SC for Chinese
- Heading weight: Bold (700)
- Body weight: Regular (400)
- Label weight: Medium (500)
- Scale: caption 10 / body-small 12 / body 14 / label 16 / heading 20 / title 26 / display 32

## Shape language
- Panel radius: <px> — 8px default for HSR battle
- Bar radius: <px> — 9999px pill default
- Button corners: <px>
- Panel fill: translucent dark, alpha 0.80-0.90
- Border usage: none for battle UI; separation by color contrast

## Density & spacing
- Base unit: 4px
- Overall density: <"compact" | "comfortable" | "spacious">
- Section padding: <token>
- Card internal padding: <token>
- Button internal padding: <token>
- Skill button size: <W×H px>

## Positioning conventions
- Anchor patterns used for CanvasPanel children
- Left/right/top/bottom anchoring conventions

## Mood descriptors
2-4 words, e.g. "clean, sci-fi, tactical"

## Do not
Project-specific prohibitions discovered during iteration.
```

Keep it factual and specific. Every line should be something a different game project might plausibly do differently — a palette for a fantasy RPG would differ from HSR's sci-fi blues, a cozy farming game would use warm tones and larger fonts, etc.
