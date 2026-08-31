#include "studio/imagemetatileapproval.h"

#include <QSet>

namespace Studio::ImageMetatileApproval {

namespace {

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

} // namespace Studio::ImageMetatileApproval