# Clean Up updateTool() Static Locals

## Summary

Replace `static QRect` local variables in `CaptureWidget::updateTool()` with member variables and eliminate redundant `update()` calls.

## Motivation

`capturewidget.cpp:1836-1860`, `updateTool()` uses `static QRect` to track which screen regions need repainting between calls:

```cpp
void CaptureWidget::updateTool(CaptureTool* tool)
{
    static QRect oldPreviewRect, oldToolObjectRect;
    // ...
    update(previewRect);
    update(toolObjectRect);
    update(oldPreviewRect);
    update(oldToolObjectRect);
    // ...
}
```

This has two problems:
1. **Thread safety concern**: `static` locals in C++ are not thread-safe by default (though Qt's main thread makes this mostly theoretical)
2. **4 `update()` calls per invocation**: Triggers 4 repaint regions even though only 2-3 may be needed. Each `update()` schedules a paint event.
3. **Static blend between calls**: The static variables carry state across calls to different tool instances, which is incorrect if `updateTool()` is ever called for a different tool without proper cleanup.

## Changes

### 1. Convert statics to member variables

Move `oldPreviewRect` and `oldToolObjectRect` to member variables of `CaptureWidget`:
- `m_lastPreviewRect`
- `m_lastToolObjectRect`

### 2. Reduce update calls

Instead of calling `update()` 4 times unconditionally, compute the dirty region as the union of the new and old rects and call `update()` once:

```cpp
QRect dirtyRect = previewRect.united(m_lastPreviewRect)
                   .united(toolObjectRect.united(m_lastToolObjectRect));
update(dirtyRect);
```

## Acceptance criteria

- [ ] No `static` local variables in `updateTool()`
- [ ] Only 1 `update()` call per invocation (union of dirty regions)
- [ ] Visual behavior is identical — no ghost artifacts from missing repaints
- [ ] No performance regression (the union computation is negligible)

## Blocked by

None — can start immediately
