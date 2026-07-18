# Remove Double drawToolsData() in Undo/Redo

## Summary

Remove the redundant pre- and post- `drawToolsData()` calls in `undo()` and `redo()` after the per-tool render cache is in place.

## Motivation

`capturewidget.cpp:2040-2070` has a documented workaround:

```cpp
void CaptureWidget::undo()
{
    // FIXME this is a temporary workaround
    drawToolsData();      // full re-render before undo
    m_undoStack.undo();
    drawToolsData();      // full re-render after undo
    // ...
}
```

Same pattern in `redo()` at lines 2059-2070. The first call is meant to flush any pending state to the current pixmap before the undo changes the tool stack. The second call re-renders with the new state. With the per-tool render cache from issue #1, this redundancy is no longer needed.

## Changes

### 1. Remove the pre-undo drawToolsData() call

With per-tool caching, the current rendered state is already in each tool's cache. The undo operation modifies the tool stack, which invalidates the affected tool's cache. A single `drawToolsData()` call after the undo is sufficient.

### 2. Remove the pre-redo drawToolsData() call

Same change for `redo()`.

### 3. Verify with the render cache

This depends on the guarantee that the render cache in issue #1 always reflects the latest committed state before an undo/redo.

## Acceptance criteria

- [ ] `undo()` calls `drawToolsData()` exactly once
- [ ] `redo()` calls `drawToolsData()` exactly once
- [ ] No visual artifacts during undo/redo sequences
- [ ] FIXME comment at line 2050 is removed

## Blocked by

- #1 (Per-tool render cache for drawToolsData())
