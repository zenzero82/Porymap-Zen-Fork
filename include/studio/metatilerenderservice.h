#pragma once

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>

class Tileset;

namespace Studio {

class MetatileRenderService
{
public:
    struct RenderContext {
        QList<int> layerOrder;
        QList<float> layerOpacity;
        QString description;
    };

    enum class SourceTileset {
        Primary,
        Secondary,
    };

    struct RenderedMetatile {
        uint16_t metatileId = 0;
        SourceTileset source = SourceTileset::Primary;
        QString sourceTilesetName;
        QImage image;
    };

    struct Result {
        QList<RenderedMetatile> metatiles;
        int availableMetatileCount = 0;
        QStringList warnings;
        QString errorMessage;

        bool isValid() const { return errorMessage.isEmpty(); }
    };

    Result renderAll(
        const Tileset *primaryTileset,
        const Tileset *secondaryTileset,
        const RenderContext &renderContext) const;

    bool exportPngs(const Result &result, const QString &directoryPath, QString *errorMessage = nullptr) const;
};

} // namespace Studio