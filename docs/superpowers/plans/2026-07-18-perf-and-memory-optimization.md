# Performance & Memory Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reduce standby RAM from ~25 MB to ~15 MB on macOS and drastically reduce per-frame render cost during capture editing.

**Architecture:** 11 independent tasks ordered by impact and dependency. Tasks 1-7 optimize the capture rendering pipeline (CPU/per-frame). Tasks 8-11 optimize daemon standby memory.

**Tech Stack:** C++20, Qt 6 (Core, Gui, Widgets, Network, Svg)

---

### Task 1: Reduce QPixmapCache Limit

**Files:**
- Modify: `src/core/flameshotdaemon.cpp:66-100`
- No test changes needed

- [ ] **Step 1: Add cache limit call in FlameshotDaemon constructor**

At the end of the `FlameshotDaemon` constructor body, before the `#if !defined(DISABLE_UPDATE_CHECKER)` block at line 97:

```cpp
QPixmapCache::setCacheLimit(512 * 1024);
```

This reduces Qt's internal pixmap cache from 10 MB to 512 KB. During standby, only the tray icon is rendered — the cache is essentially unused. During capture mode, the working set (tool icons, selection handles, style data) is ~400-500 KB, well within 512 KB.

- [ ] **Step 2: Verify the change**

Build and launch the daemon. Check that:
- The tray icon appears correctly
- Standby memory drops by ~9 MB (from ~24 MB to ~15 MB on macOS)

- [ ] **Step 3: Commit**

```bash
git add src/core/flameshotdaemon.cpp
git commit -m "perf: reduce QPixmapCache from 10MB to 512KB for daemon standby
```

---

### Task 2: Destroy QNetworkAccessManager After Update Check

**Files:**
- Modify: `src/core/flameshotdaemon.cpp:212-258`
- No test changes needed

- [ ] **Step 1: Add cleanup at end of the reply handler**

In `handleReplyCheckUpdates` (daemon.cpp ~line 234), after processing the network reply, add cleanup:

```cpp
void FlameshotDaemon::handleReplyCheckUpdates(QNetworkReply* reply)
{
    // ... existing reply processing code ...

    reply->deleteLater();

    // Clean up the network manager after each check — saves ~1-5 MB during standby
    if (m_networkCheckUpdates) {
        m_networkCheckUpdates->deleteLater();
        m_networkCheckUpdates = nullptr;
    }
}
```

- [ ] **Step 2: Ensure `getLatestAvailableVersion` handles null manager**

In `getLatestAvailableVersion()`, the existing code already handles `m_networkCheckUpdates == nullptr` at line 217. It re-creates the manager when needed:

```cpp
if (nullptr == m_networkCheckUpdates) {
    m_networkCheckUpdates = new QNetworkAccessManager(this);
    connect(m_networkCheckUpdates, &QNetworkAccessManager::finished,
            this, &FlameshotDaemon::handleReplyCheckUpdates);
}
```

This path is already correct — it re-creates fresh on the next 24h timer or manual check.

- [ ] **Step 3: Build and verify**

Build and launch the daemon. Trigger the update check. Verify memory drops by ~1-5 MB after the reply is processed.

- [ ] **Step 4: Commit**

```bash
git add src/core/flameshotdaemon.cpp
git commit -m "perf: destroy QNetworkAccessManager after update check completes"
```

---

### Task 3: Fix Dangling OverlayMessage::m_instance

**Files:**
- Modify: `src/widgets/capture/overlaymessage.h:20-46`
- Modify: `src/widgets/capture/overlaymessage.cpp:12-41`

- [ ] **Step 1: Add destructor declaration to header**

In `overlaymessage.h`, add a destructor declaration in the private section:

```cpp
private:
    QStack<QString> m_messageStack;
    QRect m_targetArea;
    QColor m_fillColor, m_textColor;
    static OverlayMessage* m_instance;

    OverlayMessage(QWidget* parent, const QRect& center);
    ~OverlayMessage() override;  // <-- add this
```

- [ ] **Step 2: Add destructor implementation**

At the end of `overlaymessage.cpp`, before `m_instance = nullptr;`:

```cpp
OverlayMessage::~OverlayMessage()
{
    if (m_instance == this) {
        m_instance = nullptr;
    }
}
```

- [ ] **Step 3: Commit**

