// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "rectangletool.h"
#include "utils/colorutils.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

RectangleTool::RectangleTool(QObject* parent)
  : AbstractTwoPointTool(parent)
{
    m_supportsDiagonalAdj = true;
}

QIcon RectangleTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "square.svg");
}
QString RectangleTool::name() const
{
    return tr("Rectangle");
}

CaptureTool::Type RectangleTool::type() const
{
    return CaptureTool::TYPE_RECTANGLE;
}

QString RectangleTool::description() const
{
    return tr("Set the Rectangle as the paint tool");
}

CaptureTool* RectangleTool::copy(QObject* parent)
{
    auto* tool = new RectangleTool(parent);
    copyParams(this, tool);
    return tool;
}

void RectangleTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    QPen orig_pen = painter.pen();
    QBrush orig_brush = painter.brush();
    QColor borderColor = ColorUtils::contrastColor(color());
    int w = size();
    int cornerRadius = qMax(w, 1);
    QPoint offset(2, 2);

    auto rect = QRect(points().first, points().second);
    if (w <= 5) {
        // Thin: stroked outline with rounded corners
        QPainterPath path;
        path.addRoundedRect(QRectF(rect), cornerRadius, cornerRadius);
        // Shadow
        painter.setPen(QPen(
          QColor(0, 0, 0, 40), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.translate(offset);
        painter.drawPath(path);
        painter.translate(-offset);
        // Border
        painter.setPen(QPen(
          borderColor, w + 2, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
        painter.drawPath(path);
        // Main
        painter.setPen(
          QPen(color(), w, Qt::SolidLine, Qt::SquareCap, Qt::RoundJoin));
        painter.drawPath(path);
    } else {
        // Thick: filled rounded rect
        int pad = w <= 1 ? 1 : static_cast<int>(round(w / 2 + 0.5));
        QPainterPath path;
        path.addRoundedRect(
          QRectF(std::min(points().first.x(), points().second.x()) - pad,
                 std::min(points().first.y(), points().second.y()) - pad,
                 std::abs(points().first.x() - points().second.x()) + pad * 2,
                 std::abs(points().first.y() - points().second.y()) + pad * 2),
          w, w);
        // Shadow
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 40));
        painter.translate(offset);
        painter.drawPath(path);
        painter.translate(-offset);
        // Border
        QPainterPathStroker stroker;
        stroker.setWidth(3);
        stroker.setJoinStyle(Qt::RoundJoin);
        painter.fillPath(stroker.createStroke(path), borderColor);
        // Main fill
        painter.fillPath(path, color());
    }
    painter.setPen(orig_pen);
    painter.setBrush(orig_brush);
}

void RectangleTool::drawStart(const CaptureContext& context)
{
    AbstractTwoPointTool::drawStart(context);
    onSizeChanged(context.toolSize);
}

void RectangleTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
