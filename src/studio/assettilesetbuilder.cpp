#include "studio/assettilesetbuilder.h"

#include "tile.h"

#include <QImage>
#include <QPainter>

#include <cmath>

namespace Studio {

AssetTilesetBuilder::Result AssetTilesetBuilder::build(
    const QStringList &assetPaths,
    const ImageTilesetBuilder::Options &options) const
{
    Result result;
    result.assetPaths = assetPaths;
    if (assetPaths.isEmpty()) {
        result.errorMessage = QStringLiteral("Add at least one image asset.");
        return result;
    }

    struct LoadedAsset {
        QImage image;
        QString path;
    };
    QVector<LoadedAsset> assets;
    int totalTiles = 0;
    int totalMetatiles = 0;
    for (const QString &path : assetPaths) {
        QImage image(path);
        if (image.isNull()) {
            result.errorMessage = QString("Unable to load image asset '%1'.").arg(path);
            return result;
        }
        image = image.convertToFormat(QImage::Format_ARGB32);
        const int tilesWide = (image.width() + Tile::pixelWidth() - 1) / Tile::pixelWidth();
        const int tilesHigh = (image.height() + Tile::pixelHeight() - 1) / Tile::pixelHeight();
        if (tilesWide <= 0 || tilesHigh <= 0) {
            result.errorMessage = QString("Image asset '%1' is empty.").arg(path);
            return result;
        }
        totalTiles += tilesWide * tilesHigh;
        totalMetatiles +=
            ((image.width() + Metatile::pixelWidth() - 1) / Metatile::pixelWidth())
            * ((image.height() + Metatile::pixelHeight() - 1) / Metatile::pixelHeight());
        assets.append({image, path});
    }

    const int atlasMetatilesWide = 8;
    const int atlasMetatileRows =
        qMax(1, (totalMetatiles + atlasMetatilesWide - 1) / atlasMetatilesWide);
    QImage atlas(
        atlasMetatilesWide * Metatile::pixelWidth(),
        atlasMetatileRows * Metatile::pixelHeight(),
        QImage::Format_ARGB32
    );
    atlas.fill(Qt::transparent);
    QPainter painter(&atlas);
    int metatileIndex = 0;
    for (const LoadedAsset &asset : assets) {
        const int metatilesWide =
            (asset.image.width() + Metatile::pixelWidth() - 1) / Metatile::pixelWidth();
        const int metatilesHigh =
            (asset.image.height() + Metatile::pixelHeight() - 1) / Metatile::pixelHeight();
        for (int metatileY = 0; metatileY < metatilesHigh; metatileY++) {
            for (int metatileX = 0; metatileX < metatilesWide; metatileX++) {
                const QRect sourceRect(
                    metatileX * Metatile::pixelWidth(),
                    metatileY * Metatile::pixelHeight(),
                    Metatile::pixelWidth(),
                    Metatile::pixelHeight()
                );
                const QRect targetRect(
                    (metatileIndex % atlasMetatilesWide) * Metatile::pixelWidth(),
                    (metatileIndex / atlasMetatilesWide) * Metatile::pixelHeight(),
                    Metatile::pixelWidth(),
                    Metatile::pixelHeight()
                );
                painter.drawImage(targetRect, asset.image, sourceRect);
                metatileIndex++;
            }
        }
    }
    painter.end();

    result.sourceTileCount = totalTiles;
    result.sourceImage = atlas;
    result.tileset = ImageTilesetBuilder().build(atlas, options);
    if (!result.tileset.isValid()) {
        result.errorMessage = result.tileset.errorMessage;
    }
    return result;
}

} // namespace Studio