```bash
git add src/widgets/capture/overlaymessage.h src/widgets/capture/overlaymessage.cpp
git commit -m "fix: null OverlayMessage::m_instance on destruction to prevent dangling pointer"
```

---

### Task 4: Remove Dead `Screenshot` Member from ScreenGrabber

**Files:**
- Modify: `src/utils/screengrabber.h:44-51`

- [ ] **Step 1: Remove the dead member**

In `screengrabber.h`, delete line 45:

```diff
     DesktopInfo m_info;
-    QPixmap Screenshot;
     int m_selectedMonitor;
```

- [ ] **Step 2: Verify build**

Build the project. Ensure no compilation errors referencing `Screenshot`.

- [ ] **Step 3: Commit**

```bash
git add src/utils/screengrabber.h
git commit -m "cleanup: remove unused Screenshot member from ScreenGrabber"
```

---

### Task 5: Fix Tool Hit-Testing Cache + Geometric Prefilter

**Files:**
- Modify: `src/widgets/capture/capturetoolobjects.cpp:64-140`
- Modify: `src/widgets/capture/capturetoolobjects.h:32-33`

- [ ] **Step 1: Fix the broken cache logic in `findWithRadius()`**

Replace the current broken caching at lines 89-111 with a working approach. The cache should persist across calls and only be invalidated when the tool list changes (via `append()`, `insert()`, `removeAt()`).

Key changes:
1. Remove `m_imageCache.clear()` at line 91 (the one that always clears before use)
2. Keep the `clear()` calls in `append()`/`insert()`/`removeAt()` (lines 18, 29, 60)
3. Add a geometric prefilter: before rendering any tool to the QImage, check if the click point is within the tool's `boundingRect()` plus radius
4. Only render tools whose bounding rect (plus radius) contains the click point

```cpp
int CaptureToolObjects::findWithRadius(QPainter& painter,
                                       QPixmap& pixmap,
                                       const QPoint& pos,
                                       int radius)
{
    // Check cache usability: must have same size as tool list
    // Cache is invalidated in append()/insert()/removeAt()
    bool useCache = (m_imageCache.size() == m_captureToolObjects.size());

    for (int index = m_captureToolObjects.size() - 1; index >= 0; --index) {
        auto toolItem = m_captureToolObjects.at(index);
        if (toolItem.isNull()) continue;

        QRect toolRect = toolItem->boundingRect();
        // Geometric prefilter: skip if the click is outside the tool's bounding
        // rect (plus radius margin) — avoids rendering this tool entirely
        if (!toolRect.isEmpty() &&
            !toolRect.adjusted(-radius, -radius, +radius, +radius).contains(pos)) {
            continue;
        }

        int currentRadius = radius;
        QImage image;

        if (useCache && index < m_imageCache.size()) {
            image = m_imageCache.at(index);
        } else {
            // render this tool only — not all tools
            pixmap.fill(Qt::transparent);
            toolItem->drawSearchArea(painter, pixmap);

            image = pixmap.toImage();
            if (index >= m_imageCache.size()) {
                m_imageCache.resize(index + 1);
            }
            m_imageCache[index] = image;
        }

        if (toolItem->type() == CaptureTool::TYPE_TEXT) {
            if (currentRadius > SEARCH_RADIUS_NEAR) {
                continue;
            }
            currentRadius += SEARCH_RADIUS_TEXT_HANDICAP;
        }

        // Pixel scan with direct buffer access
        const uchar* bits = image.constBits();
        int bytesPerLine = image.bytesPerLine();
        int pixelBytes = image.depth() / 8;

        for (int x = pos.x() - currentRadius; x <= pos.x() + currentRadius; ++x) {
            if (x < 0 || x >= image.width()) continue;
            for (int y = pos.y() - currentRadius; y <= pos.y() + currentRadius; ++y) {
                if (y < 0 || y >= image.height()) continue;
                // Direct pointer access instead of QImage::pixel() virtual dispatch
                const QRgb* pixelPtr = reinterpret_cast<const QRgb*>(
                    bits + y * bytesPerLine + x * pixelBytes);
                if (*pixelPtr != 0) {
                    return index;
                }
            }
        }
    }
    return -1;
}
```

Also update the pixmap creation in `find()` — the pixmap only needs to be as large as the largest tool's bounding rect, not the full capture size. But keep it simple: reuse the existing size, but reduce the fill/clear area:

```cpp
int CaptureToolObjects::find(const QPoint& pos, QSize captureSize)
{
    if (m_captureToolObjects.empty()) {
        return -1;
    }
    QPixmap pixmap(captureSize);
    QPainter painter(&pixmap);
    // First pass with SEARCH_RADIUS_NEAR
    int index = findWithRadius(painter, pixmap, pos, SEARCH_RADIUS_NEAR);
    if (-1 == index) {
        // Second pass with SEARCH_RADIUS_FAR
        index = findWithRadius(painter, pixmap, pos, SEARCH_RADIUS_FAR);
    }
    return index;
}
```

- [ ] **Step 2: Cache only rendered tool images**

Remove the `m_imageCache.clear()` call from `findWithRadius()` (the one at line 91). The cache is already cleared in `append()`/`insert()`/`removeAt()`.

- [ ] **Step 3: Build and verify**

Build the project. Test by clicking on various tools in the capture editor. Verify that:
- Clicking on a tool selects it correctly
- Clicking on empty space deselects
- Performance is better than before (especially with 5+ tools)

- [ ] **Step 4: Commit**

```bash
git add src/widgets/capture/capturetoolobjects.cpp src/widgets/capture/capturetoolobjects.h
git commit -m "perf: fix hit-testing cache and add geometric prefilter"
```

---

### Task 6: Clean Up updateTool() Static Locals

**Files:**
- Modify: `src/widgets/capture/capturewidget.h:115-235`
- Modify: `src/widgets/capture/capturewidget.cpp:1836-1860`

- [ ] **Step 1: Add member variables to header**

Add two member variables in the private section of `CaptureWidget`:

```cpp
    // Grid
    bool m_displayGrid{ false };
    int m_gridSize{ 10 };

    // Cached update rects for updateTool()
    QRect m_lastPreviewRect;
    QRect m_lastToolObjectRect;

    bool m_clipboardWorkaroundDone{ false };
```

- [ ] **Step 2: Replace static locals with member variables**

Replace the body of `updateTool()`:

```cpp
void CaptureWidget::updateTool(CaptureTool* tool)
{
    if (!tool || !tool->showMousePreview()) {
        return;
    }

    QRect previewRect(tool->mousePreviewRect(m_context));
    previewRect += QMargins(previewRect.width(),
                            previewRect.height(),
                            previewRect.width(),
                            previewRect.height());

    QRect toolObjectRect = paddedUpdateRect(tool->boundingRect());

    // Union of old and new dirty rects — single update() call instead of 4
    QRect dirtyRect = previewRect.united(toolObjectRect)
                      .united(m_lastPreviewRect)
                      .united(m_lastToolObjectRect);
    update(dirtyRect);

    m_lastPreviewRect = previewRect;
    m_lastToolObjectRect = toolObjectRect;
}
```

- [ ] **Step 3: Build and verify**

Build and test. Verify no change in visual behavior during tool drawing.

- [ ] **Step 4: Commit**

```bash
git add src/widgets/capture/capturewidget.h src/widgets/capture/capturewidget.cpp
git commit -m "perf: replace updateTool static locals with member vars, reduce update() calls"
```

---

### Task 7: Add Bounding Rect Caching for Tools

**Files:**
- Modify: `src/tools/capturetool.h:84-214`
- Modify: `src/tools/abstractpathtool.cpp:55-86`
- Modify: `src/tools/abstractpathtool.h:34-43`

- [ ] **Step 1: Add cached bounding rect to base class**

In `capturetool.h`, add to the protected section:

```cpp
protected:
    void copyParams(const CaptureTool* from, CaptureTool* to)
    {
        to->m_count = from->m_count;
    }

    // Cached bounding rect — subclasses call invalidateBoundingRect() when state
    // changes, and the base class recomputes on next access.
    QRect cachedBoundingRect() const
    {
        if (m_boundingRectDirty) {
            m_cachedBoundingRect = recomputeBoundingRect();
            m_boundingRectDirty = false;
        }
        return m_cachedBoundingRect;
    }
    void invalidateBoundingRect() const { m_boundingRectDirty = true; }
    virtual QRect recomputeBoundingRect() const { return {}; }
```

Add to the private section:

