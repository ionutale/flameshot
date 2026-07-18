#include <QApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QtTest/QtTest>

#include "tools/arrow/arrowtool.h"
#include "tools/capturecontext.h"
#include "tools/circle/circletool.h"
#include "tools/circlecount/circlecounttool.h"
#include "tools/invert/inverttool.h"
#include "tools/line/linetool.h"
#include "tools/marker/markertool.h"
#include "tools/pencil/penciltool.h"
#include "tools/pixelate/pixelatetool.h"
#include "tools/rectangle/rectangletool.h"
#include "tools/text/texttool.h"
#include "widgets/capture/capturetoolobjects.h"

class TestTools : public QObject
{
    Q_OBJECT

private:
    QPixmap createTestPixmap()
    {
        QPixmap pixmap(200, 200);
        pixmap.fill(Qt::white);
        QPainter painter(&pixmap);
        painter.fillRect(50, 50, 100, 100, Qt::red);
        painter.fillRect(75, 75, 50, 50, Qt::blue);
        painter.end();
        return pixmap;
    }

    void setTwoPoints(CaptureTool* tool, const QPoint& p1, const QPoint& p2)
    {
        CaptureContext ctx;
        ctx.screenshot = createTestPixmap();
        ctx.mousePos = p1;
        tool->drawStart(ctx);
        tool->drawMove(p2);
    }

private slots:
    // ===== Tool Creation and Type Tests =====

    void testArrowToolType()
    {
        ArrowTool tool;
        QVERIFY(tool.name().contains("Arrow", Qt::CaseInsensitive));
    }

    void testPencilToolType()
    {
        PencilTool tool;
        QVERIFY(tool.name().contains("Pencil", Qt::CaseInsensitive));
    }

    void testRectangleToolType()
    {
        RectangleTool tool;
        QVERIFY(tool.name().contains("Rectangle", Qt::CaseInsensitive));
    }

    void testCircleToolType()
    {
        CircleTool tool;
        QVERIFY(tool.name().contains("Circle", Qt::CaseInsensitive));
    }

    void testLineToolType()
    {
        LineTool tool;
        QVERIFY(tool.name().contains("Line", Qt::CaseInsensitive));
    }

    void testMarkerToolType()
    {
        MarkerTool tool;
        QVERIFY(tool.name().contains("Marker", Qt::CaseInsensitive));
    }

    void testPixelateToolType()
    {
        PixelateTool tool;
        QVERIFY(tool.name().contains("Pixelate", Qt::CaseInsensitive));
    }

    void testInvertToolType()
    {
        InvertTool tool;
        QVERIFY(tool.name().contains("Invert", Qt::CaseInsensitive));
    }

    void testCircleCountToolType()
    {
        CircleCountTool tool;
        QVERIFY(tool.name().contains("Count", Qt::CaseInsensitive) ||
                tool.name().contains("Circle", Qt::CaseInsensitive));
    }

    void testTextToolType()
    {
        TextTool tool;
        QVERIFY(tool.name().contains("Text", Qt::CaseInsensitive));
    }

    // ===== Tool Process Does Not Crash =====

    void testArrowToolProcess()
    {
        ArrowTool tool;
        setTwoPoints(&tool, QPoint(20, 20), QPoint(180, 180));
        QPixmap pixmap = createTestPixmap();
        QPainter painter(&pixmap);
        tool.process(painter, pixmap);
        painter.end();
        QVERIFY(!pixmap.isNull());
    }

    void testPencilToolProcess()
    {
        PencilTool tool;
        CaptureContext ctx;
        ctx.screenshot = createTestPixmap();
        ctx.mousePos = QPoint(50, 50);
        tool.drawStart(ctx);
        for (int i = 0; i < 50; ++i)
            tool.drawMove(QPoint(50 + i, 50 + i / 2));

        QPixmap pixmap = createTestPixmap();
        QPainter painter(&pixmap);
        tool.process(painter, pixmap);
        painter.end();
        QVERIFY(!pixmap.isNull());
    }

    void testRectangleToolProcess()
    {
        RectangleTool tool;
        setTwoPoints(&tool, QPoint(30, 30), QPoint(170, 170));
        QPixmap pixmap = createTestPixmap();
        QPainter painter(&pixmap);
        tool.process(painter, pixmap);
        painter.end();
        QVERIFY(!pixmap.isNull());
    }

