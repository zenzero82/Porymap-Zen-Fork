#include "studio/artworksourcegenerator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <algorithm>

namespace Studio {

namespace {

constexpr int tilePixelSize = 8;
constexpr int metatilePixelSize = 16;

QByteArray tileKey(const QImage &image, int x, int y)
{
    QImage tile = image.copy(x, y, tilePixelSize, tilePixelSize).convertToFormat(QImage::Format_RGBA8888);
    return QByteArray(reinterpret_cast<const char *>(tile.constBits()), static_cast<int>(tile.sizeInBytes()));
}

QRgb normalizedColor(QRgb color)
{
    return qAlpha(color) == 0 ? QRgb(0) : color;
}

bool allocatePalettes(const QList<QSet<QRgb>> &sets, int index, QList<QSet<QRgb>> *palettes, int maximum)
{
    if (index == sets.size()) return true;
    const QSet<QRgb> &colors = sets.at(index);
    QSet<QByteArray> tried;
    for (int i = 0; i < palettes->size(); ++i) {
        QList<QRgb> signatureValues = palettes->at(i).values();
        std::sort(signatureValues.begin(), signatureValues.end());
        QByteArray signature(reinterpret_cast<const char *>(signatureValues.constData()),
                             static_cast<int>(signatureValues.size() * sizeof(QRgb)));
        if (tried.contains(signature)) continue;
        tried.insert(signature);
        QSet<QRgb> combined = palettes->at(i);
        combined.unite(colors);
        if (combined.size() > 16) continue;
        const QSet<QRgb> previous = palettes->at(i);
        (*palettes)[i] = combined;
        if (allocatePalettes(sets, index + 1, palettes, maximum)) return true;
        (*palettes)[i] = previous;
    }
    if (palettes->size() < maximum) {
        palettes->append(colors);
        if (allocatePalettes(sets, index + 1, palettes, maximum)) return true;
        palettes->removeLast();
    }
    return false;
}

bool validPrimarySource(const QString &directory)
{
    const QDir dir(directory);
    if (!dir.exists()) return false;
    for (const QString &name : {QStringLiteral("bottom.png"), QStringLiteral("middle.png"),
                                QStringLiteral("top.png"), QStringLiteral("attributes.csv")}) {
        const QFileInfo info(dir.filePath(name));
        if (!info.isFile() || info.size() <= 0) return false;
    }
    return true;
}

}

int ArtworkSourceGenerator::countColors(const QImage &image)
{
    QSet<QRgb> colors;
    const QImage normalized = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < normalized.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(normalized.constScanLine(y));
        for (int x = 0; x < normalized.width(); ++x) {
            colors.insert(normalizedColor(line[x]));
        }
    }
    return colors.size();
}

bool ArtworkSourceGenerator::saveImage(const QImage &image, const QString &path, QString *error)
{
    if (!image.save(path, "PNG")) {
        if (error) *error = QStringLiteral("Could not write generated artwork '%1'.").arg(path);
        return false;
    }
    return true;
}