```cpp
private:
    unsigned int m_count;
    bool m_editMode;
    mutable QRect m_cachedBoundingRect;
    mutable bool m_boundingRectDirty{ true };
};
```

- [ ] **Step 2: Make `boundingRect()` non-pure with cached default**

In `capturetool.h`, change `boundingRect()` from pure virtual to a method that uses the cache:

```cpp
virtual QRect boundingRect() const { return cachedBoundingRect(); }
```

- [ ] **Step 3: Update AbstractPathTool to use caching**

In `abstractpathtool.h`, change `boundingRect()` override to use the cache pattern:

```diff
-    QRect boundingRect() const override;
+    QRect recomputeBoundingRect() const override;
```

In `abstractpathtool.cpp`, rename the existing `boundingRect()` to `recomputeBoundingRect()`:

```cpp
QRect AbstractPathTool::recomputeBoundingRect() const
{
    if (m_points.isEmpty()) {
        return {};
    }
    int min_x = m_points.at(0).x();
    int min_y = m_points.at(0).y();
    int max_x = m_points.at(0).x();
    int max_y = m_points.at(0).y();
    for (auto point : m_points) {
        if (point.x() < min_x) min_x = point.x();
        if (point.y() < min_y) min_y = point.y();
        if (point.x() > max_x) max_x = point.x();
        if (point.y() > max_y) max_y = point.y();
    }
    int offset = m_thickness <= 1
      ? 1
      : static_cast<int>(round(m_thickness * 0.7 + 0.5));
    return QRect(min_x - offset,
                 min_y - offset,
                 std::abs(min_x - max_x) + offset * 2,
                 std::abs(min_y - max_y) + offset * 2)
      .normalized();
}
```

In `addPoint()`, invalidate the cache:

```cpp
void AbstractPathTool::addPoint(const QPoint& point)
{
    if (m_pathArea.left() > point.x()) {
        m_pathArea.setLeft(point.x());
    } else if (m_pathArea.right() < point.x()) {
        m_pathArea.setRight(point.x());
    }
    if (m_pathArea.top() > point.y()) {
        m_pathArea.setTop(point.y());
    } else if (m_pathArea.bottom() < point.y()) {
        m_pathArea.setBottom(point.y());
    }
    m_points.append(point);
    invalidateBoundingRect();
}
```

In `onSizeChanged()` and `onColorChanged()`, call `invalidateBoundingRect()`.

- [ ] **Step 4: Update AbstractTwoPointTool to use caching**

In `abstracttwopointtool.h`, replace `boundingRect()` with `recomputeBoundingRect()`:

```diff
-    QRect boundingRect() const override;
+    QRect recomputeBoundingRect() const override;
```

In `abstracttwopointtool.cpp`, rename the function and add invalidate calls to `drawStart()`, `drawMove()`, `drawMoveWithAdjustment()`, and `onSizeChanged()`.

- [ ] **Step 5: Update concrete tools that override `boundingRect()`**

Search for all `boundingRect() const override` implementations in the tools/ directory (pixelate, text, circlecount, etc.) and rename them to `recomputeBoundingRect()` and add `invalidateBoundingRect()` calls in their state-changing methods.

Then replace their `virtual QRect boundingRect() const override` declarations with `virtual QRect recomputeBoundingRect() const override`.

- [ ] **Step 6: Build and verify**

Build and test. Verify that:
- All tools render correctly
- Bounding rects are correct (selection, hit-testing, paint regions)
- No performance regression

- [ ] **Step 7: Commit**

```bash
git add src/tools/capturetool.h src/tools/abstractpathtool.cpp src/tools/abstractpathtool.h
git add src/tools/abstracttwopointtool.cpp src/tools/abstracttwopointtool.h
git add $(find src/tools -name '*.cpp' -o -name '*.h' | xargs grep -l 'boundingRect()')
git commit -m "perf: add cached bounding rect to CaptureTool base class"
```

---

### Task 8: Optimize PixelateTool

**Files:**
- Modify: `src/tools/pixelate/pixelatetool.cpp:63-223`

- [ ] **Step 1: Replace QImage::pixel()/setPixel() with direct buffer access**

Replace `fringe[i].pixel(x, y)` calls (lines 170-180) with direct `constBits()` pointer access. Replace `pixelated.setPixel(x, y, value)` (line 212) with direct `bits()` pointer access.

