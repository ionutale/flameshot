# UI Refinement Implementation Plan

> **For agentic workers:** Implement each task in order. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Update default colors and add visual polish to buttons, panels, and selection.

**Architecture:** Only default values and widget stylesheets change — no structural refactoring.

**Tech Stack:** C++17, Qt 5.15+, QSS stylesheets, QPainter

---

### Task 1: Update default colors in ConfigHandler

**Files:**
- Modify: `src/utils/confighandler.cpp:110-111`

- [ ] Update `uiColor` default from `116,0,150` to `155,89,182` (`#9B59B6`)
- [ ] Update `contrastUiColor` default from `39,0,50` to `108,52,131` (`#6C3483`)

```cpp
OPTION("uiColor"             , Color( {155, 89, 182} )),
OPTION("contrastUiColor"     , Color( {108, 52, 131} )),
```

- [ ] Commit: `git add src/utils/confighandler.cpp && git commit -m "refactor(ui): soften default purple colors"`

### Task 2: Add border and refined shadow to CaptureButton

**Files:**
- Modify: `src/widgets/capture/capturebutton.cpp:styleSheet()`

- [ ] In `CaptureButton::styleSheet()`, add a 1px border and richer shadow:

Current code produces something like:
```
"CaptureButton { background-color: %1; color: %2; } "
"CaptureButton:hover { background-color: %3; } "
```

Change to include border and refined shadow:
```
"CaptureButton { background-color: %1; color: %2; "
"  border: 1px solid rgba(%4, %5, %6, 0.15); "
"} "
"CaptureButton:hover { background-color: %3; } "
```

Where `%4,%5,%6` are RGB of `contrastUiColor`. The existing shadow setup via `QGraphicsDropShadowEffect` stays — only adjust it to `blurRadius=6` (was 5).

- [ ] Commit: `git add src/widgets/capture/capturebutton.cpp && git commit -m "feat(ui): add border and refined shadow to tool buttons"`

### Task 3: Theme the side panel

**Files:**
- Modify: `src/widgets/panel/sidepanelwidget.cpp`

- [ ] In `SidePanelWidget` constructor (after `setupUi`), apply a stylesheet:

```cpp
setStyleSheet(
    "SidePanelWidget { background-color: #F5F0FF; }"
    "QWidget { background-color: #F5F0FF; }"
    "QLabel { color: #6C3483; }"
    "QGroupBox { background-color: #F5F0FF; border: 1px solid #EDE4F5;"
    "  border-radius: 6px; margin-top: 12px; padding-top: 12px; }"
    "QGroupBox::title { color: #6C3483; font-weight: 600; }"
    "QPushButton { background-color: #EDE4F5; border: 1px solid #E0D0EE;"
    "  border-radius: 4px; padding: 4px 12px; }"
    "QPushButton:hover { background-color: #E0D0EE; }"
    "QSlider::groove:horizontal { background: #EDE4F5; height: 4px;"
    "  border-radius: 2px; }"
    "QSlider::handle:horizontal { background: #9B59B6; width: 14px;"
    "  height: 14px; border-radius: 7px; margin: -5px 0; }"
);
```

- [ ] Commit: `git add src/widgets/panel/sidepanelwidget.cpp && git commit -m "feat(ui): theme side panel with lilac palette"`

### Task 4: Theme the utility panel

**Files:**
- Modify: `src/widgets/panel/utilitypanel.cpp`

- [ ] In `UtilityPanel` constructor, apply a stylesheet matching the side panel:

```cpp
setStyleSheet(
    "UtilityPanel { background-color: #F5F0FF; }"
    "QWidget { background-color: #F5F0FF; }"
    "QListWidget { background-color: #F5F0FF; border: 1px solid #EDE4F5;"
    "  border-radius: 6px; }"
    "QListWidget::item { border-radius: 4px; padding: 4px; }"
    "QListWidget::item:selected { background-color: #EDE4F5; color: #6C3483; }"
    "QPushButton { background-color: #EDE4F5; border: 1px solid #E0D0EE;"
    "  border-radius: 4px; padding: 4px 12px; }"
    "QPushButton:hover { background-color: #E0D0EE; }"
);
```

- [ ] Commit: `git add src/widgets/panel/utilitypanel.cpp && git commit -m "feat(ui): theme utility panel with lilac palette"`

### Task 5: Update selection border to white with purple glow

**Files:**
- Modify: `src/widgets/capture/selectionwidget.cpp`

- [ ] In `SelectionWidget::paintEvent`, change the border drawing:

Current: draws border with `m_color` (uiColor).
Change: draw the border with `QColor(Qt::white)`, 2px width. Add a purple glow by painting a wider, semi-transparent version of `m_color` around the border.

The glow can be done by painting the same rect outline with `m_color` at alpha=80 and pen width 6px (behind the white border).

```cpp
// Glow layer (behind)
QPen glowPen(m_color, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
glowPen.setColor(QColor(m_color.red(), m_color.green(), m_color.blue(), 80));
painter.setPen(glowPen);
painter.drawRect(rect);

// White border on top
QPen borderPen(Qt::white, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
painter.setPen(borderPen);
painter.drawRect(rect);
```

Handles stay using `m_color` (they already do, no change needed).

- [ ] Commit: `git add src/widgets/capture/selectionwidget.cpp && git commit -m "feat(ui): change selection border to white with purple glow"`

### Task 6: Final commit and push

- [ ] `git push origin master`
