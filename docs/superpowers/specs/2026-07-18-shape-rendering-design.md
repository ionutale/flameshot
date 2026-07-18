# Shape Rendering Refinement

## Summary

Improve the visual quality of all annotation shapes (arrows, lines,
rectangles, circles, pencil, marker, text) with macOS-inspired polish.
Round all line caps and joins, add soft glow shadows, smooth pencil strokes
with bezier curves, refine arrowheads, and fix rectangle rendering.

## Motivation

All tools currently use sharp `SquareCap`/`BevelJoin` pen settings, giving
lines and outlines a jagged, unfinished look. Shapes sit flat on the image
with no depth. The pencil tool produces jittery polylines. The rectangle
paradoxically fills instead of stroking when thickness is set. These are the
types of details Apple obsesses over — and they add up to a noticeably more
polished feel.

## Changes

### 1. All Tools — Pen Settings

Every tool that draws lines or outlines changes its pen:

- `Qt::SquareCap` → `Qt::RoundCap`
- `Qt::BevelJoin` → `Qt::RoundJoin`

This applies to:
- `PencilTool::process()` — drawPolyline pen
- `LineTool::process()` — drawLine pen
- `ArrowTool::process()` — shaft pen (already uses FlatCap for curved style)
- `RectangleTool::process()` — preserve existing SquareCap+RoundJoin, keep as-is
- `CircleTool::process()` — drawEllipse pen
- `MarkerTool::process()` — drawLine pen
- `CircleCountTool::process()` — pointer line pen

For **RectangleTool**: the existing pen already uses `Qt::SquareCap` +
`Qt::RoundJoin`. Keep as-is — RoundJoin already handles corners elegantly.

### 2. Soft Glow Shadow

Add a subtle shadow behind each shape using QPainter's `QGraphicsDropShadowEffect`
or an equivalent QPainter-based approach.

Approach: Add a shadow pass before the main rendering in each tool's `process()`
method. The shadow is a blurred copy of the shape at low opacity:

- Color: same as the shape's `m_color`
- Opacity: 20% (`QColor(m_color.red(), m_color.green(), m_color.blue(), 51)`)
- Blur radius: 4px
- Offset: 1px down, 1px right

Implementation options:
- **QPainter approach**: Draw the shape a second time with a blurred pen
  (using `QPainterPath::setFillRule` and `painter.drawPath()` with a blurred
  `QGraphicsEffect` on a temp pixmap, OR use `QPainterPathStroker` + blur)
- **Simpler approach**: Draw the shape offset by (1,1) with the color at alpha=51
  and a 4px thicker pen, then draw the main shape on top

For performance, the shadow should be rendered directly in the tool's `process()`
method, not as a separate widget overlay.

### 3. Arrowhead — Concave Notch Style

The current arrow has two styles (Default and Curved). Replace both with a
single refined concave notch style:

- Shaft: `RoundCap`, no shortening (extends fully to the arrowhead tip)
- Arrowhead path: a filled shape with concave trailing edge (quad bezier)
- The arrowhead connects smoothly to the shaft with no gap
- Proportions: `ArrowHeight = 18 + thickness * 4` (keep current),
  `ArrowWidth = 10 + thickness * 2` (keep current)
- The concave notch is a quad bezier curve: `QPainterPath::quadTo()` from
  the base corners inward toward the center

Remove the "default" vs "curved" distinction — the concave notch replaces
both. The `reverseArrow` config option stays.

### 4. Rectangle — Adaptive Rendering

When `thickness <= 5`:
- Draw as stroked outline: `painter.setPen(QPen(color(), thickness, RoundCap, RoundJoin))`
- Rounded corners: `radius = thickness`
- No fill

When `thickness > 5`:
- Keep current behavior: filled rounded rect (rounded by `thickness`)
- The fill gives a "blob" or "highlight" feel at large sizes

This is controlled by a simple threshold check in `RectangleTool::process()`.

### 5. Circle — RoundCap + Subtle Fill