```cpp
// At the start of the secure branch, extract fringe as const uchar* pointers
std::array<const uchar*, 4> fringeBits;
std::array<int, 4> fringeStride;
std::array<int, 4> fringePixelBytes;
for (int i = 0; i < 4; ++i) {
    fringeBits[i] = fringe[i].constBits();
    fringeStride[i] = fringe[i].bytesPerLine();
    fringePixelBytes[i] = fringe[i].depth() / 8;
}

// In the inner loop (replacing lines 169-183):
for (int i = 0; i < 4; ++i) {
    int fx = std::clamp(
        static_cast<int>(horizontal * fringe[i].width() + sampling_noise(prng)),
        0, fringe[i].width() - 1);
    int fy = std::clamp(
        static_cast<int>(vertical * fringe[i].height() + sampling_noise(prng)),
        0, fringe[i].height() - 1);
    const QRgb* pixelPtr = reinterpret_cast<const QRgb*>(
        fringeBits[i] + fy * fringeStride[i] + fx * fringePixelBytes[i]);
    QColor c = QColor::fromRgb(*pixelPtr);
    samples[i][0] = c.redF();
    samples[i][1] = c.greenF();
    samples[i][2] = c.blueF();
}

// After computing rgb values (replacing line 211-212):
uchar* pixelatedBits = pixelated.bits();
int pixelatedStride = pixelated.bytesPerLine();
int pixelatedPixelBytes = pixelated.depth() / 8;
QRgb* dest = reinterpret_cast<QRgb*>(pixelatedBits + y * pixelatedStride + x * pixelatedPixelBytes);
*dest = qRgb(rgb[0], rgb[1], rgb[2]);
```

- [ ] **Step 2: Pre-generate noise tables**

Instead of calling `std::normal_distribution` per pixel, pre-generate noise arrays:

```cpp
const int NOISE_TABLE_SIZE = 256;
std::array<float, NOISE_TABLE_SIZE> precomputedNoise;
std::array<float, NOISE_TABLE_SIZE> precomputedSamplingNoise;
for (int i = 0; i < NOISE_TABLE_SIZE; ++i) {
    precomputedNoise[i] = noise(prng);
    precomputedSamplingNoise[i] = sampling_noise(prng);
}
```

Index into the table with `(x + y) % 256` in the inner loop.

- [ ] **Step 3: Reduce fringe copies from 4 to 1**

Instead of 4 `pixmap.copy().toImage()` calls, copy the full selection region once and access fringe pixels via offset arithmetic:

```cpp
// Single copy instead of 4
QImage selectionImage = pixmap.copy(selectionScaled).toImage();
int selStride = selectionImage.bytesPerLine();
int selBpp = selectionImage.depth() / 8;
const uchar* selBits = selectionImage.constBits();
int selW = selectionImage.width();
int selH = selectionImage.height();

// Access fringe pixels via offset:
// Top fringe: y = 0
// Bottom fringe: y = selH - 1
// Left fringe: x = 0
// Right fringe: x = selW - 1
```

- [ ] **Step 4: Build and verify**

Build and test. Verify:
- Visual output is identical to previous (deterministic since seed=42 is preserved)
- Pixelation performance improves measurably
- Both secure and insecure modes work

- [ ] **Step 5: Commit**

```bash
git add src/tools/pixelate/pixelatetool.cpp
git commit -m "perf: optimize PixelateTool with direct buffer access and pre-generated noise"
```

---

### Task 9: Grid Overlay Caching

**Files:**
- Modify: `src/widgets/capture/capturewidget.h:230-235`
- Modify: `src/widgets/capture/capturewidget.cpp:786-808`
- Modify: `src/widgets/capture/capturewidget.cpp` — add `onDisplayGridChanged()` / `onGridSizeChanged()` handlers

- [ ] **Step 1: Add grid cache member**

In `capturewidget.h`, add:

```cpp
    // Grid
    bool m_displayGrid{ false };
    int m_gridSize{ 10 };
    QPixmap m_gridCache;        // <-- add this
    bool m_gridCacheDirty{ true };  // <-- add this
```

- [ ] **Step 2: Add cache invalidation method**

Add to the private section:

```cpp
    void invalidateGridCache();
```

In `capturewidget.cpp`, add:

```cpp
void CaptureWidget::invalidateGridCache()
{
    m_gridCache = QPixmap();
    m_gridCacheDirty = true;
}
```

