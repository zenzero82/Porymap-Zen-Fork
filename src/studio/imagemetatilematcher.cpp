#include "studio/imagemetatilematcher.h"

#include <QByteArray>
#include <QHash>
#include <QPainter>

#include <algorithm>
#include <cmath>
#include <numeric>
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

double pixelDistance(const QImage &left, const QImage &right)
{
    constexpr double maximumChannelDifference = 255.0;
    const int width = left.width();
    const int height = left.height();
    double totalDifference = 0.0;

    for (int y = 0; y < height; y++) {
        const uchar *leftLine = left.constScanLine(y);
        const uchar *rightLine = right.constScanLine(y);
        for (int x = 0; x < width; x++) {
            const int offset = x * 4;
            const int leftAlpha = leftLine[offset + 3];
            const int rightAlpha = rightLine[offset + 3];
            for (int channel = 0; channel < 3; channel++) {
                const int leftPremultiplied = (
                    static_cast<int>(leftLine[offset + channel]) * leftAlpha + 127
                ) / 255;
                const int rightPremultiplied = (
                    static_cast<int>(rightLine[offset + channel]) * rightAlpha + 127
                ) / 255;
                totalDifference += std::abs(leftPremultiplied - rightPremultiplied);
            }
            totalDifference += std::abs(leftAlpha - rightAlpha);
        }
    }

    const double channelCount = static_cast<double>(width) * height * 4;
    return channelCount == 0.0
        ? 1.0
        : totalDifference / (channelCount * maximumChannelDifference);
}

double fuzzyConfidence(double bestDistance, double secondDistance, bool hasSecondCandidate)
{
    const double closeness = std::clamp(1.0 - bestDistance, 0.0, 1.0);
    if (!hasSecondCandidate) {
        return closeness;
    }

    constexpr double minimumDistance = 1.0e-12;
    const double separation = std::clamp(
        (secondDistance - bestDistance) / std::max(secondDistance, minimumDistance),
        0.0,
        1.0
    );
    return closeness * separation;
}

struct FuzzyEvaluation {
    ImageMetatileMatcher::MatchStatus status = ImageMetatileMatcher::MatchStatus::FuzzyRejected;
    QList<ImageMetatileMatcher::CandidateResult> rankedCandidates;
    int bestCandidateIndex = -1;
    double bestDistance = 1.0;
    double confidence = 0.0;
};

} // namespace

