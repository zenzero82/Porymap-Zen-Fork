#include "studio/imagemetatileapproval.h"

#include <QSet>
#include <limits>

namespace Studio::ImageMetatileApproval {

namespace {

bool fail(QString *errorMessage, const QString &message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
    return false;
}

const MetatileRenderService::RenderedMetatile *findRenderedCandidate(
    const ImageMetatileMatcher::CandidateResult &candidate,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles)
{
    for (const auto &rendered : renderedMetatiles) {
        if (rendered.metatileId == candidate.metatileId
            && rendered.source == candidate.sourceTileset
            && rendered.sourceTilesetName == candidate.sourceTilesetName) {
            return &rendered;
        }
    }
    return nullptr;
}

} // namespace

bool isCorrectionValid(
    const ImageMetatileMatcher::CellResult &cell,
    const ImageMetatileCorrection &correction,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles)
{
    if (!correction.approved || correction.sourceImage != cell.sourceImage) {
        return false;
    }
    const auto *rendered = findRenderedCandidate(correction.candidate, renderedMetatiles);
    return rendered && rendered->image == correction.renderedImage;
}

bool allCellsResolved(
    const QList<ImageMetatileMatcher::CellResult> &cells,
    const QHash<int, ImageMetatileCorrection> &corrections,
    const QSize &mapSize,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles)
{
    if (cells.isEmpty() || !mapSize.isValid()) {
        return false;
    }

    QSet<int> seenIndices;
    for (const auto &cell : cells) {
        if (cell.position.x() < 0 || cell.position.x() >= mapSize.width()
            || cell.position.y() < 0 || cell.position.y() >= mapSize.height()) {
            return false;
        }
        const int index = cell.position.y() * mapSize.width() + cell.position.x();
        if (seenIndices.contains(index)) {
            return false;
        }
        seenIndices.insert(index);
        if (cell.matched) {
            continue;
        }
        const auto correctionIt = corrections.constFind(index);
        if (correctionIt == corrections.cend()
            || !isCorrectionValid(cell, correctionIt.value(), renderedMetatiles)) {
            return false;
        }
    }
    return seenIndices.size() == mapSize.width() * mapSize.height();
}

bool resolveMetatileId(
    const ImageMetatileMatcher::CellResult &cell,
    int cellIndex,
    const QHash<int, ImageMetatileCorrection> &corrections,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles,
    uint16_t *metatileId)
{
    if (!metatileId) {
        return false;
    }
    if (cell.matched) {
        *metatileId = cell.metatileId;
        return true;
    }
    const auto correctionIt = corrections.constFind(cellIndex);
    if (correctionIt == corrections.cend()
        || !isCorrectionValid(cell, correctionIt.value(), renderedMetatiles)) {
        return false;
    }
    *metatileId = correctionIt.value().candidate.metatileId;
    return true;
}

bool buildBlockdata(
    const QList<ImageMetatileMatcher::CellResult> &cells,
    const QHash<int, ImageMetatileCorrection> &corrections,
    const QSize &mapSize,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles,
    Blockdata *blockdata,
    QString *errorMessage)
{
    if (!blockdata) {
        return fail(errorMessage, QStringLiteral("No blockdata output was provided."));
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    blockdata->clear();

    if (!mapSize.isValid()) {
        return fail(errorMessage, QStringLiteral("Map dimensions are invalid."));
    }
    const qint64 expectedBlockCount = static_cast<qint64>(mapSize.width()) * mapSize.height();
    if (expectedBlockCount <= 0 || expectedBlockCount > std::numeric_limits<int>::max()) {
        return fail(errorMessage, QStringLiteral("Map dimensions are outside the supported range."));
    }
    if (cells.isEmpty()) {
        return fail(errorMessage, QStringLiteral("The match result contains no cells."));
    }
    if (!allCellsResolved(cells, corrections, mapSize, renderedMetatiles)) {
        return fail(
            errorMessage,
            QStringLiteral("The match result is incomplete, duplicated, out of bounds, or has an invalid correction.")
        );
    }

    blockdata->resize(static_cast<int>(expectedBlockCount));
    for (const auto &cell : cells) {
        const int index = cell.position.y() * mapSize.width() + cell.position.x();
        uint16_t metatileId = 0;
        if (!resolveMetatileId(cell, index, corrections, renderedMetatiles, &metatileId)) {
            blockdata->clear();
            return fail(errorMessage, QStringLiteral("A cell could not be resolved to a metatile."));
        }
        // Imported images only provide metatile identities. Collision and
        // elevation remain the project's normal default values until edited.
        (*blockdata)[index] = Block(metatileId, 0, 0);
    }
    return true;
}

} // namespace Studio::ImageMetatileApproval