- [ ] **Step 3: Rebuild the grid cache when dirty**

Add a helper to rebuild the grid cache:

```cpp
void CaptureWidget::ensureGridCache()
{
    if (!m_gridCacheDirty || !m_displayGrid) {
        return;
    }

    const auto scale{ m_context.screenshot.devicePixelRatio() };
    auto topLeft = mapToGlobal(m_context.selection.topLeft() / scale);
    topLeft.rx() -= topLeft.x() % m_gridSize;
    topLeft.ry() -= topLeft.y() % m_gridSize;
    topLeft = mapFromGlobal(topLeft);

    const auto step{ m_gridSize / scale };
    const auto radius{ 1 * scale };

    QRect gridRect(topLeft, QPoint(m_context.selection.right() / scale,
                                   m_context.selection.bottom() / scale));
    m_gridCache = QPixmap(gridRect.size());
    m_gridCache.fill(Qt::transparent);

    QPainter painter(&m_gridCache);
    QColor gridColor = ConfigHandler().uiColor();
    gridColor.setAlpha(100);
    painter.setPen(gridColor);
    painter.setBrush(QBrush(gridColor));

    for (int y = 0; y < gridRect.height(); y += step) {
        for (int x = 0; x < gridRect.width(); x += step) {
            painter.drawEllipse(x, y, radius, radius);
        }
    }

    m_gridCacheDirty = false;
}
```

- [ ] **Step 4: Replace loop in paintEvent**

Replace lines 786-808 in `paintEvent()` with:

```cpp
    if (m_displayGrid) {
        ensureGridCache();
        if (!m_gridCache.isNull()) {
            auto topLeft = mapToGlobal(m_context.selection.topLeft() / scale);
            topLeft.rx() -= topLeft.x() % m_gridSize;
            topLeft.ry() -= topLeft.y() % m_gridSize;
            topLeft = mapFromGlobal(topLeft);
            painter.drawPixmap(topLeft, m_gridCache);
        }
    }
```

- [ ] **Step 5: Add invalidation triggers**

In `onDisplayGridChanged()` and `onGridSizeChanged()`, call `invalidateGridCache()`. Also add to `resizeEvent()` and the selection geometry change handler.

- [ ] **Step 6: Build and verify**

Build and test. Verify:
- Grid appears correctly in the selection area
- Grid updates when selection changes
- No flickering or visual artifacts

- [ ] **Step 7: Commit**

```bash
git add src/widgets/capture/capturewidget.h src/widgets/capture/capturewidget.cpp
git commit -m "perf: cache grid overlay to QPixmap, avoid 500K drawEllipse calls per frame"
```

---

### Task 10: Per-Tool Render Cache for drawToolsData()

**Files:**
- Modify: `src/tools/capturetool.h:84-214`
- Modify: `src/widgets/capture/capturewidget.cpp:1899-1936`
- New: none

This is the most impactful change. Instead of re-rendering all tools from scratch on every change, maintain a cached composition of committed tools.

- [ ] **Step 1: Add render cache and dirty flag to CaptureTool base class**

In `capturetool.h`, add to the private section:

```cpp
    mutable QRect m_cachedBoundingRect;
    mutable bool m_boundingRectDirty{ true };

protected:
    QPixmap m_renderCache;
    bool m_renderCacheDirty{ true };

public:
    const QPixmap& renderCache() const { return m_renderCache; }
    void setRenderCache(const QPixmap& cache) { m_renderCache = cache; m_renderCacheDirty = false; }
    bool isRenderCacheDirty() const { return m_renderCacheDirty; }
    void markRenderCacheDirty() { m_renderCacheDirty = true; invalidateBoundingRect(); }
```

Mark the cache dirty in `onColorChanged()`, `onSizeChanged()`, `drawEnd()`, and `drawStart()` in all base classes.

- [ ] **Step 2: Rewrite drawToolsData() for incremental compositing**

```cpp
void CaptureWidget::drawToolsData(bool drawSelection)
{
    QPixmap pixmapItem = m_context.origScreenshot;

    for (const auto& toolItem : m_captureToolObjects.captureToolObjects()) {
        if (toolItem.isNull()) continue;

        if (toolItem->isRenderCacheDirty()) {
            // Re-render this tool starting from the composited pixmap so far
            QPixmap toolCache = pixmapItem;
            processPixmapWithTool(&toolCache, toolItem);
            toolItem->setRenderCache(toolCache);
        }
        // For future compositing, render subsequent tools on top of this one
        pixmapItem = toolItem->renderCache();
    }

    m_context.screenshot = pixmapItem;
    if (drawSelection) {
        drawObjectSelection();
    }
}
```

