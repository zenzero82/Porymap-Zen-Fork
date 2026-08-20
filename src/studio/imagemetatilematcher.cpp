#include "studio/imagemetatilematcher.h"

#include <QByteArray>
#include <QHash>
#include <QPainter>

#include <utility>

namespace Studio {

namespace {

QByteArray pixelKey(const QImage &image)
{
    return QByteArray(
        reinterpret_cast<const char *>(image.constBits()),
        static_cast<qsizetype>(image.sizeInBytes())
    );
}

} // namespace

ImageMetatileMatcher::Result ImageMetatileMatcher::match(
    const QImage &sourceImage,
    const QSize &mapSize,
    const QSize &metatilePixelSize,
    const QList<MetatileRenderService::RenderedMetatile> &candidates) const
{
    Result result;
    result.mapSize = mapSize;

    if (sourceImage.isNull()) {
        result.errorMessage = QStringLiteral("No source image is available for matching.");
        return result;
    }
    if (mapSize.isEmpty() || metatilePixelSize.isEmpty()) {
        result.errorMessage = QStringLiteral("The source image does not have valid map or metatile dimensions.");
        return result;
    }
    if (sourceImage.size() != QSize(
            mapSize.width() * metatilePixelSize.width(),
            mapSize.height() * metatilePixelSize.height())) {
        result.errorMessage = QStringLiteral("The source image dimensions do not match the requested map grid.");
        return result;
    }
    if (candidates.isEmpty()) {
        result.errorMessage = QStringLiteral("No rendered metatiles are available for exact matching.");
        return result;
    }

    const QImage normalizedSource = sourceImage.convertToFormat(QImage::Format_RGBA8888);
    QHash<QByteArray, int> candidateByPixels;
    QList<QImage> normalizedCandidates;
    normalizedCandidates.reserve(candidates.count());

    for (int index = 0; index < candidates.count(); index++) {
        const QImage candidate = candidates.at(index).image.convertToFormat(QImage::Format_RGBA8888);
        if (candidate.size() != metatilePixelSize) {
            result.errorMessage = QStringLiteral("A rendered metatile does not match the expected pixel size.");
            return result;
        }
        normalizedCandidates.append(candidate);
        const QByteArray key = pixelKey(candidate);
        if (!candidateByPixels.contains(key)) {
            candidateByPixels.insert(key, index);
        }
    }

    result.reconstructedImage = QImage(normalizedSource.size(), QImage::Format_RGBA8888);
    result.reconstructedImage.fill(Qt::transparent);
    result.differenceImage = QImage(normalizedSource.size(), QImage::Format_RGBA8888);
    result.differenceImage.fill(Qt::transparent);

    QPainter reconstructedPainter(&result.reconstructedImage);
    reconstructedPainter.setCompositionMode(QPainter::CompositionMode_Source);
    QPainter differencePainter(&result.differenceImage);
    for (int y = 0; y < mapSize.height(); y++) {
        for (int x = 0; x < mapSize.width(); x++) {
            const QPoint cellPosition(x, y);
            const QRect sourceRect(
                x * metatilePixelSize.width(),
                y * metatilePixelSize.height(),
                metatilePixelSize.width(),
                metatilePixelSize.height()
            );
            const QImage sourceCell = normalizedSource.copy(sourceRect);
            CellResult cell;
            cell.position = cellPosition;
            cell.sourceImage = sourceCell;

            const auto candidateIt = candidateByPixels.constFind(pixelKey(sourceCell));
            if (candidateIt != candidateByPixels.cend()) {
                const int candidateIndex = candidateIt.value();
                const auto &candidate = candidates.at(candidateIndex);
                cell.matched = true;
                cell.metatileId = candidate.metatileId;
                cell.sourceTileset = candidate.source;
                cell.sourceTilesetName = candidate.sourceTilesetName;
                reconstructedPainter.drawImage(sourceRect.topLeft(), normalizedCandidates.at(candidateIndex));
                result.exactMatchCount++;
                if (candidate.source == MetatileRenderService::SourceTileset::Primary) {
                    result.primaryMatchCount++;
                } else {
                    result.secondaryMatchCount++;
                }
            } else {
                differencePainter.drawImage(sourceRect.topLeft(), sourceCell);
                differencePainter.fillRect(sourceRect, QColor(220, 24, 48, 150));
                result.unmatchedCount++;
            }

            result.cells.append(std::move(cell));
        }
    }

    reconstructedPainter.end();
    differencePainter.end();
    if (result.unmatchedCount == 0 && result.reconstructedImage != normalizedSource) {
        result.errorMessage = QStringLiteral(
            "Exact candidates did not reproduce the source pixels without alteration."
        );
    }
    return result;
}

} // namespace Studio