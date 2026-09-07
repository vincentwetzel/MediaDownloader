#pragma once

#include "BaseTest.h"

class TestArtworkNormalizer : public BaseTest {
    Q_OBJECT

private slots:
    void testCropsSymmetricBordersAroundSquareArtwork();
    void testPreservesGenuineLandscapeArtwork();
    void testCropsVerticalBordersAroundSquareArtwork();
    void testNormalizesPngInPlace();
};
