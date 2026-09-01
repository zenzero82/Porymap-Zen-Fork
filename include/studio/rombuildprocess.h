#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QTimer>

namespace Studio {

enum class RomBuildOperation { Build, Test };

struct RomBuildRequest {
    QString executable;
    QStringList arguments;
    QString workingDirectory;
    int timeoutMs = 600000;
    RomBuildOperation operation = RomBuildOperation::Build;
};

struct RomBuildResult {
    bool success = false;
    bool canceled = false;
    bool timedOut = false;
    QString error;
    QString standardOutput;
    QString standardError;
    int exitCode = -1;
    qint64 elapsedMs = 0;
};

class RomBuildProcess : public QObject
{
    Q_OBJECT
public:
    explicit RomBuildProcess(QObject *parent = nullptr);
    bool start(const RomBuildRequest &request, QString *error = nullptr);
    bool isRunning() const;
    static bool validateRequest(const RomBuildRequest &request, QString *error = nullptr);
    static QString encodeText(const QString &text);
    static QString decodeText(const QString &encoded, bool *ok = nullptr);
    static QString encodeArguments(const QStringList &arguments);
    static QStringList decodeArguments(const QString &encoded, bool *ok = nullptr);
    void cancelAndWait(int timeoutMs = 3000);

public slots:
    void cancel();

signals:
    void outputReceived(const QString &text, bool standardError);
    void finished(const Studio::RomBuildResult &result);

private slots:
    void readStandardOutput();
    void readStandardError();
    void processFinished(int exitCode, QProcess::ExitStatus status);
    void processTimedOut();

private:
    QProcess process;
    QTimer timeoutTimer;
    QElapsedTimer elapsedTimer;
    RomBuildRequest activeRequest;
    RomBuildResult activeResult;
};

} // namespace Studio

Q_DECLARE_METATYPE(Studio::RomBuildResult)