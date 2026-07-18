# Fix Tool Hit-Testing Cache + Geometric Prefilter

## Summary

Fix the broken pixel-level hit-testing cache in `CaptureToolObjects::findWithRadius()` (always cleared before use) and add a geometric bounding-rect prefilter to avoid unnecessary per-pixel scanning.

## Motivation

`capturetoolobjects.cpp:64-140` implements object hit-testing when the user clicks on the canvas to select a tool. It:

1. Creates a transparent `QPixmap` the size of the entire capture
2. Renders ALL tools onto it using `drawSearchArea()`
3. Converts to `QImage`
4. Scans pixels in a (2*radius+1)^2 area around the click point
5. Does this TWICE (radius=3 pass, then radius=5 pass if first fails)

The `m_imageCache` (declared at capturetoolobjects.h:33) is meant to cache the rendered QImage between calls, but at line 91 it's always cleared just before the check, making the cache dead code:

```cpp
m_imageCache.clear();  // <-- Always clears, defeating the cache
if (!m_imageCache.isEmpty()) { ... }  // Never reached
```

## Changes

### 1. Fix the cache logic

Don't clear the cache before checking it. Clear only when tool objects are added, removed, or reordered (in `append()`, `insert()`, `removeAt()`).

### 2. Add geometric bounding-rect prefilter

Before doing any pixel scanning, check which tools' `boundingRect()` contains the click point. Only render and pixel-scan those tools (in reverse order). For captures with 10+ tools spread across the canvas, this eliminates the redundant renders for tools far from the click point.

### 3. Single pass optimization

If a match is found at a smaller radius, skip the second pass entirely. The two-pass approach (first radius=3, then radius=5) should be unified into a single pass with the final radius.

## Acceptance criteria

- [ ] `m_imageCache` is no longer cleared before the emptiness check
- [ ] Cache is correctly invalidated when tool list changes
- [ ] Geometric prefilter reduces `findWithRadius()` calls to only tools near the click point
- [ ] Hit detection behavior is identical to current (same sensitivity, same tool priority)
- [ ] Measurable reduction in hit-testing latency for captures with 5+ tools

## Blocked by

None — can start immediately
