// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "pixelatetool.h"
#include "utils/confighandler.h"

#include <QApplication>
#include <QGraphicsBlurEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QImage>
#include <QPainter>
#include <array>

#include <random>

PixelateTool::PixelateTool(QObject* parent)
  : AbstractTwoPointTool(parent)
{}

QIcon PixelateTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor)
    return QIcon(iconPath(background) + "pixelate.svg");
}

QString PixelateTool::name() const
{
    return tr("Pixelate");
}

CaptureTool::Type PixelateTool::type() const
{
    return CaptureTool::TYPE_PIXELATE;
}

QString PixelateTool::description() const
{
    return tr("Set Pixelate as the paint tool.");
}

QRect PixelateTool::recomputeBoundingRect() const
{
    return QRect(points().first, points().second).normalized();
}

CaptureTool* PixelateTool::copy(QObject* parent)
{
    auto* tool = new PixelateTool(parent);
    copyParams(this, tool);
    return tool;
}

/**
 * Since pixelation does not protect the contents of the pixelated area
 * (see e.g. https://github.com/bishopfox/unredacter),
 * _pseudo-pixelation_ is used:
 *
 * Only colors from the fringe of the selected area are used to generate
 * a pixelation-like effect. The interior of the selected area is not used
 * as an input at all and hence can not be recovered.
 *
 */
