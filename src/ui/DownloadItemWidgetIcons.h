#pragma once

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QMap>
#include <QPainter>
#include <QPair>
#include <QPixmap>
#include <QString>
#include <QPolygonF>
#include <QStyle>
#include <QStyleOption>

namespace DownloadItemWidgetIcons {

inline QIcon createColoredIcon(QStyle::StandardPixmap standardPixmap, const QColor &color)
{
    static QMap<QPair<int, QRgb>, QIcon> cache;
    const QPair<int, QRgb> key = qMakePair(static_cast<int>(standardPixmap), color.rgba());
    if (cache.contains(key)) {
        return cache.value(key);
    }

    QPixmap pixmap = QApplication::style()->standardIcon(standardPixmap).pixmap(32, 32);
    if (pixmap.isNull()) {
        return {};
    }

    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), color);

    QIcon icon;
    icon.addPixmap(pixmap, QIcon::Normal);
    QStyleOption styleOption;
    styleOption.palette = QApplication::palette();
    const QPixmap disabledPixmap = QApplication::style()->generatedIconPixmap(
        QIcon::Disabled, pixmap, &styleOption);
    if (!disabledPixmap.isNull()) {
        icon.addPixmap(disabledPixmap, QIcon::Disabled);
    }

    cache.insert(key, icon);
    return icon;
}

inline QIcon createMediaTypeIcon(const QString &type, const QColor &color)
{
    static QMap<QPair<QString, QRgb>, QIcon> cache;
    const QPair<QString, QRgb> key = qMakePair(type, color.rgba());
    if (cache.contains(key)) {
        return cache.value(key);
    }

    QPixmap pixmap(20, 20);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    if (type == QStringLiteral("audio")) {
        painter.drawLine(QPointF(4.0, 10.5), QPointF(8.0, 10.5));
        painter.drawLine(QPointF(8.0, 10.5), QPointF(8.0, 6.0));
        painter.drawLine(QPointF(8.0, 6.0), QPointF(13.0, 4.5));
        painter.drawLine(QPointF(13.0, 4.5), QPointF(13.0, 14.5));
        painter.drawArc(QRectF(10.0, 11.0, 6.0, 5.0), -55 * 16, 105 * 16);
    } else if (type == QStringLiteral("gallery")) {
        painter.drawRoundedRect(QRectF(2.0, 3.0, 16.0, 14.0), 1.5, 1.5);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(12.5, 5.0, 2.5, 2.5));
        painter.drawPolygon(QPolygonF({QPointF(3.5, 15.5), QPointF(8.0, 10.5),
                                       QPointF(10.5, 13.0), QPointF(13.0, 10.0),
                                       QPointF(17.0, 15.5)}));
    } else {
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(2.0, 4.0, 11.0, 12.0), 2.0, 2.0);
        painter.drawPolygon(QPolygonF({QPointF(14.0, 7.0), QPointF(18.0, 4.5),
                                       QPointF(18.0, 15.5), QPointF(14.0, 13.0)}));
    }

    QIcon icon(pixmap);
    cache.insert(key, icon);
    return icon;
}

} // namespace DownloadItemWidgetIcons