ArtworkSourceResult ArtworkSourceGenerator::generate(const ArtworkSourceRequest &request) const
{
    ArtworkSourceResult result;
    if (request.maxUniqueTiles <= 0 || request.maxColors <= 0) {
        result.error = QStringLiteral("Artwork generation limits must be positive.");
        return result;
    }
    if (request.secondary && !validPrimarySource(request.primarySourceDirectory)) {
        result.error = QStringLiteral(
            "Secondary artwork requires a primary Porytiles source containing bottom.png, middle.png, top.png, and attributes.csv.");
        return result;
    }
    QImageReader reader(request.artworkPath);
    reader.setAutoTransform(false);
    if (!reader.canRead() || reader.format().toLower() != QByteArrayLiteral("png")) {
        result.error = QStringLiteral("Choose a readable PNG artwork sheet.");
        return result;
    }
    const QImage artwork = reader.read();
    if (artwork.isNull() || artwork.width() <= 0 || artwork.height() <= 0) {
        result.error = QStringLiteral("The artwork sheet could not be decoded.");
        return result;
    }
    if (artwork.width() % metatilePixelSize != 0
        || artwork.height() % (metatilePixelSize * 3) != 0) {
        result.error = QStringLiteral(
            "Artwork must be a vertical bottom/middle/top sheet aligned to %1×%2 px metatiles.")
                       .arg(metatilePixelSize).arg(metatilePixelSize);
        return result;
    }
    result.colorCount = countColors(artwork);
    if (result.colorCount > request.maxColors) {
        result.error = QStringLiteral("Artwork uses %1 colors; the configured limit is %2.")
                           .arg(result.colorCount).arg(request.maxColors);
        return result;
    }

    const int metatileColumns = artwork.width() / metatilePixelSize;
    const int metatileRows = artwork.height() / (metatilePixelSize * 3);
    result.metatileCount = metatileColumns * metatileRows;
    if (result.metatileCount > request.maxMetatiles) {
        result.error = QStringLiteral("Artwork contains %1 metatiles; the selected tileset limit is %2.")
                           .arg(result.metatileCount).arg(request.maxMetatiles);
        return result;
    }
    QSet<QByteArray> tiles;
    QList<QSet<QRgb>> tileColorSets;
    QSet<QByteArray> colorSetKeys;
    for (int layer = 0; layer < 3; ++layer) {
        const QImage layerImage = artwork.copy(
            0, layer * artwork.height() / 3, artwork.width(), artwork.height() / 3);
        for (int y = 0; y < layerImage.height(); y += tilePixelSize) {
            for (int x = 0; x < layerImage.width(); x += tilePixelSize) {
                tiles.insert(tileKey(layerImage, x, y));
                QSet<QRgb> tileColors;
                const QImage tile = layerImage.copy(x, y, tilePixelSize, tilePixelSize).convertToFormat(QImage::Format_ARGB32);
                for (int ty = 0; ty < tile.height(); ++ty) {
                    const QRgb *line = reinterpret_cast<const QRgb *>(tile.constScanLine(ty));
                    for (int tx = 0; tx < tile.width(); ++tx)
                        tileColors.insert(normalizedColor(line[tx]));
                }
                if (tileColors.size() > 16) {
                    result.error = QStringLiteral("An 8×8 tile at layer %1, x %2, y %3 uses %4 colors; 4bpp tiles allow 16.")
                                       .arg(layer).arg(x).arg(y).arg(tileColors.size());
                    return result;
                }
                QList<QRgb> values = tileColors.values();
                std::sort(values.begin(), values.end());
                QByteArray key(reinterpret_cast<const char *>(values.constData()),
                               static_cast<int>(values.size() * sizeof(QRgb)));
                if (!colorSetKeys.contains(key)) {
                    colorSetKeys.insert(key);
                    tileColorSets.append(tileColors);
                }
            }
        }
    }
    std::sort(tileColorSets.begin(), tileColorSets.end(),
              [](const QSet<QRgb> &a, const QSet<QRgb> &b) { return a.size() > b.size(); });
    QList<QSet<QRgb>> palettes;
    if (!allocatePalettes(tileColorSets, 0, &palettes, request.maxPalettes)) {
        result.error = QStringLiteral("Artwork colors cannot be allocated within the selected tileset's %1 palettes.")
                           .arg(request.maxPalettes);
        return result;
    }
    result.paletteCount = palettes.size();
    result.uniqueTileCount = tiles.size();
    if (result.uniqueTileCount > request.maxUniqueTiles) {
        result.error = QStringLiteral("Artwork requires %1 unique 8×8 tiles; the configured limit is %2.")
                           .arg(result.uniqueTileCount).arg(request.maxUniqueTiles);
        return result;
    }

    QDir output(request.outputDirectory);
    const QString absoluteOutput = QFileInfo(request.outputDirectory).absoluteFilePath();
    const QString forbidden = QFileInfo(request.forbiddenRoot).absoluteFilePath();
    if (!request.forbiddenRoot.isEmpty()
        && (absoluteOutput == forbidden || absoluteOutput.startsWith(forbidden + QDir::separator()))) {
        result.error = QStringLiteral("Generated source must be outside the active project directory.");
        return result;
    }
    if (output.exists()) {
        result.error = QStringLiteral("Choose a new generated source directory.");
        return result;
    }
    if (!output.exists() && !output.mkpath(QStringLiteral("."))) {
        result.error = QStringLiteral("Could not create generated source directory.");
        return result;
    }
    const QStringList layerNames = {QStringLiteral("bottom.png"), QStringLiteral("middle.png"), QStringLiteral("top.png")};
    for (int layer = 0; layer < 3; ++layer) {
        const QImage layerImage = artwork.copy(
            0, layer * artwork.height() / 3, artwork.width(), artwork.height() / 3);
        const QString path = output.filePath(layerNames.at(layer));
        if (!saveImage(layerImage, path, &result.error)) {
            output.removeRecursively();
            return result;
        }
        result.generatedFiles.append(path);
    }

    QFile attributes(output.filePath(QStringLiteral("attributes.csv")));
    if (!attributes.open(QIODevice::WriteOnly | QIODevice::Text)) {
        result.error = QStringLiteral("Could not write generated attributes.csv.");
        output.removeRecursively();
        return result;
    }
    attributes.write("id,behavior\n");
    for (int i = 0; i < result.metatileCount; ++i) {
        attributes.write(QStringLiteral("%1,MB_NORMAL\n").arg(i).toUtf8());
    }
    attributes.close();
    result.generatedFiles.append(attributes.fileName());
    result.outputDirectory = absoluteOutput;
    result.success = true;
    return result;
}

} // namespace Studio