# Grid Overlay Caching

## Summary

Cache the grid dot overlay to a `QPixmap` instead of calling 500K+ `drawEllipse()` per frame in `CaptureWidget::paintEvent()`.

## Motivation

In `capturewidget.cpp:786-808`, the grid is rendered with nested loops:

```cpp
for (int y = topLeft.y(); y < m_context.selection.bottom() / scale; y += step) {
    for (int x = topLeft.x(); x < m_context.selection.right() / scale; x += step) {
        painter.drawEllipse(x, y, radius, radius);
    }
}
```

For a full-screen 1920x1080 capture with 10px grid stepping, this is ~20,000 `drawEllipse()` calls per frame. On a 5K display, it's ~500,000. The grid only changes when the selection geometry changes or the grid size/color is reconfigured — not every frame.

## Changes

### 1. Cache the grid to a QPixmap

Add a `QPixmap m_gridCache` member to `CaptureWidget`. Render the grid dots into this pixmap once when the selection geometry or grid configuration changes.

### 2. Invalidation triggers

Invalidate the cache (and re-render) when:
- Selection rectangle changes (`geometryChanged` signal)
- Grid size or color changes in config
- Widget is resized

### 3. Paint path

In `paintEvent()`, draw the cached grid pixmap with a single `drawPixmap()` call instead of the nested loops.

## Acceptance criteria

- [ ] Grid is rendered to a cache pixmap once per geometry change
- [ ] `paintEvent()` uses `drawPixmap()` instead of nested loop
- [ ] Grid appearance is identical to current behavior
- [ ] Cache is correctly invalidated on selection/zoom changes
- [ ] Measurable reduction in `paintEvent()` CPU time during idle

## Blocked by

None — can start immediately
