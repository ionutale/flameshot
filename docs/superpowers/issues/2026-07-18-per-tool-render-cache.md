# Per-Tool Render Cache for drawToolsData()

## Summary

Replace the current full-render approach in `drawToolsData()` with a per-tool render cache. Instead of re-rendering every annotation tool from scratch on any change, only re-render tools whose state has changed since the last render.

## Motivation

`CaptureWidget::drawToolsData()` (capturewidget.cpp:1899-1913) iterates every tool object in order and calls `processPixmapWithTool()` on each, re-rendering ALL annotations from `m_context.origScreenshot` every time. The code's own TODO comment at line 1901 acknowledges this:

```cpp
// TODO refactor this for performance. The objects should not all be updated
// at once every time
```

This function is called from 10+ places: every mouse move during drawing, every color/size change, every undo/redo, every tool commit. For a capture with 10 annotations, each mouse movement triggers 10 full tool renders.

## Changes

### 1. Per-tool render cache

Add a `QPixmap m_renderCache` member to `CaptureTool` (or use a parallel struct). Each tool renders its visual output once into its own cache when its state is "committed" (not actively being drawn). `drawToolsData()` composites from caches instead of re-rendering.

### 2. Dirty flag tracking

Each tool needs a `bool m_dirty` flag. When the tool's properties change (color, size, points, text), mark dirty. `drawToolsData()` only calls `process()` on dirty tools, then clears the flag.

### 3. Incremental compositing

Maintain a flat composited `QPixmap` of all committed tools. When a single tool is modified, re-render only that tool and composite it onto the cached result of all other tools. For tools drawn before the changed one, the cache is unchanged. For tools drawn after, they may need to be re-composited only (not re-rendered).

### 4. Special case for active tool

The tool currently being drawn should not use the cache — it's rendered on every mouse move. Once drawing is finished (mouse release), cache it.

## Acceptance criteria

- [ ] Per-tool `QPixmap` cache exists and stores the tool's rendered output
- [ ] `drawToolsData()` only re-renders dirty tools
- [ ] Compositing from caches produces identical visual output to the current full-render
- [ ] No regressions in tool layering order or transparency effects
- [ ] Existing tests (if any) pass without modification
- [ ] Measurable reduction in per-frame render cost for captures with 5+ annotations

## Blocked by

None — can start immediately
