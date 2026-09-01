#include "studio/smartcollisionsuggester.h"

#include <QtGlobal>

namespace Studio {

CollisionSuggestionResult SmartCollisionSuggester::suggest(
    const QImage &terrain,
    const QSize &mapSize,
    uint16_t maxCollision,
    uint16_t maxElevation,
    uint16_t defaultCollision,
    uint16_t defaultElevation,
    int blockedCollision) const
{
    CollisionSuggestionResult result;
    result.mapSize = mapSize;
    if (terrain.isNull() || mapSize.isEmpty()) {
        result.error = QStringLiteral("A non-empty terrain image and map size are required.");
        return result;
    }
    if (terrain.width() != mapSize.width() * 16 || terrain.height() != mapSize.height() * 16) {
        result.error = QStringLiteral("Terrain dimensions must equal the map size multiplied by 16 pixels.");
        return result;
    }
    if (defaultCollision > maxCollision || defaultElevation > maxElevation
        || blockedCollision > maxCollision) {
        result.error = QStringLiteral("Configured collision/elevation values exceed the project field limits.");
        return result;
    }

    const QImage image = terrain.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < mapSize.height(); ++y) {
        for (int x = 0; x < mapSize.width(); ++x) {
            int opaquePixels = 0;
            int edgeOpaquePixels = 0;
            int edgeBrightness = 0;
            for (int py = 0; py < 16; ++py) {
                const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y * 16 + py));
                for (int px = 0; px < 16; ++px) {
                    const QRgb pixel = line[x * 16 + px];
                    const bool opaque = qAlpha(pixel) >= 128;
                    opaquePixels += opaque;
                    edgeOpaquePixels += opaque && (px == 0 || py == 0 || px == 15 || py == 15);
                    if (opaque && (px == 0 || py == 0 || px == 15 || py == 15))
                        edgeBrightness += qGray(pixel);
                }
            }
            const int coverage = opaquePixels * 100 / 256;
            const bool blocked = coverage < 50;
            CollisionSuggestion suggestion;
            suggestion.x = x;
            suggestion.y = y;
            suggestion.collision = defaultCollision;
            suggestion.elevation = defaultElevation;
            if (blocked && blockedCollision >= 0)
                suggestion.collision = static_cast<uint16_t>(blockedCollision);
            const bool raised = !blocked && coverage >= 95 && edgeOpaquePixels == 60
                && edgeBrightness / 60 > 180 && defaultElevation < maxElevation;
            if (raised)
                suggestion.elevation = defaultElevation + 1;
            if (blocked) {
                if (blockedCollision >= 0) {
                    suggestion.confidence = coverage < 15 ? 94 : 78;
                    suggestion.rationale = QStringLiteral(
                        "Only %1% of the cell is opaque; suggest the selected blocked collision value %2.")
                        .arg(coverage).arg(blockedCollision);
                    ++result.blockedCount;
                } else {
                    suggestion.confidence = 20;
                    suggestion.rationale = QStringLiteral(
                        "Low opacity suggests a boundary, but blocked inference is disabled; retain project defaults.");
                    ++result.uncertainCount;
                }
            } else if (raised) {
                suggestion.confidence = 61;
                suggestion.rationale = QStringLiteral(
                    "Opaque boundary and bright surface suggest a raised edge; review elevation.");
            } else {
                suggestion.confidence = 42;
                suggestion.rationale = QStringLiteral(
                    "No reliable collision or elevation cue; retain walkable/base defaults.");
                ++result.uncertainCount;
            }
            result.suggestions.append(suggestion);
        }
    }
    result.valid = true;
    return result;
}

} // namespace Studio