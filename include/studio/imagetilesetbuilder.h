#pragma once

#include "metatile.h"

#include <QImage>
#include <QList>
#include <QSize>
#include <QString>
#include <QVector>

namespace Studio {

class ImageTilesetBuilder
{
public:
    struct Options {
        int maxTiles = 0;
        int maxMetatiles = 0;
        int tileIdBase = 0;
        int metatileIdBase = 0;
        int paletteId = 0;
        int tilesPerMetatile = Metatile::tilesPerLayer();
        QList<QRgb> paletteOverride;
    };

    struct Result {
        QSize mapSize;
        QImage indexedSource;
        QImage tilesImage;
        QList<QRgb> palette;
        QVector<Metatile> metatiles;
        QVector<uint16_t> mapMetatileIds;
        int sourceColorCount = 0;
        int uniqueTileCount = 0;
        int uniqueMetatileCount = 0;
        bool quantized = false;
        bool partialMap = false;
        QString errorMessage;

        bool isValid() const
        {
            return errorMessage.isEmpty()
                && !indexedSource.isNull()
                && !tilesImage.isNull()
                && !metatiles.isEmpty()
                && (partialMap || mapMetatileIds.size() == mapSize.width() * mapSize.height());
        }
    };

    struct PairResult {
        Result primary;
        Result secondary;
        QSize mapSize;
        QVector<uint16_t> mapMetatileIds;
        int sourceColorCount = 0;
        bool quantized = false;
        QString errorMessage;

        bool isValid() const
        {
            return errorMessage.isEmpty()
                && primary.isValid()
                && secondary.isValid()
                && mapMetatileIds.size() == mapSize.width() * mapSize.height();
        }
    };

    Result build(const QImage &sourceImage, const Options &options) const;
    PairResult buildPair(
        const QImage &sourceImage,
        const Options &primaryOptions,
        const Options &secondaryOptions
    ) const;
};

} // namespace Studio