#include "studio/metatilerenderservice.h"

#include "imageproviders.h"
#include "metatile.h"
#include "tileset.h"

#include <QDir>
#include <QFileInfo>

#include <limits>

namespace Studio {

MetatileRenderService::Result MetatileRenderService::renderAll(
    const Tileset *primaryTileset,
    const Tileset *secondaryTileset,
    const RenderContext &renderContext) const
{
    Result result;
    if (!primaryTileset) {
        result.errorMessage = QStringLiteral("Select a valid primary tileset.");
        return result;
    }
    if (!secondaryTileset) {
        result.errorMessage = QStringLiteral("Select a valid secondary tileset.");
        return result;
    }
    if (primaryTileset->is_secondary || !secondaryTileset->is_secondary) {
        result.errorMessage = QStringLiteral("The selected tilesets do not have valid primary/secondary roles.");
        return result;
    }

    if (renderContext.layerOrder.isEmpty() || renderContext.layerOpacity.isEmpty()) {
        result.errorMessage = QStringLiteral("The metatile rendering context is incomplete.");
        return result;
    }

    const QList<QPair<const Tileset *, SourceTileset>> tilesets = {
        {primaryTileset, SourceTileset::Primary},
        {secondaryTileset, SourceTileset::Secondary},
    };

    for (const auto &[tileset, source] : tilesets) {
        if (tileset->numMetatiles() <= 0) {
            result.warnings.append(QString("Tileset '%1' has no metatiles to render.").arg(tileset->name));
            continue;
        }

        const int firstId = tileset->firstMetatileId();
        for (int index = 0; index < tileset->numMetatiles(); index++) {
            const int rawId = firstId + index;
            if (rawId < 0 || rawId > std::numeric_limits<uint16_t>::max()) {
                result.warnings.append(QString("Tileset '%1' has an out-of-range metatile ID.").arg(tileset->name));
                continue;
            }

            const uint16_t metatileId = static_cast<uint16_t>(rawId);
            if (!Tileset::getMetatile(metatileId, primaryTileset, secondaryTileset)) {
                result.warnings.append(QString("Metatile %1 from '%2' could not be loaded.")
                    .arg(Metatile::getMetatileIdString(metatileId), tileset->name));
                continue;
            }

            result.availableMetatileCount++;
            QImage image = getMetatileImage(
                metatileId,
                primaryTileset,
                secondaryTileset,
                renderContext.layerOrder,
                renderContext.layerOpacity
            );
            if (image.isNull() || image.size() != Metatile::pixelSize()) {
                result.warnings.append(QString("Metatile %1 from '%2' could not be rendered.")
                    .arg(Metatile::getMetatileIdString(metatileId), tileset->name));
                continue;
            }

            result.metatiles.append({
                .metatileId = metatileId,
                .source = source,
                .sourceTilesetName = tileset->name,
                .image = image,
            });
        }
    }

    if (result.availableMetatileCount == 0) {
        result.errorMessage = QStringLiteral("Neither selected tileset provides a renderable metatile.");
        return result;
    }
    if (!result.warnings.isEmpty()) {
        result.errorMessage = QString(
            "Could not render a complete metatile reference library. Resolve every listed issue before matching."
        );
    }
    if (result.metatiles.count() != result.availableMetatileCount) {
        result.errorMessage = QString(
            "Only %1 of %2 available metatiles rendered. Resolve the failed candidates before matching."
        ).arg(result.metatiles.count())
         .arg(result.availableMetatileCount);
    }
    return result;
}

bool MetatileRenderService::exportPngs(
    const Result &result,
    const QString &directoryPath,
    QString *errorMessage) const
{
    QDir directory(directoryPath);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QString("Could not create debug directory '%1'.").arg(directoryPath);
        }
        return false;
    }

    for (const RenderedMetatile &metatile : result.metatiles) {
        const QString owner = metatile.source == SourceTileset::Primary
            ? QStringLiteral("primary")
            : QStringLiteral("secondary");
        const QString filename = QString("%1_%2.png")
            .arg(owner)
            .arg(Metatile::getIndexInTileset(metatile.metatileId), 4, 16, QLatin1Char('0'));
        const QString filepath = directory.filePath(filename);
        if (!metatile.image.save(filepath, "PNG")) {
            if (errorMessage) {
                *errorMessage = QString("Could not write '%1'.").arg(filepath);
            }
            return false;
        }
    }

    return true;
}

} // namespace Studio