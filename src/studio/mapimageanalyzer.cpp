#include "studio/mapimageanalyzer.h"

#include "metatile.h"

#include <QFileInfo>
#include <QImageReader>

namespace Studio {

MapImageAnalyzer::Result MapImageAnalyzer::analyzePng(const QString &filepath) const
{
    Result result;
    result.sourcePath = filepath;
    result.metatilePixelSize = Metatile::pixelSize();

    const QFileInfo fileInfo(filepath);
    if (filepath.isEmpty()) {
        result.errorMessage = QStringLiteral("Choose a PNG image to analyze.");
        return result;
    }
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        result.errorMessage = QString("Image file '%1' does not exist.").arg(filepath);
        return result;
    }

    QImageReader reader(filepath);
    reader.setAutoTransform(false);
    if (!reader.canRead()) {
        result.errorMessage = QString("Could not read '%1': %2").arg(filepath, reader.errorString());
        return result;
    }
    if (reader.format().toLower() != QByteArrayLiteral("png")) {
        result.errorMessage = QString("'%1' is not a PNG image.").arg(filepath);
        return result;
    }

    QImage image = reader.read();
    if (image.isNull()) {
        result.errorMessage = QString("Could not decode '%1': %2").arg(filepath, reader.errorString());
        return result;
    }

    result.sourceImage = image;
    result.imageSize = image.size();
    result.loaded = true;

    const int cellWidth = result.metatilePixelSize.width();
    const int cellHeight = result.metatilePixelSize.height();
    if (cellWidth <= 0 || cellHeight <= 0) {
        result.errorMessage = QStringLiteral("Porymap returned an invalid metatile pixel size.");
        return result;
    }

    if ((result.imageSize.width() % cellWidth) != 0 || (result.imageSize.height() % cellHeight) != 0) {
        result.alignmentMessage = QString(
            "Image size %1 × %2 px is not aligned to Porymap's %3 × %4 px metatile grid."
        ).arg(result.imageSize.width())
         .arg(result.imageSize.height())
         .arg(cellWidth)
         .arg(cellHeight);
        return result;
    }

    result.mapSize = QSize(result.imageSize.width() / cellWidth, result.imageSize.height() / cellHeight);
    result.gridAligned = true;
    result.alignmentMessage = QString(
        "Valid: %1 × %2 map blocks at %3 × %4 px per metatile."
    ).arg(result.mapSize.width())
     .arg(result.mapSize.height())
     .arg(cellWidth)
     .arg(cellHeight);
    return result;
}

} // namespace Studio