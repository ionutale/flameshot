# UI Color & Visual Refinement

## Summary

Refine Flameshot's default visual appearance with softer purple tones and
macOS-inspired polish. Keep the existing two-color theme system
(`uiColor` + `contrastUiColor`) but improve defaults and add subtle visual
treatments to buttons, panels, and selection borders.

## Motivation

The current default purple (`#740096`) is harsh and gives the UI a dated
look. The side panel and utility panel don't use the theme at all —
they render with the system palette, creating a visual disconnect from the
capture overlay. Buttons and selection borders are functional but lack
polish.

## Changes

### 1. Default Colors

| Token | Current | New |
|-------|---------|-----|
| `uiColor` | `#740096` | `#9B59B6` |
| `contrastUiColor` | `#270032` | `#6C3483` |

Only the defaults in `ConfigHandler` change. Users who have previously
customized their colors keep their existing settings.

### 2. Button Styling — `CaptureButton`

- Fill uses the new `uiColor` (solid, no gradient, no inner glow)
- Add a `1px` solid border: `rgba(contrastUiColor, 0.15)`
- Shadow: `0 2px 6px rgba(contrastUiColor, 0.3)` (more defined than current
  `0 2px 4px rgba(0,0,0,0.2)`)
- Hover background: use `contrastUiColor` directly (via
  `ColorUtils::contrastColor` is unchanged — the new contrastUiColor is what
  makes the difference)
- Only the `CaptureButton::styleSheet()` method needs changes (adds border
  and richer shadow); structural code is untouched

### 3. Panel Styling — `SidePanelWidget` & `UtilityPanel`

Both panels currently render with the system palette, disconnected from the
capture theme. Changes:

- Panel background: use `#F5F0FF` (lilac-tinted white) — applied via
  `setStyleSheet` on the panel container
- Panel accent / hover / section dividers: use `#EDE4F5` (soft lilac)
- Section labels use `contrastUiColor` for text color
- These are **static** (not tied to the user-configurable uiColor) to keep
  the panel readable regardless of the user's accent color choice

This requires:
- Adding stylesheet methods to `SidePanelWidget` and `UtilityPanel`
- The `ColorGrabWidget` (full-screen color picker) should remain unthemed
  (it needs to show true colors)

### 4. Selection Border — `SelectionWidget`

- Border: thin white (`#FFFFFF`, 2px width) instead of purple
- Outer glow: a `QGraphicsDropShadowEffect` with `uiColor` as the shadow
  color (blur radius 8, offset 0) — or equivalent QPainter approach
- Handles (drag circles): keep using `uiColor` (they already do)

This gives a clean, precise selection look while keeping the purple accent
on the handles.

### 5. Other Elements (unchanged)

- Overlay: stays as-is (solid black at `contrastOpacity`)
- XYWH geometry display: uses new uiColor at alpha=200 (no structural change)
- Grid: uses new uiColor at alpha=100 (no structural change)
- NotifierBox: uses new uiColor at alpha=180 (no structural change)
- Magnifier crosshair: uses new uiColor at alpha=130 (no structural change)
- Pin widget: uses new uiColor / contrastUiColor (no structural change)
- Icon system: unchanged (auto white/black based on `colorIsDark`)
- Color picker presets: unchanged

## Files to Modify

| File | Change |
|------|--------|
| `src/utils/confighandler.cpp` | Update default color values (lines ~110-111) |
| `src/widgets/capture/capturebutton.cpp` | Add border + refined shadow in `styleSheet()` |
| `src/widgets/capture/capturebutton.h` | Add `m_buttonBorder` / `QColor borderColor` if needed |
| `src/widgets/panel/sidepanelwidget.cpp` | Add themed stylesheet |
| `src/widgets/panel/utilitypanel.cpp` | Add themed stylesheet |
| `src/widgets/capture/selectionwidget.cpp` | Change border to white, add purple glow |

## Non-Goals

- No new configuration UI or settings
- No changes to the color picker presets or tool icons
- No structural widget refactoring
- No performance impact (stylesheets are applied once at startup)

## Open Questions

1. Selection glow: QPainter-based (painting a shadow path) or
   QGraphicsDropShadowEffect? Prefer QPainter to avoid widget-layer overhead.
