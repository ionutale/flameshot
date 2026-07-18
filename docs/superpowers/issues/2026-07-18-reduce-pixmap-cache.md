# Reduce QPixmapCache Limit for Daemon Mode

## Summary

Set `QPixmapCache::setCacheLimit(512 * 1024)` at application startup to reduce the Qt internal pixmap cache from 10 MB to 512 KB, saving ~9 MB during daemon standby.

## Motivation

Qt's `QPixmapCache` has a default limit of 10240 KB (10 MB). This cache holds rendered pixmaps for icons, toolbars, and other UI elements to avoid re-rendering them. During daemon/standby mode, Flameshot only shows a system tray icon — no window, no toolbars, no icons — so the pixmap cache is essentially unused.

During capture mode, the cache is used for:
- Tool button icons (~230 KB for all 46 SVGs at button size)
- Selection handles and resize grips (~50 KB)
- Side panel widgets (~100 KB)
- Style computations (~50 KB)

This totals roughly **400-500 KB** for a fully loaded capture UI. 512 KB covers this comfortably. The default 10 MB was designed for general desktop applications with complex UIs (browsers, office suites, IDEs) and is wasteful for a screenshot tool.

## Changes

### 1. Set cache limit at startup

In `src/main.cpp` (or `FlameshotDaemon::start()`), add:

```cpp
QPixmapCache::setCacheLimit(512 * 1024);  // 512 KB
```

### 2. Placement

Best placed in `FlameshotDaemon::start()` after the `QApplication` is created, or in `main.cpp` before the event loop starts.

### 3. Verification

The cache limit is a soft limit — Qt evicts entries when the total exceeds this threshold. Setting it low just means eviction happens sooner. In capture mode, icons may need to be re-decoded if the cache is under heavy pressure, but at 512 KB for a 400-500 KB working set, eviction is rare.

## Acceptance criteria

- [ ] `QPixmapCache::setCacheLimit(512 * 1024)` is called at startup
- [ ] Standby memory usage drops by ~9 MB on macOS
- [ ] No visual regressions in capture mode — icons render correctly
- [ ] No increased repainting or icon flickering during capture
- [ ] Tray icon renders correctly (should not be affected, only 1 icon)

## Blocked by

None — can start immediately
