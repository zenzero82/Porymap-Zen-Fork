#include "studio/imagetilesetbuilder.h"

#include "tile.h"
#include "tileset.h"

#include <QByteArray>
#include <QHash>

#include <algorithm>
#include <cstring>
#include <limits>

namespace Studio {
namespace {

struct WeightedColor {
    QRgb rgb = 0;
    int count = 0;
};

struct ColorBox {
    QVector<WeightedColor> colors;
};

int channelRange(const ColorBox &box, int channel)
{
    int minimum = 255;
    int maximum = 0;
    for (const auto &color : box.colors) {
        const int value = channel == 0 ? qRed(color.rgb)
                        : channel == 1 ? qGreen(color.rgb)
                                       : qBlue(color.rgb);
        minimum = qMin(minimum, value);
        maximum = qMax(maximum, value);
    }
    return maximum - minimum;
}

int widestChannel(const ColorBox &box)
{
    int widest = 0;
    for (int channel = 1; channel < 3; channel++) {
        if (channelRange(box, channel) > channelRange(box, widest)) {
            widest = channel;
        }
    }
    return widest;
}

QRgb averageColor(const ColorBox &box)
{
    qint64 red = 0;
    qint64 green = 0;
    qint64 blue = 0;
    qint64 total = 0;
    for (const auto &color : box.colors) {
        red += static_cast<qint64>(qRed(color.rgb)) * color.count;
        green += static_cast<qint64>(qGreen(color.rgb)) * color.count;
        blue += static_cast<qint64>(qBlue(color.rgb)) * color.count;
        total += color.count;
    }
    if (total <= 0) return qRgb(0, 0, 0);
    return qRgb(red / total, green / total, blue / total);
}

QList<QRgb> buildPalette(const QImage &source, int *sourceColorCount, bool *quantized)
{
    QHash<QRgb, int> counts;
    const QImage normalized = source.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < normalized.height(); y++) {
        const QRgb *line = reinterpret_cast<const QRgb *>(normalized.constScanLine(y));
        for (int x = 0; x < normalized.width(); x++) {
            if (qAlpha(line[x]) >= 128) {
                counts[qRgb(qRed(line[x]), qGreen(line[x]), qBlue(line[x]))]++;
            }
        }
    }

    if (sourceColorCount) *sourceColorCount = counts.size();
    constexpr int visibleColorLimit = Tileset::numColorsPerPalette() - 1;
    if (quantized) *quantized = counts.size() > visibleColorLimit;

    QVector<WeightedColor> colors;
    colors.reserve(counts.size());
    for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
        colors.append({it.key(), it.value()});
    }

    QList<QRgb> visiblePalette;
    if (colors.size() <= visibleColorLimit) {
        std::sort(colors.begin(), colors.end(), [](const WeightedColor &left, const WeightedColor &right) {
            if (left.count != right.count) return left.count > right.count;
            return left.rgb < right.rgb;
        });
        for (const auto &color : colors) visiblePalette.append(color.rgb);
    } else {
        QVector<ColorBox> boxes;
        boxes.append({colors});
        while (boxes.size() < visibleColorLimit) {
            int splitIndex = -1;
            int splitRange = -1;
            for (int i = 0; i < boxes.size(); i++) {
                if (boxes.at(i).colors.size() < 2) continue;
                const int range = channelRange(boxes.at(i), widestChannel(boxes.at(i)));
                if (range > splitRange) {
                    splitIndex = i;
                    splitRange = range;
                }
            }
            if (splitIndex < 0) break;

            ColorBox box = boxes.takeAt(splitIndex);
            const int channel = widestChannel(box);
            std::sort(box.colors.begin(), box.colors.end(), [channel](const WeightedColor &left, const WeightedColor &right) {
                const int leftValue = channel == 0 ? qRed(left.rgb)
                                    : channel == 1 ? qGreen(left.rgb)
                                                   : qBlue(left.rgb);
                const int rightValue = channel == 0 ? qRed(right.rgb)
                                     : channel == 1 ? qGreen(right.rgb)
                                                    : qBlue(right.rgb);
                return leftValue != rightValue ? leftValue < rightValue : left.rgb < right.rgb;
            });

            qint64 total = 0;
            for (const auto &color : box.colors) total += color.count;
            qint64 cumulative = 0;
            int cut = 1;
            for (; cut < box.colors.size(); cut++) {
                cumulative += box.colors.at(cut - 1).count;
                if (cumulative * 2 >= total) break;
            }
            cut = qBound(1, cut, box.colors.size() - 1);
            boxes.append({box.colors.mid(0, cut)});
            boxes.append({box.colors.mid(cut)});
        }
        for (const auto &box : boxes) visiblePalette.append(averageColor(box));
    }

