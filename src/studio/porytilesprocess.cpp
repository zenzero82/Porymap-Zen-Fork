#include "studio/porytilesprocess.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

namespace Studio {

namespace {

QString operationName(PorytilesOperation operation)
{
    return operation == PorytilesOperation::Compile ? QStringLiteral("compile") : QStringLiteral("decompile");
}

QString kindName(PorytilesTilesetKind kind)
{
    return kind == PorytilesTilesetKind::Primary ? QStringLiteral("primary") : QStringLiteral("secondary");
}

bool copyFileChecked(const QString &source, const QString &destination, QString *error)
{
    QFileInfo sourceInfo(source);
    if (!sourceInfo.exists() || !sourceInfo.isFile() || sourceInfo.isSymLink()) {
        if (error) *error = QStringLiteral("Staging source is not a regular file: %1").arg(source);
        return false;
    }
    QFile::remove(destination);
    if (!QFile::copy(source, destination)) {
        if (error) *error = QStringLiteral("Could not stage '%1' as '%2'.").arg(source, destination);
        return false;
    }
    return true;
}

} // namespace

PorytilesProcess::PorytilesProcess(QObject *parent)
    : QObject(parent),
      process(new QProcess(this)),
      timeoutTimer(new QTimer(this)),
      elapsedTimer(new QElapsedTimer),
      staging(nullptr)
{
    timeoutTimer->setSingleShot(true);
    connect(process, &QProcess::readyReadStandardOutput, this, &PorytilesProcess::readStandardOutput);
    connect(process, &QProcess::readyReadStandardError, this, &PorytilesProcess::readStandardError);
    connect(process, &QProcess::finished, this, &PorytilesProcess::processFinished);
    connect(timeoutTimer, &QTimer::timeout, this, &PorytilesProcess::processTimedOut);
}

PorytilesProcess::~PorytilesProcess()
{
    if (isRunning()) {
        process->kill();
        process->waitForFinished(1000);
    }
    delete elapsedTimer;
    delete staging;
}

bool PorytilesProcess::isRunning() const
{
    return process->state() != QProcess::NotRunning;
}

QStringList PorytilesProcess::buildArguments(const PorytilesRequest &request,
                                             const QString &stagedSourceDirectory,
                                             const QString &stagedOutputDirectory,
                                             const QString &stagedConstantsFile,
                                             const QString &stagedPrimarySourceDirectory,
                                             const QString &stagedPrimaryCompiledDirectory)
{
    const QString prefix = operationName(request.operation) + QLatin1Char('-') + kindName(request.tilesetKind);
    QStringList arguments;
    arguments << prefix;

    if (request.operation == PorytilesOperation::Compile) {
        arguments << QStringLiteral("-dual-layer") << QStringLiteral("-Wall");
    }
    arguments << QStringLiteral("-o") << stagedOutputDirectory;
    arguments << stagedSourceDirectory;

    if (request.tilesetKind == PorytilesTilesetKind::Secondary) {
        arguments << (request.operation == PorytilesOperation::Compile
                          ? stagedPrimarySourceDirectory
                          : stagedPrimaryCompiledDirectory);
    }
    arguments << stagedConstantsFile;
    return arguments;
}

bool PorytilesProcess::validateVersionOutput(const QString &output,
                                             const QString &expectedVersion,
                                             QString *actualVersion,
                                             QString *error)
{
    static const QRegularExpression versionExpression(QStringLiteral(R"((\d+(?:\.\d+){1,3}))"));
    const QRegularExpressionMatch match = versionExpression.match(output);
    if (!match.hasMatch()) {
        if (error) *error = QStringLiteral("Porytiles did not report a semantic version.");
        return false;
    }

    const QString version = match.captured(1);
    if (actualVersion) *actualVersion = version;
    if (expectedVersion.isEmpty()) {
        if (error) *error = QStringLiteral("An expected Porytiles version must be configured.");
        return false;
    }

    if (QVersionNumber::fromString(version) != QVersionNumber::fromString(expectedVersion)) {
        if (error) {
            *error = QStringLiteral("Porytiles version %1 does not match the configured version %2.")
                         .arg(version, expectedVersion);
        }
        return false;
    }
    return true;
}

bool PorytilesProcess::pathIsSafeFile(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() && info.isFile() && !info.isSymLink() && info.size() > 0;
}

QStringList PorytilesProcess::listFiles(const QString &directory)
{
    QStringList files;
    QDir dir(directory);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                    QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isSymLink()) {
            continue;
        }
        if (entry.isDir()) {
            for (const QString &child : listFiles(entry.absoluteFilePath())) {
                files.append(entry.fileName() + QLatin1Char('/') + child);
            }
        } else if (entry.isFile()) {
            files.append(entry.fileName());
        }
    }
    return files;
}

