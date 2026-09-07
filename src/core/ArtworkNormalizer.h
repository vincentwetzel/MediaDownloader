#pragma once

#include <QImage>
#include <QRect>
#include <QString>

/**
 * @brief Detects and removes high-confidence pillarbox/letterbox borders from artwork.
 *
 * The normalizer only crops when the image has substantial, matching low-variation
 * borders and the remaining centered image is square. Ambiguous artwork is left
 * untouched so normal landscape covers are not damaged.
 */
class ArtworkNormalizer final {
public:
    /**
     * @brief Returns the centered square crop for detected borders, or an empty rect.
     */
    [[nodiscard]] static QRect detectSquareArtworkCrop(const QImage &image);

    /**
     * @brief Normalizes a thumbnail in place when a high-confidence crop is found.
     * @return true when the file was rewritten, false when it was unchanged or could
     * not be safely rewritten.
     */
    [[nodiscard]] static bool normalizeFile(const QString &filePath);
};