When a single tool changes (e.g., color change on the 5th tool), only that tool and tools rendered after it need updating. The composition is rebuilt but rendering only happens for dirty tools.

- [ ] **Step 3: Optimize — skip re-compositing of unchanged trailing tools**

If only one tool in the middle is dirty, we still need to re-composite tools after it (since they draw on top). But we don't need to re-render them. Track the "last clean index" and only call `processPixmapWithTool` on dirty ones:

```cpp
void CaptureWidget::drawToolsData(bool drawSelection)
{
    QPixmap pixmapItem = m_context.origScreenshot;
    for (const auto& toolItem : m_captureToolObjects.captureToolObjects()) {
        if (toolItem.isNull()) continue;
        if (toolItem->isRenderCacheDirty()) {
            QPixmap toolCache = pixmapItem;
            processPixmapWithTool(&toolCache, toolItem);
            toolItem->setRenderCache(toolCache);
        }
        pixmapItem = toolItem->renderCache();
    }
    m_context.screenshot = pixmapItem;
    if (drawSelection) {
        drawObjectSelection();
    }
}
```

- [ ] **Step 4: Mark cache dirty on relevant state changes**

Ensure `markRenderCacheDirty()` is called in:
- `onColorChanged()` — in all tools
- `onSizeChanged()` — in all tools
- `drawEnd()` — when the tool is finalized
- `drawStart()` — when drawing begins (active tool is not cached)

- [ ] **Step 5: Active tool special case**

The tool currently being drawn by the user should use the old immediate-render path (since it changes every mouse move). Only cache committed tools:

```cpp
// In mouseMoveEvent — during active drawing, don't use cache:
if (m_activeTool && m_mouseIsClicked) {
    // Immediate render for active tool — not cached
    m_activeTool->process(painter, m_context.screenshot);
}
```

In `pushToolToStack()`, when a tool is committed, call `markRenderCacheDirty()` (it will re-render once).

- [ ] **Step 6: Build and verify**

Build and test. Verify:
- All tools render identically to before
- Color/size changes update the display correctly
- Undo/redo works correctly
- Layer reordering works correctly
- Performance improvement is measurable with 5+ annotations

- [ ] **Step 7: Commit**

```bash
git add src/tools/capturetool.h src/widgets/capture/capturewidget.cpp
git commit -m "perf: add per-tool render cache for incremental drawToolsData()"
```

---

### Task 11: Remove Double drawToolsData() in Undo/Redo

**Files:**
- Modify: `src/widgets/capture/capturewidget.cpp:2040-2070`

This task depends on Task 10 being complete. With per-tool render caching, the pre- and post- re-render is unnecessary.

- [ ] **Step 1: Remove the pre-undo drawToolsData call**

```cpp
void CaptureWidget::undo()
{
    if (m_activeTool &&
        (m_activeTool->isChanged() || m_activeTool->editMode())) {
        m_panel->setActiveLayer(-1);
    }

    // drawToolsData is now called once — the render cache handles the rest
    m_undoStack.undo();
    drawToolsData();
    updateLayersPanel();

    restoreCircleCountState();
}
```

- [ ] **Step 2: Remove the pre-redo drawToolsData call**

```cpp
void CaptureWidget::redo()
{
    m_undoStack.redo();
    drawToolsData();
    update();
    updateLayersPanel();

    restoreCircleCountState();
}
```

- [ ] **Step 3: Remove the FIXME comments**

Delete lines 2049-2050 and 2061-2062:

```diff
-    // drawToolsData is called twice to update both previous and new regions
-    // FIXME this is a temporary workaround
```

- [ ] **Step 4: Build and verify**

Build and test. Verify:
- Undo/redo works correctly
- No visual artifacts during undo/redo
- Performance is improved (only one render per undo instead of two)

- [ ] **Step 5: Commit**

```bash
git add src/widgets/capture/capturewidget.cpp
git commit -m "perf: remove double drawToolsData() in undo/redo after render cache landed"
```
