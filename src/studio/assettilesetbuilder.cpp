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
        assets.append({image, path});
    }

    const int atlasTilesWide = 16;
    const int atlasTileRows = qMax(
        2,
        ((totalTiles + atlasTilesWide - 1) / atlasTilesWide + 1) / 2 * 2
    );
    QImage atlas(
        atlasTilesWide * Tile::pixelWidth(),
        atlasTileRows * Tile::pixelHeight(),
        QImage::Format_ARGB32
    );
    atlas.fill(Qt::transparent);
    QPainter painter(&atlas);
    int tileIndex = 0;
    for (const LoadedAsset &asset : assets) {
        const int tilesWide =
            (asset.image.width() + Tile::pixelWidth() - 1) / Tile::pixelWidth();
        const int tilesHigh =
            (asset.image.height() + Tile::pixelHeight() - 1) / Tile::pixelHeight();
        for (int tileY = 0; tileY < tilesHigh; tileY++) {
            for (int tileX = 0; tileX < tilesWide; tileX++) {
                const QRect sourceRect(
                    tileX * Tile::pixelWidth(),
                    tileY * Tile::pixelHeight(),
                    Tile::pixelWidth(),
                    Tile::pixelHeight()
                );
                const QRect targetRect(
                    (tileIndex % atlasTilesWide) * Tile::pixelWidth(),
                    (tileIndex / atlasTilesWide) * Tile::pixelHeight(),
                    Tile::pixelWidth(),
                    Tile::pixelHeight()
                );
                painter.drawImage(targetRect, asset.image, sourceRect);
                tileIndex++;
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