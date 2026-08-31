#pragma once

#include <QImage>
#include <QList>
#include <QPoint>
#include <QSize>
#include <QString>

#include <cstdint>
#include <functional>

#include "studio/metatilerenderservice.h"

namespace Studio {

class ImageMetatileMatcher
{
public:
    struct MatchOptions {
        bool allowFuzzy = false;
        double maximumDistance = 0.15;
        double minimumConfidence = 0.50;
        int maximumRankedCandidates = 5;
        std::function<bool(int completedCells, int totalCells)> progressCallback;
    };

    enum class MatchStatus {
        NotEvaluated,
        Exact,
        FuzzyAccepted,
        FuzzyUncertain,
        FuzzyRejected,
    };

    struct CandidateResult {
        uint16_t metatileId = 0;
        MetatileRenderService::SourceTileset sourceTileset = MetatileRenderService::SourceTileset::Primary;
        QString sourceTilesetName;
        double distance = 1.0;
    };

    struct CellResult {
        QPoint position;
        QImage sourceImage;
        bool matched = false;
        uint16_t metatileId = 0;
        MetatileRenderService::SourceTileset sourceTileset = MetatileRenderService::SourceTileset::Primary;
        QString sourceTilesetName;
        MatchStatus status = MatchStatus::NotEvaluated;
        QList<CandidateResult> rankedCandidates;
        double bestDistance = 1.0;
        double confidence = 0.0;
    };

    struct Result {
        QSize mapSize;
        QList<CellResult> cells;
        QImage reconstructedImage;
        QImage differenceImage;
        int exactMatchCount = 0;
        int unmatchedCount = 0;
        int primaryMatchCount = 0;
        int secondaryMatchCount = 0;
        int fuzzyAcceptedCount = 0;
        int fuzzyUncertainCount = 0;
        int fuzzyRejectedCount = 0;
        bool usedFuzzyMatching = false;
        QString errorMessage;

        bool isValid() const { return errorMessage.isEmpty(); }
    };

    Result match(
        const QImage &sourceImage,
        const QSize &mapSize,
        const QSize &metatilePixelSize,
        const QList<MetatileRenderService::RenderedMetatile> &candidates,
        const MatchOptions &options) const;
};

} // namespace Studio