bool PorytilesProcess::validateOutput(const PorytilesRequest &request,
                                      const QString &outputDirectory,
                                      QStringList *outputFiles,
                                      QString *error)
{
    const QFileInfo outputInfo(outputDirectory);
    if (!outputInfo.exists() || !outputInfo.isDir() || outputInfo.isSymLink()) {
        if (error) *error = QStringLiteral("Porytiles did not produce a safe output directory.");
        return false;
    }

    QStringList requiredFiles;
    if (request.operation == PorytilesOperation::Compile) {
        requiredFiles = {
            QStringLiteral("tiles.png"),
            QStringLiteral("metatiles.bin"),
            QStringLiteral("metatile_attributes.bin")
        };
        const QString palettesDirectory = QDir(outputDirectory).filePath(QStringLiteral("palettes"));
        const QFileInfo palettesInfo(palettesDirectory);
        if (!palettesInfo.exists() || !palettesInfo.isDir() || palettesInfo.isSymLink()
            || request.expectedPaletteCount <= 0) {
            if (error) *error = QStringLiteral("Porytiles output cannot be validated without the expected palette set.");
            return false;
        }
        const QStringList palettes = QDir(palettesDirectory).entryList(
            {QStringLiteral("*.pal")}, QDir::Files, QDir::Name);
        QStringList expectedPalettes = request.expectedPaletteFiles;
        expectedPalettes.sort();
        if (palettes.size() != request.expectedPaletteCount || palettes != expectedPalettes) {
            if (error) {
                *error = QStringLiteral("Porytiles palette filenames do not match the selected tileset.");
            }
            return false;
        }
        for (const QString &palette : palettes) {
            if (!pathIsSafeFile(QDir(palettesDirectory).filePath(palette))) {
                if (error) *error = QStringLiteral("Porytiles produced an invalid palette: %1").arg(palette);
                return false;
            }
        }
    } else {
        requiredFiles = {
            QStringLiteral("bottom.png"),
            QStringLiteral("middle.png"),
            QStringLiteral("top.png"),
            QStringLiteral("attributes.csv")
        };
    }

    for (const QString &filename : requiredFiles) {
        const QString path = QDir(outputDirectory).filePath(filename);
        if (!pathIsSafeFile(path)) {
            if (error) *error = QStringLiteral("Porytiles output is missing or invalid: %1").arg(filename);
            return false;
        }
        if (filename.endsWith(QStringLiteral(".png"))) {
            QImageReader reader(path);
            if (!reader.canRead() || reader.size().isEmpty()) {
                if (error) *error = QStringLiteral("Porytiles produced an unreadable image: %1").arg(filename);
                return false;
            }
        }
    }

    if (outputFiles) *outputFiles = listFiles(outputDirectory);
    return true;
}

bool PorytilesProcess::copyDirectoryContents(const QString &source,
                                             const QString &destination,
                                             QString *error)
{
    const QDir sourceDir(source);
    if (!sourceDir.exists()) {
        if (error) *error = QStringLiteral("Directory does not exist: %1").arg(source);
        return false;
    }
    QDir destinationDir(destination);
    if (!destinationDir.exists() && !destinationDir.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Could not create staging directory: %1").arg(destination);
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                          QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isSymLink()) {
            if (error) *error = QStringLiteral("Symlinks are not allowed in staged Porytiles inputs: %1").arg(entry.fileName());
            return false;
        }
        const QString destinationPath = destinationDir.filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectory(entry.absoluteFilePath(), destinationPath, error)) {
                return false;
            }
        } else if (!copyFileChecked(entry.absoluteFilePath(), destinationPath, error)) {
            return false;
        }
    }
    return true;
}

bool PorytilesProcess::copyDirectory(const QString &source, const QString &destination, QString *error)
{
    QDir().mkpath(destination);
    return copyDirectoryContents(source, destination, error);
}

