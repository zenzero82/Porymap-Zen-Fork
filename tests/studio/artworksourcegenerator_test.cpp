#include "studio/artworksourcegenerator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QTemporaryDir>

#include <iostream>

using namespace Studio;

static bool check(bool condition, const char *message)
{
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}

static QImage makeArtwork()
{
    QImage image(32, 48, QImage::Format_ARGB32);
    QPainter painter(&image);
    painter.fillRect(image.rect(), Qt::black);
    painter.fillRect(QRect(16, 0, 16, 16), QColor(30, 30, 30));
    painter.fillRect(QRect(0, 16, 16, 16), QColor(60, 60, 60));
    painter.fillRect(QRect(16, 32, 16, 16), QColor(90, 90, 90));
    painter.end();
    return image;
}

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

static void writePrimarySource(const QString &path)
{
    QDir().mkpath(path);
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::black);
    for (const QString &name : {QStringLiteral("bottom.png"), QStringLiteral("middle.png"),
                                QStringLiteral("top.png")})
        image.save(QDir(path).filePath(name));
    QFile attributes(QDir(path).filePath(QStringLiteral("attributes.csv")));
    attributes.open(QIODevice::WriteOnly);
    attributes.write("id,behavior\n0,MB_NORMAL\n");
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;
    QTemporaryDir temp;
    const QString artworkPath = QDir(temp.path()).filePath(QStringLiteral("artwork.png"));
    ok &= check(makeArtwork().save(artworkPath), "test artwork should save");

    const QString firstOutput = QDir(temp.path()).filePath(QStringLiteral("first"));
    ArtworkSourceRequest request;
    request.artworkPath = artworkPath;
    request.outputDirectory = firstOutput;
    ArtworkSourceResult result = ArtworkSourceGenerator().generate(request);
    ok &= check(result.success, "valid artwork should generate");
    ok &= check(result.metatileCount == 2, "metatile count should be deterministic");
    ok &= check(result.uniqueTileCount == 4, "unique tile count should deduplicate repeated tiles");
    ok &= check(result.generatedFiles.size() == 4, "three layers and attributes should be generated");
    ok &= check(QFileInfo::exists(QDir(firstOutput).filePath("bottom.png")), "bottom layer should exist");
    ok &= check(QFileInfo::exists(QDir(firstOutput).filePath("attributes.csv")), "attributes should exist");
    ok &= check(readFile(QDir(firstOutput).filePath("attributes.csv")).startsWith("id,behavior\n0,MB_NORMAL\n"),
                "attributes should use the Porytiles legacy CSV schema");

    const QString secondOutput = QDir(temp.path()).filePath(QStringLiteral("second"));
    request.outputDirectory = secondOutput;
    ArtworkSourceResult second = ArtworkSourceGenerator().generate(request);
    ok &= check(second.success, "same artwork should generate twice");
    for (const QString &name : {QStringLiteral("bottom.png"), QStringLiteral("middle.png"),
                                QStringLiteral("top.png"), QStringLiteral("attributes.csv")}) {
        ok &= check(readFile(QDir(firstOutput).filePath(name)) == readFile(QDir(secondOutput).filePath(name)),
                    "generation should be byte-for-byte deterministic");
    }

    QImage invalid(15, 48, QImage::Format_ARGB32);
    const QString invalidPath = QDir(temp.path()).filePath(QStringLiteral("invalid.png"));
    invalid.save(invalidPath);
    request.artworkPath = invalidPath;
    request.outputDirectory = QDir(temp.path()).filePath(QStringLiteral("invalid-output"));
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "misaligned artwork should be rejected");

    request.artworkPath = artworkPath;
    request.maxUniqueTiles = 1;
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "tile allocation overflow should be rejected");

    request.maxUniqueTiles = 1024;
    request.secondary = true;
    request.primarySourceDirectory.clear();
    request.outputDirectory = QDir(temp.path()).filePath(QStringLiteral("missing-context-output"));
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "secondary generation should require primary source context");
    ok &= check(!QFileInfo::exists(request.outputDirectory),
                "rejected secondary generation should not create output");

    request.primarySourceDirectory = QDir(temp.path()).filePath(QStringLiteral("incomplete-primary"));
    QDir().mkpath(request.primarySourceDirectory);
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "incomplete primary source context should be rejected");
    const QString primarySource = QDir(temp.path()).filePath(QStringLiteral("primary-source"));
    writePrimarySource(primarySource);
    request.primarySourceDirectory = primarySource;
    request.outputDirectory = QDir(temp.path()).filePath(QStringLiteral("secondary-output"));
    ok &= check(ArtworkSourceGenerator().generate(request).success,
                "complete primary source context should permit secondary generation");

    QImage paletteArtwork(16, 48, QImage::Format_ARGB32);
    for (int y = 0; y < paletteArtwork.height(); ++y) {
        for (int x = 0; x < paletteArtwork.width(); ++x) {
            const int group = x < 8 ? 0 : 32;
            paletteArtwork.setPixelColor(x, y, QColor(group + ((x + y * 8) % 16), group, group));
        }
    }
    const QString palettePath = QDir(temp.path()).filePath(QStringLiteral("palette-overflow.png"));
    paletteArtwork.save(palettePath);
    request = {};
    request.artworkPath = palettePath;
    request.outputDirectory = QDir(temp.path()).filePath(QStringLiteral("palette-overflow-output"));
    request.maxPalettes = 1;
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "incompatible tile palettes should be rejected");
    ok &= check(!QFileInfo::exists(request.outputDirectory),
                "palette rejection should not create output");

    QImage transparent(16, 48, QImage::Format_ARGB32);
    for (int y = 0; y < transparent.height(); ++y)
        for (int x = 0; x < transparent.width(); ++x)
            transparent.setPixel(x, y, qRgba(x, y, x + y, 0));
    const QString transparentPath = QDir(temp.path()).filePath(QStringLiteral("transparent.png"));
    transparent.save(transparentPath);
    request = {};
    request.artworkPath = transparentPath;
    request.outputDirectory = QDir(temp.path()).filePath(QStringLiteral("transparent-output"));
    request.maxColors = 1;
    request.maxPalettes = 1;
    ArtworkSourceResult transparentResult = ArtworkSourceGenerator().generate(request);
    ok &= check(transparentResult.success && transparentResult.colorCount == 1,
                "transparent RGB values should normalize to one color");

    const QString existing = QDir(temp.path()).filePath(QStringLiteral("existing"));
    QDir().mkpath(existing);
    QFile sentinel(QDir(existing).filePath(QStringLiteral("keep.txt")));
    sentinel.open(QIODevice::WriteOnly);
    sentinel.write("keep");
    sentinel.close();
    request = {};
    request.artworkPath = artworkPath;
    request.outputDirectory = existing;
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "existing output directory should be rejected");
    ok &= check(readFile(sentinel.fileName()) == "keep",
                "rejected output must preserve existing directory contents");

    request.outputDirectory = QDir(temp.path()).filePath(QStringLiteral("project/generated"));
    request.forbiddenRoot = QDir(temp.path()).filePath(QStringLiteral("project"));
    ok &= check(!ArtworkSourceGenerator().generate(request).success,
                "output inside active project should be rejected");
    ok &= check(!QFileInfo::exists(request.outputDirectory),
                "project-root rejection should not create output");

    if (ok) std::cout << "All artwork source generator tests passed.\n";
    return ok ? 0 : 1;
}