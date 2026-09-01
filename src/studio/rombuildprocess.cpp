#include "studio/rombuildprocess.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

namespace Studio {

RomBuildProcess::RomBuildProcess(QObject *parent) : QObject(parent)
{
    connect(&process, &QProcess::readyReadStandardOutput, this, &RomBuildProcess::readStandardOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &RomBuildProcess::readStandardError);
    connect(&process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &RomBuildProcess::processFinished);
    connect(&timeoutTimer, &QTimer::timeout, this, &RomBuildProcess::processTimedOut);
}

bool RomBuildProcess::validateRequest(const RomBuildRequest &request, QString *error)
{
    if (request.executable.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Choose a build or test executable first.");
        return false;
    }
    if (request.workingDirectory.isEmpty() || !QDir(request.workingDirectory).exists()) {
        if (error) *error = QStringLiteral("The configured working directory does not exist.");
        return false;
    }
    if (request.timeoutMs <= 0) {
        if (error) *error = QStringLiteral("The timeout must be greater than zero.");
        return false;
    }
    return true;
}

QString RomBuildProcess::encodeText(const QString &text)
{
    return QString::fromLatin1(text.toUtf8().toBase64());
}

QString RomBuildProcess::decodeText(const QString &encoded, bool *ok)
{
    const QByteArray decoded = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    const bool valid = encoded.isEmpty() || !decoded.isEmpty();
    if (ok) *ok = valid;
    return valid ? QString::fromUtf8(decoded) : QString();
}

QString RomBuildProcess::encodeArguments(const QStringList &arguments)
{
    return QString::fromLatin1(
        QJsonDocument(QJsonArray::fromStringList(arguments)).toJson(QJsonDocument::Compact).toBase64());
}

QStringList RomBuildProcess::decodeArguments(const QString &encoded, bool *ok)
{
    const QByteArray json = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    const bool valid = parseError.error == QJsonParseError::NoError && document.isArray();
    if (ok) *ok = valid;
    if (!valid) return {};
    QStringList result;
    for (const QJsonValue &value : document.array()) {
        if (!value.isString()) {
            if (ok) *ok = false;
            return {};
        }
        result.append(value.toString());
    }
    return result;
}

bool RomBuildProcess::start(const RomBuildRequest &request, QString *error)
{
    if (isRunning()) {
        if (error) *error = QStringLiteral("A ROM build or test is already running.");
        return false;
    }
    if (!validateRequest(request, error))
        return false;
    activeRequest = request;
    activeResult = {};
    process.setProgram(request.executable);
    process.setArguments(request.arguments);
    process.setWorkingDirectory(request.workingDirectory);
    elapsedTimer.start();
    timeoutTimer.start(request.timeoutMs);
    process.start();
    if (!process.waitForStarted(5000)) {
        timeoutTimer.stop();
        if (error) *error = QStringLiteral("Could not start '%1': %2").arg(request.executable, process.errorString());
        return false;
    }
    return true;
}

bool RomBuildProcess::isRunning() const
{
    return process.state() != QProcess::NotRunning;
}

void RomBuildProcess::readStandardOutput()
{
    const QString text = QString::fromLocal8Bit(process.readAllStandardOutput());
    activeResult.standardOutput += text;
    emit outputReceived(text, false);
}

void RomBuildProcess::readStandardError()
{
    const QString text = QString::fromLocal8Bit(process.readAllStandardError());
    activeResult.standardError += text;
    emit outputReceived(text, true);
}

void RomBuildProcess::processTimedOut()
{
    activeResult.timedOut = true;
    activeResult.error = QStringLiteral("The %1 command exceeded the configured timeout.")
        .arg(activeRequest.operation == RomBuildOperation::Build ? QStringLiteral("build") : QStringLiteral("test"));
    process.kill();
}

void RomBuildProcess::cancel()
{
    if (!isRunning()) return;
    activeResult.canceled = true;
    activeResult.error = QStringLiteral("The ROM command was canceled.");
    process.kill();
}

void RomBuildProcess::cancelAndWait(int timeoutMs)
{
    if (!isRunning()) return;
    cancel();
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(timeoutMs);
    }
}

void RomBuildProcess::processFinished(int exitCode, QProcess::ExitStatus status)
{
    timeoutTimer.stop();
    readStandardOutput();
    readStandardError();
    activeResult.exitCode = exitCode;
    activeResult.elapsedMs = elapsedTimer.elapsed();
    if (!activeResult.canceled && !activeResult.timedOut && status == QProcess::NormalExit && exitCode == 0) {
        activeResult.success = true;
    } else if (activeResult.error.isEmpty()) {
        activeResult.error = status == QProcess::CrashExit
            ? QStringLiteral("The command terminated unexpectedly.")
            : QStringLiteral("The command failed with exit code %1.").arg(exitCode);
    }
    emit finished(activeResult);
}

} // namespace Studio