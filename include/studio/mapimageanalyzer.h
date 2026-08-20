#pragma once

#include <QImage>
#include <QSize>
#include <QString>

namespace Studio {

class MapImageAnalyzer
{
public:
    struct Result {
        QString sourcePath;
        QImage sourceImage;
        QSize imageSize;
        QSize mapSize;
        QSize metatilePixelSize;
        bool loaded = false;
        bool gridAligned = false;
        QString errorMessage;
        QString alignmentMessage;

        bool isReadyForRendering() const { return loaded && gridAligned; }
    };

    Result analyzePng(const QString &filepath) const;
};

} // namespace Studio