#include "ArtworkNormalizer.h"

#include <QFileInfo>
#include <QImageReader>
#include <QImageWriter>
#include <QSaveFile>

#include <cmath>

namespace {
constexpr int MAX_PROBE_DIMENSION = 320;
constexpr int MAX_IMAGE_DIMENSION = 16384;
constexpr qint64 MAX_IMAGE_PIXELS = 67108864;
constexpr double MIN_BORDER_FRACTION = 0.08;
constexpr double MAX_BORDER_DEVIATION = 22.0;
constexpr double MAX_BORDER_COLOR_DISTANCE = 30.0;

struct ColorStats {
    double red = 0.0;
    double green = 0.0;
    double blue = 0.0;
    double deviation = 0.0;
    int samples = 0;
};

ColorStats measureRegion(const QImage &image, const QRect &region)
{
    ColorStats stats;
    if (region.isEmpty()) {
        return stats;
    }

    const int step = qMax(1, qMax(region.width(), region.height()) / 96);
    double redSquares = 0.0;
    double greenSquares = 0.0;
    double blueSquares = 0.0;

    const int right = region.right();
    const int bottom = region.bottom();
    for (int y = region.top(); y <= bottom; y += step) {
        for (int x = region.left(); x <= right; x += step) {
            const QRgb pixel = image.pixel(x, y);
            const double red = qRed(pixel);
            const double green = qGreen(pixel);
            const double blue = qBlue(pixel);
            stats.red += red;
            stats.green += green;
            stats.blue += blue;
            redSquares += red * red;
            greenSquares += green * green;
            blueSquares += blue * blue;
            ++stats.samples;
        }
    }

    if (stats.samples == 0) {
        return stats;
    }

    stats.red /= stats.samples;
    stats.green /= stats.samples;
    stats.blue /= stats.samples;
    const double variance = ((redSquares / stats.samples) - stats.red * stats.red)
        + ((greenSquares / stats.samples) - stats.green * stats.green)
        + ((blueSquares / stats.samples) - stats.blue * stats.blue);
    stats.deviation = std::sqrt(qMax(0.0, variance / 3.0));
    return stats;
}

double colorDistance(const ColorStats &left, const ColorStats &right)
{
    const double red = left.red - right.red;
    const double green = left.green - right.green;
    const double blue = left.blue - right.blue;
    return std::sqrt((red * red) + (green * green) + (blue * blue));
}

bool isSafeImageSize(const QImage &image)
{
    return !image.isNull()
        && image.width() <= MAX_IMAGE_DIMENSION
        && image.height() <= MAX_IMAGE_DIMENSION
        && static_cast<qint64>(image.width()) * image.height() <= MAX_IMAGE_PIXELS;
}
}

QRect ArtworkNormalizer::detectSquareArtworkCrop(const QImage &source)
{
    if (!isSafeImageSize(source) || source.width() == source.height()) {
        return {};
    }

    const bool landscape = source.width() > source.height();
    const int majorDimension = landscape ? source.width() : source.height();
    const int minorDimension = landscape ? source.height() : source.width();
    if (minorDimension <= 0 || static_cast<double>(majorDimension) / minorDimension > 2.4) {
        return {};
    }

    QImage probe = source;
    if (qMax(probe.width(), probe.height()) > MAX_PROBE_DIMENSION) {
        const QSize probeSize = probe.size().scaled(MAX_PROBE_DIMENSION, MAX_PROBE_DIMENSION, Qt::KeepAspectRatio);
        probe = probe.scaled(probeSize, Qt::IgnoreAspectRatio, Qt::FastTransformation);
    }
    probe = probe.convertToFormat(QImage::Format_RGB32);

    const int probeMajor = landscape ? probe.width() : probe.height();
    const int probeMinor = landscape ? probe.height() : probe.width();
    const int probeBorder = (probeMajor - probeMinor) / 2;
    if (probeBorder <= 0 || static_cast<double>(probeBorder) / probeMajor < MIN_BORDER_FRACTION) {
        return {};
    }

    QRect firstBorder;
    QRect secondBorder;
    if (landscape) {
        firstBorder = QRect(0, 0, probeBorder, probe.height());
        secondBorder = QRect(probe.width() - probeBorder, 0, probeBorder, probe.height());
    } else {
        firstBorder = QRect(0, 0, probe.width(), probeBorder);
        secondBorder = QRect(0, probe.height() - probeBorder, probe.width(), probeBorder);
    }

    const ColorStats firstStats = measureRegion(probe, firstBorder);
    const ColorStats secondStats = measureRegion(probe, secondBorder);
    if (firstStats.samples == 0 || secondStats.samples == 0
        || firstStats.deviation > MAX_BORDER_DEVIATION
        || secondStats.deviation > MAX_BORDER_DEVIATION
        || colorDistance(firstStats, secondStats) > MAX_BORDER_COLOR_DISTANCE) {
        return {};
    }

    if (landscape) {
        const int x = (source.width() - source.height()) / 2;
        return QRect(x, 0, source.height(), source.height());
    }

    const int y = (source.height() - source.width()) / 2;
    return QRect(0, y, source.width(), source.width());
}

bool ArtworkNormalizer::normalizeFile(const QString &filePath)
{
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (!isSafeImageSize(image)) {
        return false;
    }

    const QRect crop = detectSquareArtworkCrop(image);
    if (crop.isEmpty() || crop == QRect(QPoint(0, 0), image.size())) {
        return false;
    }

    QByteArray format = reader.format();
    if (format.isEmpty()) {
        format = QFileInfo(filePath).suffix().toLatin1();
    }
    if (format.compare("jpg", Qt::CaseInsensitive) == 0) {
        format = "jpeg";
    }

    QSaveFile output(filePath);
    if (!output.open(QIODevice::WriteOnly)) {
        return false;
    }

    QImageWriter writer(&output, format);
    if (format.compare("jpeg", Qt::CaseInsensitive) == 0) {
        writer.setQuality(95);
    }
    if (!writer.write(image.copy(crop)) || !output.commit()) {
        output.cancelWriting();
        return false;
    }

    return true;
}
