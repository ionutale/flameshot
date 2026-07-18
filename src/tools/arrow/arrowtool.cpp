// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "arrowtool.h"
#include "utils/colorutils.h"
#include "utils/confighandler.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainterPath>
#include <QWidget>
#include <cmath>

namespace {
const int ArrowWidth = 10;
const int ArrowHeight = 18;

QPainterPath getArrowHead(QPointF p1, QPointF p2, const int thickness)
{
    QLineF line(p1, p2);
    if (line.length() <= 0) {
        return {};
    }

    const QPointF direction = (p2 - p1) / line.length();
    const QPointF normal(-direction.y(), direction.x());
    const qreal halfWidth = (ArrowWidth + thickness * 2) / 2.0;
    const qreal headLength = ArrowHeight + thickness * 4;
    const qreal baseDistance = qMin(line.length(), headLength);
    const qreal notchDepth = qMin(baseDistance * 0.45, halfWidth);

    const QPointF baseCenter = p2 - direction * (baseDistance - notchDepth);
    const QPointF baseLeft = baseCenter + normal * halfWidth;
    const QPointF baseRight = baseCenter - normal * halfWidth;
    const QPointF notch = baseCenter + direction * notchDepth;
    const QPointF leftControl = baseCenter + normal * halfWidth * 0.25;
    const QPointF rightControl = baseCenter - normal * halfWidth * 0.25;

    QPainterPath path;
    path.moveTo(p2);
    path.lineTo(baseLeft);
    path.quadTo(leftControl, notch);
    path.quadTo(rightControl, baseRight);
    path.lineTo(p2);
    return path;
}
} // unnamed namespace

ArrowTool::ArrowTool(QObject* parent)
  : AbstractTwoPointTool(parent)
{
    setPadding(ArrowWidth / 2);
    m_supportsOrthogonalAdj = true;
    m_supportsDiagonalAdj = true;
}

QIcon ArrowTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "arrow-bottom-left.svg");
}
QString ArrowTool::name() const
{
    return tr("Arrow");
}

CaptureTool::Type ArrowTool::type() const
{
    return CaptureTool::TYPE_ARROW;
}

QString ArrowTool::description() const
{
    return tr("Set the Arrow as the paint tool");
}

QRect ArrowTool::boundingRect() const
{
    if (!isValid()) {
        return {};
    }

    int offset = size() <= 1 ? 1 : static_cast<int>(round(size() / 2 + 0.5));

    // get min and max arrow pos
    int min_x = points().first.x();
    int min_y = points().first.y();
    int max_x = points().first.x();
    int max_y = points().first.y();
    for (int i = 0; i < m_arrowPath.elementCount(); i++) {
        QPointF pt = m_arrowPath.elementAt(i);
        if (static_cast<int>(pt.x()) < min_x) {
            min_x = static_cast<int>(pt.x());
        }
        if (static_cast<int>(pt.y()) < min_y) {
            min_y = static_cast<int>(pt.y());
        }
        if (static_cast<int>(pt.x()) > max_x) {
            max_x = static_cast<int>(pt.x());
        }
        if (static_cast<int>(pt.y()) > max_y) {
            max_y = static_cast<int>(pt.y());
        }
    }

    // get min and max line pos
    int line_pos_min_x =
      std::min(std::min(points().first.x(), points().second.x()), min_x);
    int line_pos_min_y =
      std::min(std::min(points().first.y(), points().second.y()), min_y);
    int line_pos_max_x =
      std::max(std::max(points().first.x(), points().second.x()), max_x);
    int line_pos_max_y =
      std::max(std::max(points().first.y(), points().second.y()), max_y);

    QRect rect = QRect(line_pos_min_x - offset,
                       line_pos_min_y - offset,
                       line_pos_max_x - line_pos_min_x + offset * 2,
                       line_pos_max_y - line_pos_min_y + offset * 2);

    return rect.normalized();
}

QWidget* ArrowTool::configurationWidget()
{
    return nullptr;
}

CaptureTool* ArrowTool::copy(QObject* parent)
{
    auto* tool = new ArrowTool(parent);
    copyParams(this, tool);
    return tool;
}

void ArrowTool::copyParams(const ArrowTool* from, ArrowTool* to)
{
    AbstractTwoPointTool::copyParams(from, to);
    to->m_arrowPath = from->m_arrowPath;
}

void ArrowTool::process(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    bool isArrowReversed = ConfigHandler().reverseArrow();
    const QPointF& head = isArrowReversed ? points().second : points().first;
    const QPointF& tail = isArrowReversed ? points().first : points().second;
    QColor borderColor = ColorUtils::contrastColor(color());
    int w = size();
    QPoint offset(2, 2);

    m_arrowPath = getArrowHead(head, tail, w);

    // Shadow (shaft + head)
    painter.setPen(
      QPen(QColor(0, 0, 0, 40), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(QColor(0, 0, 0, 40));
    painter.translate(offset);
    painter.drawLine(head, tail);
    painter.drawPath(m_arrowPath);
    painter.translate(-offset);

    // Shaft border
    painter.setPen(
      QPen(borderColor, w + 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawLine(head, tail);

    // Shaft main
    painter.setPen(
      QPen(color(), w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(head, tail);

    // Head border
    QPainterPathStroker stroker;
    stroker.setWidth(3);
    stroker.setCapStyle(Qt::RoundCap);
    stroker.setJoinStyle(Qt::RoundJoin);
    painter.setPen(Qt::NoPen);
    painter.fillPath(stroker.createStroke(m_arrowPath), borderColor);

    // Head fill
    painter.fillPath(m_arrowPath, color());
}

void ArrowTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