- Outline: `RoundCap`, `RoundJoin` with full `thickness`
- Add a subtle fill: `QColor(m_color.red(), m_color.green(), m_color.blue(), 20)`
  (approximately 8% opacity)
- This makes the circle feel more substantial without hiding the underlying image

### 6. Pencil — Bezier Curve Smoothing

Replace `drawPolyline()` with smoothed bezier curves using Catmull-Rom to
bezier conversion:

```cpp
QPainterPath smoothPath;
for (int i = 0; i < m_points.size(); i++) {
    if (i == 0) {
        smoothPath.moveTo(m_points[i]);
    } else {
        QPointF p0 = (i > 1) ? m_points[i - 2] : m_points[i - 1];
        QPointF p1 = m_points[i - 1];
        QPointF p2 = m_points[i];
        QPointF p3 = (i < m_points.size() - 1) ? m_points[i + 1] : m_points[i];
        // Centripetal Catmull-Rom to cubic bezier
        float t = 0.5;
        QPointF cp1 = p1 + (p2 - p0) * t / 6;
        QPointF cp2 = p2 - (p3 - p1) * t / 6;
        smoothPath.cubicTo(cp1, cp2, p2);
    }
}
painter.drawPath(smoothPath);
```

This converts raw mouse points into a smooth continuous curve. The smoothing
is subtle — it doesn't alter the intended shape, just removes jitter.

### 7. Marker — RoundCap + Softer Edges

- Change pen to `RoundCap`, `RoundJoin`
- Keep `CompositionMode_Multiply` and 35% opacity — these are the core of the
  marker effect
- The round caps/joins prevent hard square corners at path vertices

### 8. CircleCount — RoundCap

- The pointer line: change to `RoundCap`
- Already the most polished tool; keep all existing rendering

### 9. Text — No Changes

Text rendering is already good and doesn't involve line strokes. No changes.

## Files to Modify

| File | Change |
|------|--------|
| `src/tools/pencil/penciltool.cpp` | Add bezier smoothing, RoundCap, RoundJoin, shadow |
| `src/tools/line/linetool.cpp` | RoundCap, RoundJoin, shadow |
| `src/tools/arrow/arrowtool.cpp` | Concave notch head, no gap, RoundCap, shadow |
| `src/tools/rectangle/rectangletool.cpp` | Adaptive stroke/fill threshold, shadow |
| `src/tools/circle/circletool.cpp` | RoundCap, subtle fill, shadow |
| `src/tools/marker/markertool.cpp` | RoundCap, RoundJoin, shadow |
| `src/tools/circlecount/circlecounttool.cpp` | RoundCap on pointer, shadow |

### 10. Border/Stroke on All Shapes

Every shape gets a thin border/stroke in a contrasting color (via
`ColorUtils::contrastColor()`) to help it stand out from the image:

- **Stroked shapes** (line, arrow shaft, circle outline, pencil, marker):
  Draw twice — first with border color at `thickness + 2`, then with main
  color at `thickness`. This creates a 1px border on each side.

- **Filled shapes** (arrowhead, thick rectangle, circle count):
  Use `QPainterPathStroker` to create a thick outline path, fill it with
  border color, then fill the original path with main color.

- **Shadow**: Draw the shape offset by (2, 2) with `QColor(0, 0, 0, 40)`
  before border and main rendering.

Examples:
- Arrow: shaft uses border line + main line; head uses `QPainterPathStroker`
  border + main fill
- Rectangle (thin ≤5): rounded rect outline with border + main pass
- Rectangle (thick >5): filled rounded rect with `QPainterPathStroker` border
- Circle: outline with border + main pass
- Marker: border drawn in `SourceOver` mode, then main in `Multiply` mode

## Non-Goals

- No new tool types or features
- No changes to the color picker, panel, or UI chrome
- No animation or transition effects
- No changes to the pixelate tool (it doesn't use pen/brush)
- No changes to the move tool (it doesn't render)
