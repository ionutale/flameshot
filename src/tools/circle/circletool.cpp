// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "circletool.h"
#include "utils/colorutils.h"

#include <QPainter>

CircleTool::CircleTool(QObject* parent)
  : AbstractTwoPointTool(parent)
{
    m_supportsDiagonalAdj = true;
}

QIcon CircleTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "circle-outline.svg");
}
QString CircleTool::name() const
{
    return tr("Circle");
}

CaptureTool::Type CircleTool::type() const
{
    return CaptureTool::TYPE_CIRCLE;
}

QString CircleTool::description() const
{
    return tr("Set the Circle as the paint tool");
}

CaptureTool* CircleTool::copy(QObject* parent)
{
    auto* tool = new CircleTool(parent);
    copyParams(this, tool);
    return tool;
}

void CircleTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    QColor borderColor = ColorUtils::contrastColor(color());
    QColor fillColor(color().red(), color().green(), color().blue(), 20);
    int w = size();
    QRect r(points().first, points().second);
    QPoint offset(2, 2);
    // Shadow
    painter.setPen(QPen(QColor(0, 0, 0, 40), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(r.translated(offset));
    // Subtle fill
    painter.setPen(Qt::NoPen);
    painter.setBrush(fillColor);
    painter.drawEllipse(r);
    // Border
    painter.setPen(QPen(borderColor, w + 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(r);
    // Main outline
    painter.setPen(QPen(color(), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawEllipse(r);
}

void CircleTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
