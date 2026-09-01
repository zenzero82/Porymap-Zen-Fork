#pragma once

#include <QImage>
#include <QString>
#include <QStringList>

namespace Studio {

struct ArtworkSourceRequest {
    QString artworkPath;
    QString outputDirectory;
    int maxUniqueTiles = 1024;
    int maxColors = 256;
    int maxMetatiles = 1024;
    int maxPalettes = 16;
    QString forbiddenRoot;
    bool secondary = false;
    QString primarySourceDirectory;
};

struct ArtworkSourceResult {
    bool success = false;
    QString error;
    QStringList generatedFiles;
    int metatileCount = 0;
    int uniqueTileCount = 0;
    int colorCount = 0;
    int paletteCount = 0;
    QString outputDirectory;
};

class ArtworkSourceGenerator
{
public:
    ArtworkSourceResult generate(const ArtworkSourceRequest &request) const;

private:
    static bool saveImage(const QImage &image, const QString &path, QString *error);
    static int countColors(const QImage &image);
};

} // namespace Studio