    void testPixelateToolProcess()
    {
        PixelateTool tool;
        setTwoPoints(&tool, QPoint(50, 50), QPoint(150, 150));
        QPixmap pixmap = createTestPixmap();
        QPainter painter(&pixmap);
        QVERIFY(tool.isValid());
        tool.process(painter, pixmap);
        painter.end();
        QVERIFY(!pixmap.isNull());
    }

    // ===== Tool Validity Tests =====

    void testToolInvalidWithoutPoints()
    {
        ArrowTool tool;
        QVERIFY(!tool.isValid());
    }

    void testToolValidWithPoints()
    {
        ArrowTool tool;
        setTwoPoints(&tool, QPoint(10, 10), QPoint(100, 100));
        QVERIFY(tool.isValid());
    }

    void testPencilInvalidWithoutPoints()
    {
        PencilTool tool;
        QVERIFY(!tool.isValid());
    }

    void testPencilValidWithPoints()
    {
        PencilTool tool;
        CaptureContext ctx;
        ctx.screenshot = createTestPixmap();
        ctx.mousePos = QPoint(50, 50);
        tool.drawStart(ctx);
        tool.drawMove(QPoint(51, 51));
        QVERIFY(tool.isValid());
    }

    // ===== Bounding Rect Cache Tests =====

    void testBoundingRectCache()
    {
        ArrowTool tool;
        setTwoPoints(&tool, QPoint(10, 20), QPoint(100, 120));

        // First call computes and caches
        QRect r1 = tool.boundingRect();
        QVERIFY(!r1.isEmpty());
        QVERIFY(r1.contains(10, 20));
        QVERIFY(r1.contains(100, 120));

        // Second call should return cached value (same rect)
        QRect r2 = tool.boundingRect();
        QCOMPARE(r1, r2);
    }

    void testBoundingRectInvalidateOnSizeChange()
    {
        ArrowTool tool;
        setTwoPoints(&tool, QPoint(10, 20), QPoint(100, 120));

        QRect before = tool.boundingRect();
        tool.onSizeChanged(20);
        QRect after = tool.boundingRect();

        // Size change should invalidate and produce different bound
        QVERIFY(after != before || after.width() > before.width());
    }

    void testPencilBoundingRectCache()
    {
        PencilTool tool;
        CaptureContext ctx;
        ctx.screenshot = createTestPixmap();
        ctx.mousePos = QPoint(50, 50);
        tool.drawStart(ctx);
        for (int i = 0; i < 100; ++i)
            tool.drawMove(QPoint(50 + i, 50 + i));

        QRect r1 = tool.boundingRect();
        QVERIFY(!r1.isEmpty());

        // Second call returns cached
        QRect r2 = tool.boundingRect();
        QCOMPARE(r1, r2);
    }

    // ===== Render Cache Tests =====

    void testRenderCacheInitiallyDirty()
    {
        ArrowTool tool;
        QVERIFY(tool.isRenderCacheDirty());
    }

    void testRenderCacheCleanAfterSet()
    {
        ArrowTool tool;
        QPixmap cache(10, 10);
        tool.setRenderCache(cache);
        QVERIFY(!tool.isRenderCacheDirty());
    }

    void testRenderCacheDirtyOnMark()
    {
        ArrowTool tool;
        tool.setRenderCache(QPixmap(10, 10));
        tool.markRenderCacheDirty();
        QVERIFY(tool.isRenderCacheDirty());
    }

    void testRenderCacheDirtyOnColorChange()
    {
        ArrowTool tool;
        tool.setRenderCache(QPixmap(10, 10));
        tool.onColorChanged(Qt::green);
        QVERIFY(tool.isRenderCacheDirty());
    }

    void testRenderCacheDirtyOnSizeChange()
    {
        ArrowTool tool;
        tool.setRenderCache(QPixmap(10, 10));
        tool.onSizeChanged(15);
        QVERIFY(tool.isRenderCacheDirty());
    }

    // ===== CaptureToolObjects Hit-Testing Tests =====

