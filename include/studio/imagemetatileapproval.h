#pragma once

#include <QHash>
#include <QList>
#include <QSize>

#include "studio/imagemetatilematcher.h"

namespace Studio {

struct ImageMetatileCorrection
{
    ImageMetatileMatcher::CandidateResult candidate;
    QImage sourceImage;
    QImage renderedImage;
    bool approved = false;
};

namespace ImageMetatileApproval {

bool isCorrectionValid(
    const ImageMetatileMatcher::CellResult &cell,
    const ImageMetatileCorrection &correction,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles);

bool allCellsResolved(
    const QList<ImageMetatileMatcher::CellResult> &cells,
    const QHash<int, ImageMetatileCorrection> &corrections,
    const QSize &mapSize,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles);

bool resolveMetatileId(
    const ImageMetatileMatcher::CellResult &cell,
    int cellIndex,
    const QHash<int, ImageMetatileCorrection> &corrections,
    const QList<MetatileRenderService::RenderedMetatile> &renderedMetatiles,
    uint16_t *metatileId);

} // namespace ImageMetatileApproval
} // namespace Studio