bool PorytilesProcess::prepareStaging(const PorytilesRequest &request, QString *error)
{
    QFileInfo executableInfo(request.executable);
    if (!executableInfo.exists() || !executableInfo.isFile() || !executableInfo.isExecutable()) {
        if (error) *error = QStringLiteral("Porytiles executable is not a runnable file: %1").arg(request.executable);
        return false;
    }
    if (!QFileInfo(request.sourceDirectory).isDir()) {
        if (error) *error = QStringLiteral("Porytiles source directory does not exist: %1").arg(request.sourceDirectory);
        return false;
    }
    if (!QFileInfo(request.constantsFile).isFile()) {
        if (error) *error = QStringLiteral("Metatile behavior constants file does not exist: %1").arg(request.constantsFile);
        return false;
    }
    if (request.targetDirectory.isEmpty()) {
        if (error) *error = QStringLiteral("A target directory is required.");
        return false;
    }
    if (request.tilesetKind == PorytilesTilesetKind::Secondary) {
        const QString primaryContext = request.operation == PorytilesOperation::Compile
            ? request.primarySourceDirectory : request.primaryCompiledDirectory;
        if (!QFileInfo(primaryContext).isDir()) {
            if (error) *error = QStringLiteral("A valid primary tileset context is required for secondary operations.");
            return false;
        }
    }

    staging = new QTemporaryDir(QStringLiteral("porymap-porytiles-"));
    if (!staging->isValid()) {
        if (error) *error = QStringLiteral("Could not create a temporary Porytiles staging directory.");
        return false;
    }

    const QString root = staging->path();
    const QString stagedSource = QDir(root).filePath(QStringLiteral("input"));
    const QString stagedOutput = QDir(root).filePath(QStringLiteral("output"));
    const QString stagedConstants = QDir(root).filePath(QStringLiteral("metatile_behaviors.h"));
    if (!copyDirectory(request.sourceDirectory, stagedSource, error)
        || !QDir().mkpath(stagedOutput)
        || !copyFileChecked(request.constantsFile, stagedConstants, error)) {
        return false;
    }

    QString stagedPrimarySource;
    QString stagedPrimaryCompiled;
    if (request.tilesetKind == PorytilesTilesetKind::Secondary) {
        if (!request.primarySourceDirectory.isEmpty()) {
            stagedPrimarySource = QDir(root).filePath(QStringLiteral("primary-input"));
            if (!copyDirectory(request.primarySourceDirectory, stagedPrimarySource, error)) {
                return false;
            }
        }
        if (!request.primaryCompiledDirectory.isEmpty()) {
            stagedPrimaryCompiled = QDir(root).filePath(QStringLiteral("primary-compiled"));
            if (!copyDirectory(request.primaryCompiledDirectory, stagedPrimaryCompiled, error)) {
                return false;
            }
        }
    }

    activeResult.stagingDirectory = root;
    activeResult.stagedOutputDirectory = stagedOutput;
    activeResult.targetDirectory = request.targetDirectory;
    process->setProgram(executableInfo.absoluteFilePath());
    process->setArguments(buildArguments(request, stagedSource, stagedOutput, stagedConstants,
                                         stagedPrimarySource, stagedPrimaryCompiled));
    process->setWorkingDirectory(root);
    return true;
}

bool PorytilesProcess::start(const PorytilesRequest &request, QString *error)
{
    if (isRunning()) {
        if (error) *error = QStringLiteral("A Porytiles process is already running.");
        return false;
    }
    if (request.timeoutMs <= 0) {
        if (error) *error = QStringLiteral("Porytiles timeout must be greater than zero.");
        return false;
    }

    QProcess probe;
    probe.setProgram(request.executable);
    probe.setArguments({QStringLiteral("--version")});
    probe.start();
    if (!probe.waitForFinished(qMin(request.timeoutMs, 5000))) {
        probe.kill();
        if (error) *error = QStringLiteral("Timed out while checking the Porytiles version.");
        return false;
    }
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        if (error) *error = QStringLiteral("Porytiles version check failed: %1").arg(QString::fromLocal8Bit(probe.readAllStandardError()).trimmed());
        return false;
    }
    QString versionError;
    QString version;
    const QString versionOutput = QString::fromLocal8Bit(probe.readAllStandardOutput());
    if (!validateVersionOutput(versionOutput, request.expectedVersion, &version, &versionError)) {
        if (error) *error = versionError;
        return false;
    }

    activeRequest = request;
    activeResult = {};
    activeResult.version = version;
    if (!prepareStaging(request, error)) {
        delete staging;
        staging = nullptr;
        return false;
    }

    elapsedTimer->start();
    timeoutTimer->start(request.timeoutMs);
    process->start();
    if (!process->waitForStarted(5000)) {
        timeoutTimer->stop();
        if (error) *error = QStringLiteral("Could not start Porytiles: %1").arg(process->errorString());
        delete staging;
        staging = nullptr;
        return false;
    }
    return true;
}

