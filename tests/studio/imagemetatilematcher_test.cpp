#include "studio/imagemetatilematcher.h"

#include <QColor>
#include <QImage>
#include <QString>

#include <cmath>
#include <iostream>

namespace {

using Matcher = Studio::ImageMetatileMatcher;
using RenderedMetatile = Studio::MetatileRenderService::RenderedMetatile;
using SourceTileset = Studio::MetatileRenderService::SourceTileset;

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures++;
    }
}

QImage solidImage(const QColor &color)
{
    QImage image(2, 2, QImage::Format_RGBA8888);
    image.fill(color);
    return image;
}

RenderedMetatile candidate(uint16_t id, const QImage &image)
{
    RenderedMetatile rendered;
    rendered.metatileId = id;
    rendered.source = SourceTileset::Primary;
    rendered.sourceTilesetName = QStringLiteral("TestTileset");
    rendered.image = image;
    return rendered;
}

Matcher::Result match(
    const QImage &source,
    const QList<RenderedMetatile> &candidates,
    double maximumDistance,
    double minimumConfidence)
{
    Matcher matcher;
    Matcher::MatchOptions options;
    options.allowFuzzy = true;
    options.maximumDistance = maximumDistance;
    options.minimumConfidence = minimumConfidence;
    return matcher.match(source, QSize(1, 1), QSize(2, 2), candidates, options);
}

void testExactMatchRemainsPreferred()
{
    const QImage source = solidImage(QColor(10, 20, 30, 255));
    const auto result = match(
        source,
        {candidate(7, solidImage(QColor(40, 50, 60, 255))), candidate(8, source)},
        1.0,
        0.0
    );

    expect(result.isValid(), "exact result should be valid");
    expect(result.exactMatchCount == 1, "exact match should be counted");
    expect(result.fuzzyAcceptedCount == 0, "exact match should not become fuzzy");
    expect(result.cells.first().status == Matcher::MatchStatus::Exact, "exact status should be retained");
    expect(result.cells.first().metatileId == 8, "exact candidate should win");
    expect(result.cells.first().confidence == 1.0, "exact confidence should be 100%");
}

void testDistanceOrderingAndThresholds()
{
    const QImage source = solidImage(QColor(0, 0, 0, 255));
    const QList<RenderedMetatile> candidates = {
        candidate(10, solidImage(QColor(100, 100, 100, 255))),
        candidate(11, solidImage(QColor(10, 10, 10, 255))),
    };

    const auto accepted = match(source, candidates, 0.10, 0.80);
    expect(accepted.cells.first().metatileId == 11, "nearest candidate should rank first");
    expect(accepted.cells.first().rankedCandidates.count() == 2, "ranked alternatives should be returned");
    expect(
        accepted.cells.first().rankedCandidates.at(0).distance
            < accepted.cells.first().rankedCandidates.at(1).distance,
        "candidate distances should be ascending"
    );
    expect(
        accepted.cells.first().status == Matcher::MatchStatus::FuzzyAccepted,
        "close, distinctive candidate should be accepted"
    );

    const auto uncertain = match(source, candidates, 0.10, 0.95);
    expect(
        uncertain.cells.first().status == Matcher::MatchStatus::FuzzyUncertain,
        "candidate below confidence threshold should be uncertain"
    );

    const auto rejected = match(source, candidates, 0.01, 0.0);
    expect(
        rejected.cells.first().status == Matcher::MatchStatus::FuzzyRejected,
        "candidate above distance threshold should be rejected"
    );
}

