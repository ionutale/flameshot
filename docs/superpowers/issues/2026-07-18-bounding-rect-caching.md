# Add Bounding Rect Caching for Tools

## Summary

Cache the result of `boundingRect()` in tool base classes so it's not recomputed from scratch on every call during `drawToolsData()`, hit-testing, and paint operations.

## Motivation

`boundingRect()` is called frequently during capture operations:
- In `drawToolsData()` via `paddedUpdateRect(tool->boundingRect())` — once per tool per render
- In `updateTool()` — to compute dirty regions
- In hit-testing — to find which tools are near the click point

For tools that inherit from `AbstractPathTool` (pencil, marker), `boundingRect()` iterates all accumulated points to find min/max coordinates. For a pencil stroke with 500+ points, this is O(n) each time.

For `ArrowTool`, `boundingRect()` computes arrow path geometry. For `TextTool`, it measures text layout. All of these produce deterministic results that can be cached between calls.

## Changes

### 1. Add cached bounding rect to base classes

Add a `QRectF m_cachedBoundingRect` and `bool m_boundingRectDirty` to `CaptureTool` base class.

### 2. Invalidation triggers

Mark the bounding rect dirty when:
- `drawStart()` is called (points reset)
- `drawMove()` adds a new point
- Color, thickness, or font changes (these affect the visual size)

### 3. Bounding rect access

Add a non-virtual `boundingRect()` to the base class that returns the cached value. Add a protected `invalidateBoundingRect()` method that subclasses call on state changes. Override the actual computation in `recomputeBoundingRect()` (virtual).

### 4. Optimization for AbstractPathTool

The caching is most impactful here. Instead of iterating all points, maintain running min/max as points are added in `addPoint()`.

## Acceptance criteria

- [ ] `boundingRect()` returns cached value when not dirty
- [ ] Cache is correctly invalidated on all state changes
- [ ] Visual behavior is identical
- [ ] Measurable reduction in `drawToolsData()` CPU time for pencil/marker tools with 100+ points

## Blocked by

None — can start immediately