    QList<QRgb> palette;
    palette.append(qRgba(0, 0, 0, 0));
    palette.append(visiblePalette);
    while (palette.size() < Tileset::numColorsPerPalette()) {
        palette.append(qRgb(0, 0, 0));
    }
    return palette.mid(0, Tileset::numColorsPerPalette());
}

int nearestPaletteIndex(QRgb source, const QList<QRgb> &palette)
{
    int bestIndex = 1;
    int bestDistance = std::numeric_limits<int>::max();
    for (int index = 1; index < palette.size(); index++) {
        const int red = qRed(source) - qRed(palette.at(index));
        const int green = qGreen(source) - qGreen(palette.at(index));
        const int blue = qBlue(source) - qBlue(palette.at(index));
        const int distance = red * red * 3 + green * green * 4 + blue * blue * 2;
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = index;
        }
    }
    return bestIndex;
}

QImage indexImage(const QImage &source, const QList<QRgb> &palette)
{
    const QImage normalized = source.convertToFormat(QImage::Format_ARGB32);
    QImage indexed(source.size(), QImage::Format_Indexed8);
    indexed.setColorTable(palette.toVector());
    for (int y = 0; y < normalized.height(); y++) {
        const QRgb *sourceLine = reinterpret_cast<const QRgb *>(normalized.constScanLine(y));
        uchar *destinationLine = indexed.scanLine(y);
        for (int x = 0; x < normalized.width(); x++) {
            destinationLine[x] = qAlpha(sourceLine[x]) < 128
                ? 0
                : nearestPaletteIndex(sourceLine[x], palette);
        }
    }
    return indexed;
}

QByteArray imageKey(const QImage &image)
{
    QByteArray key;
    key.reserve(image.width() * image.height());
    for (int y = 0; y < image.height(); y++) {
        key.append(reinterpret_cast<const char *>(image.constScanLine(y)), image.width());
    }
    return key;
}

QByteArray metatileKey(const QVector<uint16_t> &tileIds)
{
    QByteArray key;
    key.reserve(tileIds.size() * 2);
    for (uint16_t tileId : tileIds) {
        key.append(static_cast<char>(tileId & 0xff));
        key.append(static_cast<char>(tileId >> 8));
    }
    return key;
}

} // namespace

