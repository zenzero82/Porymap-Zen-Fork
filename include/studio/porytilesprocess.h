#pragma once

#include <QObject>
#include <QElapsedTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QTimer>
#include <QVersionNumber>

namespace Studio {

enum class PorytilesOperation {
    Compile,
    Decompile,
};

enum class PorytilesTilesetKind {
    Primary,
    Secondary,
};

struct PorytilesRequest {
    QString executable;
    QString expectedVersion;
    PorytilesOperation operation = PorytilesOperation::Compile;
    PorytilesTilesetKind tilesetKind = PorytilesTilesetKind::Primary;
    QString sourceDirectory;
    QString targetDirectory;
    QString constantsFile;
    QString primarySourceDirectory;
    QString primaryCompiledDirectory;
    int expectedPaletteCount = 0;
    QStringList expectedPaletteFiles;
    int timeoutMs = 120000;
};

struct PorytilesResult {
    bool success = false;
    bool canceled = false;
    QString error;
    QString version;
    QString standardOutput;
    QString standardError;
    QString stagingDirectory;
    QString stagedOutputDirectory;
    QString targetDirectory;
    QStringList outputFiles;
    qint64 elapsedMs = 0;
};

class PorytilesProcess : public QObject
{
    Q_OBJECT
public:
    explicit PorytilesProcess(QObject *parent = nullptr);
    ~PorytilesProcess() override;

    bool start(const PorytilesRequest &request, QString *error = nullptr);
    bool isRunning() const;

    static QStringList buildArguments(const PorytilesRequest &request,
                                      const QString &stagedSourceDirectory,
                                      const QString &stagedOutputDirectory,
                                      const QString &stagedConstantsFile,
                                      const QString &stagedPrimarySourceDirectory = QString(),
                                      const QString &stagedPrimaryCompiledDirectory = QString());
    static bool validateVersionOutput(const QString &output,
                                      const QString &expectedVersion,
                                      QString *actualVersion = nullptr,
                                      QString *error = nullptr);
    static bool validateOutput(const PorytilesRequest &request,
                               const QString &outputDirectory,
                               QStringList *outputFiles = nullptr,
                               QString *error = nullptr);
    static bool applyStagedOutput(const PorytilesRequest &request,
                                  const QString &stagedOutputDirectory,
                                  const QString &targetDirectory,
                                  QString *error = nullptr);

public slots:
    void cancel();

signals:
    void outputReceived(const QString &text, bool standardError);
    void finished(const Studio::PorytilesResult &result);

private slots:
    void readStandardOutput();
    void readStandardError();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processTimedOut();

private:
    bool prepareStaging(const PorytilesRequest &request, QString *error);
    static bool copyDirectory(const QString &source, const QString &destination, QString *error);
    static bool copyDirectoryContents(const QString &source, const QString &destination, QString *error);
    static QStringList listFiles(const QString &directory);
    static bool pathIsSafeFile(const QString &path);

    QProcess *process = nullptr;
    QTimer *timeoutTimer = nullptr;
    QElapsedTimer *elapsedTimer = nullptr;
    QTemporaryDir *staging = nullptr;
    PorytilesRequest activeRequest;
    PorytilesResult activeResult;
};

} // namespace Studio

Q_DECLARE_METATYPE(Studio::PorytilesResult)