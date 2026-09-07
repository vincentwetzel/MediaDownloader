#include "TestArtworkNormalizer.h"

#include "core/ArtworkNormalizer.h"

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QSize>
#include <QtTest/QtTest>

namespace {
QImage borderedSquareArtwork()
{
    QImage image(160, 90, QImage::Format_RGB32);
    image.fill(Qt::black);
    for (int y = 0; y < 90; ++y) {
        for (int x = 35; x < 125; ++x) {
            image.setPixelColor(x, y, QColor((x * 3) % 256, (y * 5) % 256, 180));
        }
    }
    return image;
}

QImage genuineLandscapeArtwork()
{
    QImage image(160, 90, QImage::Format_RGB32);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            image.setPixelColor(x, y, QColor((x * 7) % 256, (y * 11) % 256, (x + y) % 256));
        }
    }
    return image;
}
}

void TestArtworkNormalizer::testCropsSymmetricBordersAroundSquareArtwork()
{
    const QImage image = borderedSquareArtwork();
    QCOMPARE(ArtworkNormalizer::detectSquareArtworkCrop(image), QRect(35, 0, 90, 90));
}

void TestArtworkNormalizer::testPreservesGenuineLandscapeArtwork()
{
    QVERIFY(ArtworkNormalizer::detectSquareArtworkCrop(genuineLandscapeArtwork()).isEmpty());
}

void TestArtworkNormalizer::testCropsVerticalBordersAroundSquareArtwork()
{
    QImage image(90, 160, QImage::Format_RGB32);
    image.fill(Qt::black);
    for (int y = 35; y < 125; ++y) {
        for (int x = 0; x < 90; ++x) {
            image.setPixelColor(x, y, QColor(40, 120, 200));
        }
    }

    QCOMPARE(ArtworkNormalizer::detectSquareArtworkCrop(image), QRect(0, 35, 90, 90));
}

void TestArtworkNormalizer::testNormalizesJpegInPlace()
{
    const QString path = QDir(getTempDir()).filePath(QStringLiteral("artwork.jpg"));
    // The Windows vcpkg test runtime deploys qjpeg consistently. Keep this
    // file-backed regression independent of optional qpng deployment; PNG
    // decoding is covered by the production Qt feature selection.
    QVERIFY(borderedSquareArtwork().save(path, "JPEG"));
    QVERIFY(QFileInfo::exists(path));
    QVERIFY(ArtworkNormalizer::normalizeFile(path));

    const QImage normalized(path);
    QCOMPARE(normalized.size(), QSize(90, 90));
    QVERIFY(normalized.pixelColor(0, 0).blue() > 150);
}

QTEST_MAIN(TestArtworkNormalizer)