ImageTilesetBuilder::Result ImageTilesetBuilder::build(
    const QImage &sourceImage,
    const Options &options) const
{
    Result result;
    if (sourceImage.isNull()) {
        result.errorMessage = QStringLiteral("The source image is empty.");
        return result;
    }
    if ((sourceImage.width() % Metatile::pixelWidth()) != 0
        || (sourceImage.height() % Metatile::pixelHeight()) != 0) {
        result.errorMessage = QString(
            "Image size %1 × %2 px must align to the %3 × %4 px metatile grid."
        ).arg(sourceImage.width())
         .arg(sourceImage.height())
         .arg(Metatile::pixelWidth())
         .arg(Metatile::pixelHeight());
        return result;
    }
    if (options.maxTiles <= 0 || options.maxMetatiles <= 0
        || options.tileIdBase < 0 || options.metatileIdBase < 0
        || options.paletteId < 0 || options.paletteId >= Tileset::maxPalettes()
        || options.tilesPerMetatile < Metatile::tilesPerLayer()) {
        result.errorMessage = QStringLiteral("The project supplied invalid tileset capacity settings.");
        return result;
    }

    result.mapSize = QSize(
        sourceImage.width() / Metatile::pixelWidth(),
        sourceImage.height() / Metatile::pixelHeight()
    );
    result.palette = buildPalette(sourceImage, &result.sourceColorCount, &result.quantized);
    result.indexedSource = indexImage(sourceImage, result.palette);

    QHash<QByteArray, int> tileIndices;
    QVector<QImage> uniqueTiles;
    QImage transparentTile(Tile::pixelSize(), QImage::Format_Indexed8);
    transparentTile.setColorTable(result.palette.toVector());
    transparentTile.fill(0);
    tileIndices.insert(imageKey(transparentTile), 0);
    uniqueTiles.append(transparentTile);
    QHash<QByteArray, int> metatileIndices;
    for (int cellY = 0; cellY < result.mapSize.height(); cellY++) {
        for (int cellX = 0; cellX < result.mapSize.width(); cellX++) {
            QVector<uint16_t> tileIds;
            for (int tileY = 0; tileY < Metatile::tileHeight(); tileY++) {
                for (int tileX = 0; tileX < Metatile::tileWidth(); tileX++) {
                    const QImage tileImage = result.indexedSource.copy(
                        (cellX * Metatile::tileWidth() + tileX) * Tile::pixelWidth(),
                        (cellY * Metatile::tileHeight() + tileY) * Tile::pixelHeight(),
                        Tile::pixelWidth(),
                        Tile::pixelHeight()
                    );
                    const QByteArray key = imageKey(tileImage);
                    auto tileIt = tileIndices.constFind(key);
                    int tileIndex = -1;
                    if (tileIt == tileIndices.cend()) {
                        tileIndex = uniqueTiles.size();
                        if (tileIndex >= options.maxTiles) {
                            result.errorMessage = QString(
                                "The image needs more than the %1 tiles available in this tileset role."
                            ).arg(options.maxTiles);
                            return result;
                        }
                        tileIndices.insert(key, tileIndex);
                        uniqueTiles.append(tileImage);
                    } else {
                        tileIndex = tileIt.value();
                    }
                    tileIds.append(static_cast<uint16_t>(options.tileIdBase + tileIndex));
                }
            }

            const QByteArray key = metatileKey(tileIds);
            auto metatileIt = metatileIndices.constFind(key);
            int metatileIndex = -1;
            if (metatileIt == metatileIndices.cend()) {
                metatileIndex = result.metatiles.size();
                if (metatileIndex >= options.maxMetatiles) {
                    result.errorMessage = QString(
                        "The image needs more than the %1 metatiles available in this tileset role."
                    ).arg(options.maxMetatiles);
                    return result;
                }
                Metatile metatile;
                for (uint16_t tileId : tileIds) {
                    metatile.tiles.append(Tile(tileId, false, false, options.paletteId));
                }
                while (metatile.tiles.size() < options.tilesPerMetatile) {
                    metatile.tiles.append(Tile(options.tileIdBase, false, false, options.paletteId));
                }
                metatileIndices.insert(key, metatileIndex);
                result.metatiles.append(metatile);
            } else {
                metatileIndex = metatileIt.value();
            }
            result.mapMetatileIds.append(
                static_cast<uint16_t>(options.metatileIdBase + metatileIndex)
            );
        }
    }

    result.uniqueTileCount = uniqueTiles.size();
    result.uniqueMetatileCount = result.metatiles.size();
    const int tilesWide = qMin(16, qMax(1, uniqueTiles.size()));
    const int tilesHigh = (uniqueTiles.size() + tilesWide - 1) / tilesWide;
    result.tilesImage = QImage(
        tilesWide * Tile::pixelWidth(),
        tilesHigh * Tile::pixelHeight(),
        QImage::Format_Indexed8
    );
    result.tilesImage.setColorTable(result.palette.toVector());
    result.tilesImage.fill(0);
    for (int index = 0; index < uniqueTiles.size(); index++) {
        const int destinationX = (index % tilesWide) * Tile::pixelWidth();
        const int destinationY = (index / tilesWide) * Tile::pixelHeight();
        for (int y = 0; y < Tile::pixelHeight(); y++) {
            memcpy(
                result.tilesImage.scanLine(destinationY + y) + destinationX,
                uniqueTiles.at(index).constScanLine(y),
                Tile::pixelWidth()
            );
        }
    }
    return result;
}

} // namespace Studio