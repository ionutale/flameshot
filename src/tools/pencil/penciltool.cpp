// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "penciltool.h"
#include "utils/colorutils.h"

#include <QPainter>
#include <QPainterPath>

PencilTool::PencilTool(QObject* parent)
  : AbstractPathTool(parent)
{}

QIcon PencilTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "pencil.svg");
}
QString PencilTool::name() const
{
    return tr("Pencil");
}

CaptureTool::Type PencilTool::type() const
{
    return CaptureTool::TYPE_PENCIL;
}

QString PencilTool::description() const
{
    return tr("Set the Pencil as the paint tool");
}

CaptureTool* PencilTool::copy(QObject* parent)
{
    auto* tool = new PencilTool(parent);
    copyParams(this, tool);
    return tool;
}

QPainterPath PencilTool::smoothPath() const
{
    QPainterPath path;
    if (m_points.size() < 2) {
        if (m_points.size() == 1) {
            path.addEllipse(m_points[0], size() / 2.0, size() / 2.0);
        }
        return path;
    }
    path.moveTo(m_points[0]);
    for (int i = 1; i < m_points.size(); i++) {
        QPointF p0 = (i > 1) ? m_points[i - 2] : m_points[i - 1];
        QPointF p1 = m_points[i - 1];
        QPointF p2 = m_points[i];
        QPointF p3 = (i < m_points.size() - 1) ? m_points[i + 1] : m_points[i];
        float t = 0.5;
        QPointF cp1 = p1 + (p2 - p0) * t / 6;
        QPointF cp2 = p2 - (p3 - p1) * t / 6;
        path.cubicTo(cp1, cp2, p2);
    }
    return path;
}

void PencilTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    QColor borderColor = ColorUtils::contrastColor(m_color);
    int w = size();
    QPoint offset(2, 2);
    QPainterPath path = smoothPath();
    if (path.isEmpty()) {
        return;
    }
    // Shadow
    painter.setPen(QPen(QColor(0, 0, 0, 40), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.translate(offset);
    painter.drawPath(path);
    painter.translate(-offset);
    // Border
    painter.setPen(QPen(borderColor, w + 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
    // Main
    painter.setPen(QPen(m_color, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(path);
}

void PencilTool::paintMousePreview(QPainter& painter,
                                   const CaptureContext& context)
{
    painter.setPen(QPen(context.color, context.toolSize + 2));
    painter.drawLine(context.mousePos, context.mousePos);
}

void PencilTool::drawStart(const CaptureContext& context)
{
    m_color = context.color;
    onSizeChanged(context.toolSize);
    m_points.append(context.mousePos);
    m_pathArea.setTopLeft(context.mousePos);
    m_pathArea.setBottomRight(context.mousePos);
}

void PencilTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
