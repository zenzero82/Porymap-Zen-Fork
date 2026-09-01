#pragma once

#include "studio/imagetilesetbuilder.h"

#include <QStringList>

namespace Studio {

class AssetTilesetBuilder
{
public:
    struct Result {
        ImageTilesetBuilder::Result tileset;
        QImage sourceImage;
        QStringList assetPaths;
        int sourceTileCount = 0;
        QString errorMessage;

        bool isValid() const
        {
            return errorMessage.isEmpty() && tileset.isValid();
        }
    };

    Result build(
        const QStringList &assetPaths,
        const ImageTilesetBuilder::Options &options
    ) const;
};

} // namespace Studio