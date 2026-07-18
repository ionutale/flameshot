// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2021 Yurii Puchkov & Contributors

#include "capturetoolobjects.h"

#define SEARCH_RADIUS_NEAR 3
#define SEARCH_RADIUS_FAR 5
#define SEARCH_RADIUS_TEXT_HANDICAP 5

CaptureToolObjects::CaptureToolObjects(QObject* parent)
  : QObject(parent)
{}

void CaptureToolObjects::append(const QPointer<CaptureTool>& captureTool)
{
    if (!captureTool.isNull()) {
        m_captureToolObjects.append(captureTool->copy(captureTool->parent()));
        m_imageCache.clear();
    }
}

void CaptureToolObjects::insert(int index,
                                const QPointer<CaptureTool>& captureTool)
{
    if (!captureTool.isNull() && index >= 0 &&
        index <= m_captureToolObjects.size()) {
        m_captureToolObjects.insert(index,
                                    captureTool->copy(captureTool->parent()));
        m_imageCache.clear();
    }
}

QPointer<CaptureTool> CaptureToolObjects::at(int index)
{
    if (index >= 0 && index < m_captureToolObjects.size()) {
        return m_captureToolObjects[index];
    }
    return nullptr;
}

void CaptureToolObjects::clear()
{
    m_captureToolObjects.clear();
    m_imageCache.clear();
}

QList<QPointer<CaptureTool>> CaptureToolObjects::captureToolObjects()
{
    return m_captureToolObjects;
}

int CaptureToolObjects::size()
{
    return m_captureToolObjects.size();
}

void CaptureToolObjects::removeAt(int index)
{
    if (index >= 0 && index < m_captureToolObjects.size()) {
        m_captureToolObjects.removeAt(index);
        m_imageCache.clear();
    }
}

int CaptureToolObjects::find(const QPoint& pos, QSize captureSize)
{
    if (m_captureToolObjects.empty()) {
        return -1;
    }
    QPixmap pixmap(captureSize);
    pixmap.fill(Qt::transparent);
    int index = findWithRadius(pixmap, pos, SEARCH_RADIUS_NEAR);
    if (-1 == index) {
        index = findWithRadius(pixmap, pos, SEARCH_RADIUS_FAR);
    }
    return index;
}

int CaptureToolObjects::findWithRadius(QPixmap& pixmap,
                                       const QPoint& pos,
                                       int radius)
{
    for (int index = m_captureToolObjects.size() - 1; index >= 0; --index) {
        auto toolItem = m_captureToolObjects.at(index);
        if (toolItem.isNull())
            continue;

        // Geometric prefilter: skip if click is outside bounding rect + radius
        // margin
        QRect toolRect = toolItem->boundingRect();
        if (!toolRect.isEmpty() &&
            !toolRect.adjusted(-radius, -radius, radius, radius)
               .contains(pos)) {
            continue;
        }

        int currentRadius = radius;
        QImage image;

        // Use cached image if available (cache may be sparsely populated due to
        // geometric prefilter skipping some tools)
        if (index < m_imageCache.size() && !m_imageCache.at(index).isNull()) {
            image = m_imageCache.at(index);
        } else {
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            toolItem->drawSearchArea(painter, pixmap);
            painter.end();
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

        // Direct buffer access instead of QImage::pixel()
        // Runtime format check (Q_ASSERT is compiled out in release builds)
        bool canUseDirectAccess =
          image.format() == QImage::Format_ARGB32_Premultiplied ||
          image.format() == QImage::Format_ARGB32 ||
          image.format() == QImage::Format_RGB32;
        const uchar* bits = image.constBits();
        int bytesPerLine = image.bytesPerLine();
        int xMin = qMax(0, pos.x() - currentRadius);
        int xMax = qMin(image.width() - 1, pos.x() + currentRadius);
        int yMin = qMax(0, pos.y() - currentRadius);
        int yMax = qMin(image.height() - 1, pos.y() + currentRadius);

        if (canUseDirectAccess) {
            for (int y = yMin; y <= yMax; ++y) {
                const QRgb* line =
                  reinterpret_cast<const QRgb*>(bits + y * bytesPerLine);
                for (int x = xMin; x <= xMax; ++x) {
                    if (line[x] != 0) {
                        return index;
                    }
                }
            }
        } else {
            for (int y = yMin; y <= yMax; ++y) {
                for (int x = xMin; x <= xMax; ++x) {
                    if (image.pixel(x, y) != 0) {
                        return index;
                    }
                }
            }
        }
    }
    return -1;
}

CaptureToolObjects& CaptureToolObjects::operator=(
  const CaptureToolObjects& other)
{
    m_imageCache.clear();
    // remove extra items for this if size is bigger
    while (this->m_captureToolObjects.size() >
           other.m_captureToolObjects.size()) {
        this->m_captureToolObjects.removeLast();
    }

    int count = 0;
    for (const auto& item : other.m_captureToolObjects) {
        QPointer<CaptureTool> itemCopy = item->copy(item->parent());
        if (count < this->m_captureToolObjects.size()) {
            this->m_captureToolObjects[count] = itemCopy;
        } else {
            this->m_captureToolObjects.append(itemCopy);
        }
        count++;
    }
    return *this;
}