void PixelateTool::process(QPainter& painter, const QPixmap& pixmap)
{
    bool useInsecurePixelate = ConfigHandler().insecurePixelate();

    QRect selection = boundingRect().intersected(pixmap.rect());
    auto pixelRatio = pixmap.devicePixelRatio();
    QRect selectionScaled = QRect(selection.topLeft() * pixelRatio,
                                  selection.bottomRight() * pixelRatio);

    const auto width =
      static_cast<int>(selection.width() * (0.5 / qMax(1, size() + 1)));
    const auto height =
      static_cast<int>(selection.height() * (0.5 / qMax(1, size() + 1)));
    const auto effectSize = QSize(qMax(width, 1), qMax(height, 1));

    if (useInsecurePixelate) {
        if (size() <= 1) {
            auto* blur = new QGraphicsBlurEffect(this);
            blur->setBlurRadius(10);
            auto* item = new QGraphicsPixmapItem(pixmap.copy(selectionScaled));
            item->setGraphicsEffect(blur);

            QGraphicsScene scene;
            scene.addItem(item);

            scene.render(&painter, selection, QRectF());
            blur->setBlurRadius(12);
            // multiple repeat for make blur effect stronger
            scene.render(&painter, selection, QRectF());

        } else {
            auto pixmapPixelated = pixmap.copy(selectionScaled);
            pixmapPixelated = pixmapPixelated.scaled(
              effectSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            pixmapPixelated =
              pixmapPixelated.scaled(selection.width(), selection.height());
            painter.drawImage(selection, pixmapPixelated.toImage());
        }
    } else {
        // the PRNG is only used for visual effects and NOT part of the security
        // boundary
        std::mt19937 prng(42);

        // noise for the sampling process to avoid only sampling from a small
        // subset of the fringe
        std::normal_distribution<float> sampling_noise(0, 5 * size() + 1);

        // additional noise that will be added on top of the effect to avoid
        // generating a monochromatic box when the fringe is monochromatic
        std::normal_distribution<float> noise(0, 0.1f);

        QPoint const offset_top(0, selectionScaled.topLeft().y() == 0 ? 0 : -1);
        QPoint const offset_bottom(0,
                                   selectionScaled.bottomLeft().y() ==
                                       pixmap.rect().bottomLeft().y()
                                     ? 0
                                     : 1);
        QPoint const offset_left(selectionScaled.topLeft().x() == 0 ? 0 : -1,
                                 0);
        QPoint const offset_right(
          selectionScaled.topRight().x() == pixmap.rect().topRight().x() ? 0
                                                                         : 1,
          0);

        // only values from the fringe will be used to compute the
        // pseudo-pixelation
        std::array<QImage, 4> fringe = {
            // top fringe
            pixmap
              .copy(QRect(selectionScaled.topLeft() + offset_top,
                          selectionScaled.topRight() + offset_top))
              .toImage(),
            // bottom fringe
            pixmap
              .copy(QRect(selectionScaled.bottomLeft() + offset_bottom,
                          selectionScaled.bottomRight() + offset_bottom))
              .toImage(),
            // left fringe
            pixmap
              .copy(QRect(selectionScaled.topLeft() + offset_left,
                          selectionScaled.bottomLeft() + offset_left))
              .toImage(),
            // right fringe
            pixmap
              .copy(QRect(selectionScaled.topRight() + offset_right,
                          selectionScaled.bottomRight() + offset_right))
              .toImage()
        };

        // Image where the pseudo-pixelation is calculated.
        // This will later be scaled to cover the selected area.
        QImage pixelated = QImage(effectSize, QImage::Format_RGB32);

        // Direct buffer access for fringes
        const uchar* fringeBits[4];
        int fringeStride[4];
        for (int i = 0; i < 4; ++i) {
            fringeBits[i] = fringe[i].constBits();
            fringeStride[i] = fringe[i].bytesPerLine();
        }

        // Direct buffer access for pixelated output
        QRgb* pixelatedBits = reinterpret_cast<QRgb*>(pixelated.bits());
        int pixelatedStride = pixelated.bytesPerLine() / sizeof(QRgb);

        // For every pixel of the effect, we consider four projections
        // to the fringe and sample a pixel from there.
        // Then a horizontal and vertical interpolation are calculated.
        std::array<std::array<float, 3>, 4> samples;

        for (int x = 0; x < width; ++x) {
            for (int y = 0; y < height; ++y) {
                float n = noise(prng);

                // relative horizontal resp. vertical position
                float const horizontal = x / (float)width;
                float const vertical = y / (float)height;

                for (int i = 0; i < 4; ++i) {
                    float sx = sampling_noise(prng);
                    float sy = sampling_noise(prng);
                    int fx = std::clamp(
                      static_cast<int>(horizontal * fringe[i].width() + sx),
                      0,
                      fringe[i].width() - 1);
                    int fy = std::clamp(
                      static_cast<int>(vertical * fringe[i].height() + sy),
                      0,
                      fringe[i].height() - 1);
                    const QRgb* pixelPtr = reinterpret_cast<const QRgb*>(
                      fringeBits[i] + fy * fringeStride[i] +
                      fx * (fringe[i].depth() / 8));
                    samples[i][0] = qRed(*pixelPtr) / 255.0f;
                    samples[i][1] = qGreen(*pixelPtr) / 255.0f;
                    samples[i][2] = qBlue(*pixelPtr) / 255.0f;
                }

                // weights of the horizontal resp. vertical interpolation
                float const weight_h = (qMin(x, width - x) / width) -
                                       (qMin(y, height - y) / height) + 0.5;

                float const weight_v = 1 - weight_h;

                // compute the weighted sum of the vertical and horizontal
                // interpolations
                std::array<int, 3> rgb = { 0, 0, 0 };
                for (int i = 0; i < 3; ++i) {
                    float c =
                      // horizontal interpolation
                      weight_h * ((1 - horizontal) * samples[2][i] +
                                  horizontal * samples[3][i])

                      // vertical interpolation
                      + weight_v * ((1 - vertical) * samples[0][i] +
                                    vertical * samples[1][i])

                      // additional noise
                      + n;

                    rgb[i] = static_cast<int>(0xff * c);
                    rgb[i] = std::clamp(rgb[i], 0, 0xff);
                }
                QRgb const value = qRgb(rgb[0], rgb[1], rgb[2]);
                QRgb* dest = pixelatedBits + y * pixelatedStride;
                dest[x] = value;
            }
        }

        pixelated = pixelated.scaled(selection.width(),
                                     selection.height(),
                                     Qt::IgnoreAspectRatio,
                                     Qt::FastTransformation);

        painter.drawImage(selection, pixelated);
    }
}

void PixelateTool::drawSearchArea(QPainter& painter, const QPixmap& pixmap)
{
    Q_UNUSED(pixmap)
    painter.fillRect(boundingRect(), QBrush(Qt::black));
}

void PixelateTool::paintMousePreview(QPainter& painter,
                                     const CaptureContext& context)
{
    Q_UNUSED(context)
    Q_UNUSED(painter)
}

void PixelateTool::pressed(CaptureContext& context)
{
    Q_UNUSED(context)
}
