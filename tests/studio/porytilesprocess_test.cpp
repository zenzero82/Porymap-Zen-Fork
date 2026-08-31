#include "studio/porytilesprocess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <iostream>

using namespace Studio;

static bool check(bool condition, const char *message)
{
    if (!condition) std::cerr << "FAILED: " << message << '\n';
    return condition;
}

static bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    return file.write(data) == data.size();
}

static bool writePng(const QString &path)
{
    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(Qt::magenta);
    return image.save(path);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    QString actual;
    QString error;
    ok &= check(PorytilesProcess::validateVersionOutput("porytiles-legacy 1.2.3\n", "1.2.3", &actual, &error),
                "exact version should pass");
    ok &= check(actual == "1.2.3", "actual version should be captured");
    ok &= check(!PorytilesProcess::validateVersionOutput("porytiles-legacy 1.2.4\n", "1.2.3", nullptr, &error),
                "mismatched version should fail");

    PorytilesRequest secondary;
    secondary.operation = PorytilesOperation::Decompile;
    secondary.tilesetKind = PorytilesTilesetKind::Secondary;
    const QStringList arguments = PorytilesProcess::buildArguments(
        secondary, "/secondary", "/output", "/constants.h", "/primary-source", "/primary-compiled");
    ok &= check(arguments == QStringList({
                    "decompile-secondary", "-o", "/output", "/secondary", "/primary-compiled", "/constants.h"
                }), "secondary decompile arguments should use positional primary compiled context");

    QTemporaryDir output;
    QDir().mkpath(output.filePath("palettes"));
    writePng(output.filePath("tiles.png"));
    writeFile(output.filePath("metatiles.bin"), "metatiles");
    writeFile(output.filePath("metatile_attributes.bin"), "attributes");
    writeFile(output.filePath("palettes/00.pal"), "palette");
    PorytilesRequest compile;
    compile.operation = PorytilesOperation::Compile;
    compile.expectedPaletteCount = 1;
    compile.expectedPaletteFiles = {"00.pal"};
    QStringList files;
    ok &= check(PorytilesProcess::validateOutput(compile, output.path(), &files, &error),
                "complete compile output should pass");
    QFile::remove(output.filePath("metatiles.bin"));
    ok &= check(!PorytilesProcess::validateOutput(compile, output.path(), nullptr, &error),
                "incomplete compile output should fail");

    QTemporaryDir staged;
    QTemporaryDir targetParent;
    QDir().mkpath(staged.filePath("palettes"));
    writePng(staged.filePath("tiles.png"));
    writeFile(staged.filePath("metatiles.bin"), "new");
    writeFile(staged.filePath("metatile_attributes.bin"), "new");
    writeFile(staged.filePath("palettes/00.pal"), "new");
    const QString target = targetParent.filePath("tileset");
    QDir().mkpath(target);
    writeFile(QDir(target).filePath("animations.inc"), "preserve");
    writeFile(staged.filePath("animations.inc"), "malicious overwrite");
    ok &= check(PorytilesProcess::applyStagedOutput(compile, staged.path(), target, &error),
                "validated output should apply");
    ok &= check(QFileInfo::exists(QDir(target).filePath("animations.inc")),
                "applying output should preserve unrelated target files");
    QFile preserved(QDir(target).filePath("animations.inc"));
    preserved.open(QIODevice::ReadOnly);
    ok &= check(preserved.readAll() == "preserve",
                "staged files outside the validated manifest must not overwrite target files");

    QFile::rename(staged.filePath("palettes/00.pal"), staged.filePath("palettes/wrong.pal"));
    ok &= check(!PorytilesProcess::validateOutput(compile, staged.path(), nullptr, &error),
                "palette filenames must match the selected tileset");

    if (ok) std::cout << "All Porytiles adapter tests passed.\n";
    return ok ? 0 : 1;
}