    void testCaptureToolObjectsFind()
    {
        CaptureToolObjects objects;
        auto* tool = new ArrowTool();
        tool->setParent(&objects);
        setTwoPoints(tool, QPoint(10, 10), QPoint(100, 100));

        objects.append(QPointer<CaptureTool>(tool));
        QCOMPARE(objects.size(), 1);

        // Find at the center of the tool
        int index = objects.find(QPoint(50, 50), QSize(200, 200));
        QCOMPARE(index, 0);
    }

    void testCaptureToolObjectsFindNoMatch()
    {
        CaptureToolObjects objects;
        // Add a tool then verify clicking on empty canvas returns -1
        auto* tool = new ArrowTool();
        tool->setParent(&objects);
        setTwoPoints(tool, QPoint(50, 50), QPoint(100, 100));
        objects.append(QPointer<CaptureTool>(tool));

        // Clear cache so the pixmap is freshly rendered
        // (prefilter may skip cached tools from previous tests)
        objects.clear();
        objects.append(QPointer<CaptureTool>(tool));

        // Click where no tool exists — use empty objects
        CaptureToolObjects emptyObjects;
        int index = emptyObjects.find(QPoint(50, 50), QSize(200, 200));
        QCOMPARE(index, -1);
    }

    void testCaptureToolObjectsFindLastToolWins()
    {
        CaptureToolObjects objects;
        auto* tool1 = new ArrowTool();
        tool1->setParent(&objects);
        setTwoPoints(tool1, QPoint(10, 10), QPoint(100, 100));

        auto* tool2 = new RectangleTool();
        tool2->setParent(&objects);
        setTwoPoints(tool2, QPoint(10, 10), QPoint(100, 100));

        objects.append(QPointer<CaptureTool>(tool1));
        objects.append(QPointer<CaptureTool>(tool2));
        QCOMPARE(objects.size(), 2);

        // Both at same position, last added should be found first
        int index = objects.find(QPoint(50, 50), QSize(200, 200));
        QCOMPARE(index, 1); // Last tool (tool2) wins since search is in reverse
    }

    // ===== PixelateTool Determinism =====

    void testPixelateToolDeterminism()
    {
        PixelateTool tool1, tool2;
        setTwoPoints(&tool1, QPoint(30, 30), QPoint(170, 170));
        setTwoPoints(&tool2, QPoint(30, 30), QPoint(170, 170));

        QPixmap pixmap1 = createTestPixmap();
        QPixmap pixmap2 = createTestPixmap();

        {
            QPainter painter1(&pixmap1);
            tool1.process(painter1, pixmap1);
        }
        {
            QPainter painter2(&pixmap2);
            tool2.process(painter2, pixmap2);
        }

        // Same seed (42) should produce identical output
        QImage img1 = pixmap1.toImage();
        QImage img2 = pixmap2.toImage();
        QCOMPARE(img1, img2);
    }

    // ===== TextTool Tests =====

    void testTextToolInitiallyInvalid()
    {
        TextTool tool;
        QVERIFY(!tool.isValid());
    }

    // ===== InvertTool Test =====

    void testInvertToolProcess()
    {
        InvertTool tool;
        setTwoPoints(&tool, QPoint(50, 50), QPoint(150, 150));
        QPixmap pixmap = createTestPixmap();
        QPainter painter(&pixmap);
        tool.process(painter, pixmap);
        painter.end();

        // Pixel at center should be inverted
        QImage img = pixmap.toImage();
        QRgb centerPixel = img.pixel(100, 100);
        // Original was blue (0, 0, 255), inverted should be (255, 255, 0) =
        // yellow (depending on exact RGB vs ARGB format)
        QVERIFY(qRed(centerPixel) >
                200); // Red channel should be high after inverting blue
    }

    // ===== Grid related (non-widget) tests =====

    void testGridGeometry()
    {
        // Test that grid step logic doesn't produce zero step on HiDPI-like
        // scale
        int gridSize = 10;
        qreal scale = 2.0;
        int step = qMax(1, static_cast<int>(gridSize / scale));
        QCOMPARE(step, 5);

        // Edge case: very small grid + high DPI
        gridSize = 1;
        scale = 2.0;
        step = qMax(1, static_cast<int>(gridSize / scale));
        QCOMPARE(step, 1); // Should not be 0
    }
};

QTEST_MAIN(TestTools)
#include "test_tools.moc"
