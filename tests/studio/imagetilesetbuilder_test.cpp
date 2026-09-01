#include "studio/imagetilesetbuilder.h"

#include <QImage>

#include <iostream>

Tile::Tile()
    : tileId(0), xflip(false), yflip(false), palette(0)
{}

Tile::Tile(uint16_t tileId, uint16_t xflip, uint16_t yflip, uint16_t palette)
    : tileId(tileId), xflip(xflip), yflip(yflip), palette(palette)
{}

namespace {

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        failures++;
    }
}

Studio::ImageTilesetBuilder::Options options()
{
    Studio::ImageTilesetBuilder::Options result;
    result.maxTiles = 32;
    result.maxMetatiles = 32;
    result.tileIdBase = 100;
    result.metatileIdBase = 300;
    result.paletteId = 6;
    result.tilesPerMetatile = 8;
    return result;
}

void testRejectsMisalignedImages()
{
    Studio::ImageTilesetBuilder builder;
    const auto result = builder.build(QImage(17, 16, QImage::Format_ARGB32), options());
    expect(!result.isValid(), "misaligned image should be rejected");
    expect(!result.errorMessage.isEmpty(), "misaligned image should explain the error");
}

void testDeduplicatesTilesAndMetatiles()
{
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(qRgb(40, 80, 120));

    Studio::ImageTilesetBuilder builder;
    const auto result = builder.build(image, options());
    expect(result.isValid(), "solid aligned image should build");
    expect(result.mapSize == QSize(2, 1), "map dimensions should use the metatile grid");
    expect(result.uniqueTileCount == 2, "identical source tiles should share one entry after transparent tile zero");
    expect(result.uniqueMetatileCount == 2, "transparent metatile zero and the source metatile should be stored");
    expect(
        result.mapMetatileIds == QVector<uint16_t>({301, 301}),
        "map cells should reference the deduplicated metatile"
    );
    expect(result.metatiles.at(1).tiles.size() == 8, "project layer count should be preserved");
    bool tileReferencesValid = true;
    for (int index = 0; index < result.metatiles.at(1).tiles.size(); index++) {
        const Tile &tile = result.metatiles.at(1).tiles.at(index);
        const uint16_t expectedTileId = index < Metatile::tilesPerLayer() ? 101 : 100;
        tileReferencesValid = tileReferencesValid
            && tile.tileId == expectedTileId
            && tile.palette == 6;
    }
    expect(tileReferencesValid, "unused layers should use transparent tile zero");
}

void testOpaquePixelsNeverUseTransparentIndex()
{
    QImage image(16, 16, QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            image.setPixel(x, y, qRgb(x * 15, y * 15, (x + y) * 7));
        }
    }

    Studio::ImageTilesetBuilder builder;
    const auto result = builder.build(image, options());
    expect(result.isValid(), "many-color image should build through quantization");
    expect(result.quantized, "more than 15 opaque colors should report quantization");
    expect(result.palette.size() == 16, "generated palette should contain exactly 16 entries");
    bool foundTransparentOpaquePixel = false;
    for (int y = 0; y < result.indexedSource.height(); y++) {
        const uchar *line = result.indexedSource.constScanLine(y);
        for (int x = 0; x < result.indexedSource.width(); x++) {
            if (line[x] == 0) foundTransparentOpaquePixel = true;
        }
    }
    expect(!foundTransparentOpaquePixel, "opaque artwork should not be assigned transparent palette index zero");
}

void testTransparentPixelsUseIndexZero()
{
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(qRgba(0, 0, 0, 0));
    image.setPixel(8, 8, qRgb(255, 0, 0));

    Studio::ImageTilesetBuilder builder;
    const auto result = builder.build(image, options());
    expect(result.isValid(), "transparent artwork should build");
    expect(result.indexedSource.pixelIndex(0, 0) == 0, "transparent pixels should use index zero");
    expect(result.indexedSource.pixelIndex(8, 8) != 0, "opaque pixels should use a visible index");
}

void testCapacityFailuresAreExplicit()
{
    QImage image(16, 16, QImage::Format_ARGB32);
    image.fill(Qt::black);
    image.fill(qRgb(10, 10, 10));
    for (int y = 0; y < 8; y++) {
        for (int x = 0; x < 8; x++) image.setPixel(x, y, qRgb(255, 0, 0));
        for (int x = 8; x < 16; x++) image.setPixel(x, y, qRgb(0, 255, 0));
    }
    for (int y = 8; y < 16; y++) {
        for (int x = 0; x < 8; x++) image.setPixel(x, y, qRgb(0, 0, 255));
        for (int x = 8; x < 16; x++) image.setPixel(x, y, qRgb(255, 255, 0));
    }

    auto limited = options();
    limited.maxTiles = 2;
    Studio::ImageTilesetBuilder builder;
    const auto result = builder.build(image, limited);
    expect(!result.isValid(), "tile overflow should reject the build");
    expect(result.errorMessage.contains("2 tiles"), "tile overflow should report project capacity");
}

void testSplitsOverflowAcrossBothRoles()
{
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < 16; x++) image.setPixel(x, y, qRgb(255, 0, 0));
        for (int x = 16; x < 32; x++) image.setPixel(x, y, qRgb(0, 255, 0));
    }

    auto primary = options();
    primary.maxTiles = 2;
    auto secondary = options();
    secondary.maxTiles = 2;
    secondary.tileIdBase = 200;
    secondary.metatileIdBase = 700;
    secondary.paletteId = 7;

    Studio::ImageTilesetBuilder builder;
    expect(!builder.build(image, primary).isValid(), "one role should overflow");
    const auto pair = builder.buildPair(image, primary, secondary);
    expect(pair.isValid(), "the same image should fit when split across both roles");
    expect(pair.primary.uniqueTileCount == 2, "primary should stay within its tile limit");
    expect(pair.secondary.uniqueTileCount == 2, "secondary should stay within its tile limit");
    expect(pair.mapMetatileIds == QVector<uint16_t>({301, 701}),
           "map cells should reference their assigned tileset role");
}

} // namespace

int main()
{
    testRejectsMisalignedImages();
    testDeduplicatesTilesAndMetatiles();
    testOpaquePixelsNeverUseTransparentIndex();
    testTransparentPixelsUseIndexZero();
    testCapacityFailuresAreExplicit();
    testSplitsOverflowAcrossBothRoles();

    if (failures == 0) {
        std::cout << "All image tileset builder tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}