void PorytilesProcess::readStandardOutput()
{
    const QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
    activeResult.standardOutput += text;
    emit outputReceived(text, false);
}

void PorytilesProcess::readStandardError()
{
    const QString text = QString::fromLocal8Bit(process->readAllStandardError());
    activeResult.standardError += text;
    emit outputReceived(text, true);
}

void PorytilesProcess::processTimedOut()
{
    activeResult.error = QStringLiteral("Porytiles exceeded the configured timeout.");
    process->kill();
}

void PorytilesProcess::cancel()
{
    if (!isRunning()) {
        return;
    }
    activeResult.canceled = true;
    activeResult.error = QStringLiteral("Porytiles operation canceled.");
    process->kill();
}

void PorytilesProcess::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    timeoutTimer->stop();
    readStandardOutput();
    readStandardError();
    activeResult.elapsedMs = elapsedTimer->elapsed();

    if (!activeResult.canceled && activeResult.error.isEmpty()
        && exitStatus == QProcess::NormalExit && exitCode == 0) {
        QString validationError;
        if (validateOutput(activeRequest, activeResult.stagedOutputDirectory,
                           &activeResult.outputFiles, &validationError)) {
            activeResult.success = true;
        } else {
            activeResult.error = validationError;
        }
    } else if (activeResult.error.isEmpty()) {
        activeResult.error = exitStatus == QProcess::CrashExit
            ? QStringLiteral("Porytiles terminated unexpectedly.")
            : QStringLiteral("Porytiles failed with exit code %1.").arg(exitCode);
    }

    emit finished(activeResult);
}

bool PorytilesProcess::applyStagedOutput(const PorytilesRequest &request,
                                         const QString &stagedOutputDirectory,
                                         const QString &targetDirectory,
                                         QString *error)
{
    if (!validateOutput(request, stagedOutputDirectory, nullptr, error)) {
        return false;
    }
    const QFileInfo targetInfo(targetDirectory);
    QDir parent = targetInfo.dir();
    if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Could not create target parent directory.");
        return false;
    }

    const QString stagedSibling = parent.filePath(targetInfo.fileName() + QStringLiteral(".porymap-staged-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString backupSibling = parent.filePath(targetInfo.fileName() + QStringLiteral(".porymap-backup-") + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (targetInfo.exists() && !copyDirectory(targetDirectory, stagedSibling, error)) {
        return false;
    }
    QStringList manifest;
    if (request.operation == PorytilesOperation::Compile) {
        manifest = {
            QStringLiteral("tiles.png"),
            QStringLiteral("metatiles.bin"),
            QStringLiteral("metatile_attributes.bin")
        };
        for (const QString &palette : request.expectedPaletteFiles) {
            manifest.append(QStringLiteral("palettes/") + palette);
        }
    } else {
        manifest = {
            QStringLiteral("bottom.png"),
            QStringLiteral("middle.png"),
            QStringLiteral("top.png"),
            QStringLiteral("attributes.csv")
        };
    }
    for (const QString &relativePath : manifest) {
        const QString source = QDir(stagedOutputDirectory).filePath(relativePath);
        const QString destination = QDir(stagedSibling).filePath(relativePath);
        if (!QDir().mkpath(QFileInfo(destination).absolutePath())
            || !copyFileChecked(source, destination, error)) {
            QDir(stagedSibling).removeRecursively();
            return false;
        }
    }

    bool movedTarget = false;
    if (targetInfo.exists()) {
        movedTarget = parent.rename(targetInfo.fileName(), QFileInfo(backupSibling).fileName());
        if (!movedTarget) {
            QDir(stagedSibling).removeRecursively();
            if (error) *error = QStringLiteral("Could not safely move the existing tileset aside.");
            return false;
        }
    }
    if (!parent.rename(QFileInfo(stagedSibling).fileName(), targetInfo.fileName())) {
        if (movedTarget) {
            parent.rename(QFileInfo(backupSibling).fileName(), targetInfo.fileName());
        }
        QDir(stagedSibling).removeRecursively();
        if (error) *error = QStringLiteral("Could not install the validated Porytiles output.");
        return false;
    }
    if (movedTarget) {
        QDir(backupSibling).removeRecursively();
    }
    return true;
}

} // namespace Studio