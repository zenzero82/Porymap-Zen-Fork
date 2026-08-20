#pragma once

#include <QImage>
#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>

#include <cstdint>

#include "studio/metatilerenderservice.h"

namespace Studio {

class ImageMetatileMatcher
{
public:
    struct CellResult {
        QPoint position;
        QImage sourceImage;
        bool matched = false;
        uint16_t metatileId = 0;
        MetatileRenderService::SourceTileset sourceTileset = MetatileRenderService::SourceTileset::Primary;
        QString sourceTilesetName;
    };

    struct Result {
        QSize mapSize;
        QList<CellResult> cells;
        QImage reconstructedImage;
        QImage differenceImage;
        int exactMatchCount = 0;
        int unmatchedCount = 0;
        int primaryMatchCount = 0;
        int secondaryMatchCount = 0;
        QString errorMessage;

        bool isValid() const { return errorMessage.isEmpty(); }
    };

    Result match(
        const QImage &sourceImage,
        const QSize &mapSize,
        const QSize &metatilePixelSize,
        const QList<MetatileRenderService::RenderedMetatile> &candidates) const;
};

} // namespace Studio