ImageMetatileMatcher::Result ImageMetatileMatcher::match(
    const QImage &sourceImage,
    const QSize &mapSize,
    const QSize &metatilePixelSize,
    const QList<MetatileRenderService::RenderedMetatile> &candidates,
    const MatchOptions &options) const
{
    Result result;
    result.mapSize = mapSize;
    result.usedFuzzyMatching = options.allowFuzzy;

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
        result.errorMessage = QStringLiteral("No rendered metatiles are available for matching.");
        return result;
    }
    if (options.maximumDistance < 0.0 || options.maximumDistance > 1.0
        || options.minimumConfidence < 0.0 || options.minimumConfidence > 1.0
        || options.maximumRankedCandidates <= 0) {
        result.errorMessage = QStringLiteral("The fuzzy matching options are invalid.");
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
    QHash<QByteArray, FuzzyEvaluation> fuzzyEvaluationByPixels;
    const int totalCells = mapSize.width() * mapSize.height();
    int completedCells = 0;
    for (int y = 0; y < mapSize.height(); y++) {
        for (int x = 0; x < mapSize.width(); x++) {
            if (options.allowFuzzy && options.progressCallback
                && !options.progressCallback(completedCells, totalCells)) {
                result.errorMessage = QStringLiteral("Fuzzy matching was cancelled.");
                return result;
            }

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

            const QByteArray sourcePixels = pixelKey(sourceCell);
            const auto candidateIt = candidateByPixels.constFind(sourcePixels);
            if (candidateIt != candidateByPixels.cend()) {
                const int candidateIndex = candidateIt.value();
                const auto &candidate = candidates.at(candidateIndex);
                cell.matched = true;
                cell.metatileId = candidate.metatileId;
                cell.sourceTileset = candidate.source;
                cell.sourceTilesetName = candidate.sourceTilesetName;
                cell.status = MatchStatus::Exact;
                cell.bestDistance = 0.0;
                cell.confidence = 1.0;
                reconstructedPainter.drawImage(sourceRect.topLeft(), normalizedCandidates.at(candidateIndex));
                result.exactMatchCount++;
                if (candidate.source == MetatileRenderService::SourceTileset::Primary) {
                    result.primaryMatchCount++;
                } else {
                    result.secondaryMatchCount++;
                }
            } else if (options.allowFuzzy) {
                FuzzyEvaluation fuzzy;
                const auto fuzzyIt = fuzzyEvaluationByPixels.constFind(sourcePixels);
                if (fuzzyIt != fuzzyEvaluationByPixels.cend()) {
                    fuzzy = fuzzyIt.value();
                } else {
                    QList<int> rankedIndices(candidates.count());
                    QList<double> candidateDistances;
                    candidateDistances.reserve(candidates.count());
                    for (const auto &candidateImage : normalizedCandidates) {
                        candidateDistances.append(pixelDistance(sourceCell, candidateImage));
                    }
                    std::iota(rankedIndices.begin(), rankedIndices.end(), 0);
                    const int analysisCount = std::min(
                        qMax(2, options.maximumRankedCandidates),
                        static_cast<int>(rankedIndices.count())
                    );
                    std::partial_sort(
                        rankedIndices.begin(),
                        rankedIndices.begin() + analysisCount,
                        rankedIndices.end(),
                        [&](int left, int right) {
                            const double leftDistance = candidateDistances.at(left);
                            const double rightDistance = candidateDistances.at(right);
                            if (leftDistance == rightDistance) {
                                return left < right;
                            }
                            return leftDistance < rightDistance;
                        }
                    );

                    fuzzy.bestCandidateIndex = rankedIndices.first();
                    fuzzy.bestDistance = candidateDistances.at(fuzzy.bestCandidateIndex);
                    const bool hasSecondCandidate = rankedIndices.count() > 1;
                    const double secondDistance = hasSecondCandidate
                        ? candidateDistances.at(rankedIndices.at(1))
                        : 1.0;
                    fuzzy.confidence = fuzzyConfidence(
                        fuzzy.bestDistance,
                        secondDistance,
                        hasSecondCandidate
                    );

                    const int displayedCount = std::min(
                        options.maximumRankedCandidates,
                        static_cast<int>(rankedIndices.count())
                    );
                    fuzzy.rankedCandidates.reserve(displayedCount);
                    for (int rank = 0; rank < displayedCount; rank++) {
                        const int candidateIndex = rankedIndices.at(rank);
                        const auto &candidate = candidates.at(candidateIndex);
                        CandidateResult rankedCandidate;
                        rankedCandidate.metatileId = candidate.metatileId;
                        rankedCandidate.sourceTileset = candidate.source;
                        rankedCandidate.sourceTilesetName = candidate.sourceTilesetName;
                        rankedCandidate.distance = candidateDistances.at(candidateIndex);
                        fuzzy.rankedCandidates.append(rankedCandidate);
                    }

                    if (fuzzy.bestDistance <= options.maximumDistance
                        && fuzzy.confidence >= options.minimumConfidence) {
                        fuzzy.status = MatchStatus::FuzzyAccepted;
                    } else if (fuzzy.bestDistance <= options.maximumDistance) {
                        fuzzy.status = MatchStatus::FuzzyUncertain;
                    } else {
                        fuzzy.status = MatchStatus::FuzzyRejected;
                    }
                    fuzzyEvaluationByPixels.insert(sourcePixels, fuzzy);
                }

                cell.rankedCandidates = fuzzy.rankedCandidates;
                cell.bestDistance = fuzzy.bestDistance;
                cell.confidence = fuzzy.confidence;
                cell.status = fuzzy.status;
                const auto &bestCandidate = candidates.at(fuzzy.bestCandidateIndex);
                cell.metatileId = bestCandidate.metatileId;
                cell.sourceTileset = bestCandidate.source;
                cell.sourceTilesetName = bestCandidate.sourceTilesetName;
                if (cell.status == MatchStatus::FuzzyAccepted) {
                    result.fuzzyAcceptedCount++;
                    reconstructedPainter.drawImage(
                        sourceRect.topLeft(),
                        normalizedCandidates.at(fuzzy.bestCandidateIndex)
                    );
                    differencePainter.drawImage(sourceRect.topLeft(), sourceCell);
                    differencePainter.fillRect(sourceRect, QColor(24, 170, 90, 120));
                } else if (cell.status == MatchStatus::FuzzyUncertain) {
                    result.fuzzyUncertainCount++;
                    reconstructedPainter.drawImage(
                        sourceRect.topLeft(),
                        normalizedCandidates.at(fuzzy.bestCandidateIndex)
                    );
                    differencePainter.drawImage(sourceRect.topLeft(), sourceCell);
                    differencePainter.fillRect(sourceRect, QColor(230, 160, 30, 150));
                } else {
                    result.fuzzyRejectedCount++;
                    differencePainter.drawImage(sourceRect.topLeft(), sourceCell);
                    differencePainter.fillRect(sourceRect, QColor(220, 24, 48, 150));
                }
                result.unmatchedCount++;
            } else {
                cell.status = MatchStatus::NotEvaluated;
                differencePainter.drawImage(sourceRect.topLeft(), sourceCell);
                differencePainter.fillRect(sourceRect, QColor(220, 24, 48, 150));
                result.unmatchedCount++;
            }

            result.cells.append(std::move(cell));
            completedCells++;
        }
    }
    if (options.allowFuzzy && options.progressCallback
        && !options.progressCallback(totalCells, totalCells)) {
        result.errorMessage = QStringLiteral("Fuzzy matching was cancelled.");
        return result;
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