void testCandidateOrderBreaksDistanceTies()
{
    const QImage source = solidImage(QColor(0, 0, 0, 255));
    const auto result = match(
        source,
        {
            candidate(21, solidImage(QColor(10, 0, 0, 255))),
            candidate(22, solidImage(QColor(0, 10, 0, 255))),
        },
        1.0,
        0.0
    );

    expect(result.cells.first().metatileId == 21, "candidate order should break equal-distance ties");
    expect(result.cells.first().confidence == 0.0, "equal best candidates should have zero confidence");

    Matcher matcher;
    Matcher::MatchOptions options;
    options.allowFuzzy = true;
    options.maximumDistance = 1.0;
    options.minimumConfidence = 0.1;
    options.maximumRankedCandidates = 1;
    const auto truncated = matcher.match(
        source,
        QSize(1, 1),
        QSize(2, 2),
        {
            candidate(21, solidImage(QColor(10, 0, 0, 255))),
            candidate(22, solidImage(QColor(0, 10, 0, 255))),
        },
        options
    );
    expect(
        truncated.cells.first().rankedCandidates.count() == 1,
        "displayed ranking should honor its configured limit"
    );
    expect(
        truncated.cells.first().confidence == 0.0,
        "confidence should still account for a hidden runner-up"
    );
    expect(
        truncated.cells.first().status == Matcher::MatchStatus::FuzzyUncertain,
        "a truncated display must not turn an ambiguous match into an accepted one"
    );
}

void testTransparentRgbDoesNotDistortFuzzyDistance()
{
    QImage source(2, 2, QImage::Format_RGBA8888);
    QImage transparentCandidate(2, 2, QImage::Format_RGBA8888);
    for (int y = 0; y < 2; y++) {
        uchar *sourceLine = source.scanLine(y);
        uchar *candidateLine = transparentCandidate.scanLine(y);
        for (int x = 0; x < 2; x++) {
            const int offset = x * 4;
            sourceLine[offset] = 255;
            sourceLine[offset + 1] = 0;
            sourceLine[offset + 2] = 0;
            sourceLine[offset + 3] = 0;
            candidateLine[offset] = 0;
            candidateLine[offset + 1] = 0;
            candidateLine[offset + 2] = 255;
            candidateLine[offset + 3] = 0;
        }
    }

    const auto result = match(
        source,
        {
            candidate(30, transparentCandidate),
            candidate(31, solidImage(QColor(0, 0, 0, 255))),
        },
        0.01,
        0.5
    );
    expect(result.exactMatchCount == 0, "different hidden RGB bytes should not be an exact match");
    expect(result.cells.first().bestDistance == 0.0, "fully transparent RGB should have zero fuzzy distance");
    expect(
        result.cells.first().status == Matcher::MatchStatus::FuzzyAccepted,
        "visually identical transparent candidate should be accepted"
    );
}

void testInvalidOptionsAreRejected()
{
    Matcher matcher;
    Matcher::MatchOptions options;
    options.allowFuzzy = true;
    options.maximumDistance = 1.1;
    const auto result = matcher.match(
        solidImage(Qt::black),
        QSize(1, 1),
        QSize(2, 2),
        {candidate(1, solidImage(Qt::white))},
        options
    );
    expect(!result.isValid(), "out-of-range fuzzy options should be rejected");
}

void testFuzzyMatchingCanBeCancelled()
{
    Matcher matcher;
    Matcher::MatchOptions options;
    options.allowFuzzy = true;
    options.progressCallback = [](int, int) {
        return false;
    };
    const auto result = matcher.match(
        solidImage(Qt::black),
        QSize(1, 1),
        QSize(2, 2),
        {candidate(1, solidImage(Qt::white))},
        options
    );
    expect(!result.isValid(), "cancelled fuzzy matching should not return a usable result");
    expect(
        result.errorMessage == QStringLiteral("Fuzzy matching was cancelled."),
        "cancelled fuzzy matching should report cancellation"
    );

    options.progressCallback = [](int completedCells, int totalCells) {
        return completedCells != totalCells;
    };
    const auto terminalCancellation = matcher.match(
        solidImage(Qt::black),
        QSize(1, 1),
        QSize(2, 2),
        {candidate(1, solidImage(Qt::white))},
        options
    );
    expect(
        !terminalCancellation.isValid(),
        "cancellation during the terminal progress update should invalidate the result"
    );
}

} // namespace

int main()
{
    testExactMatchRemainsPreferred();
    testDistanceOrderingAndThresholds();
    testCandidateOrderBreaksDistanceTies();
    testTransparentRgbDoesNotDistortFuzzyDistance();
    testInvalidOptionsAreRejected();
    testFuzzyMatchingCanBeCancelled();

    if (failures == 0) {
        std::cout << "All image metatile matcher tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}