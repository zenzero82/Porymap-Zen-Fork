#include "studio/aiassistantservice.h"

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>

namespace Studio {

static bool validatePayloadFields(const AiRequest &request, QString *error)
{
    const QString host = request.endpoint.host().toLower();
    const bool loopback = host == QStringLiteral("localhost")
                          || host == QStringLiteral("127.0.0.1")
                          || host == QStringLiteral("::1");
    const bool safeScheme = request.endpoint.scheme() == QStringLiteral("https")
                            || (request.endpoint.scheme() == QStringLiteral("http") && loopback);
    if (!request.endpoint.isValid() || !safeScheme || host.isEmpty()) {
        if (error) *error = QStringLiteral("Use an HTTPS AI endpoint, or HTTP only for localhost.");
        return false;
    }
    if (request.prompt.trimmed().isEmpty()) {
        if (error) *error = QStringLiteral("Enter a request before sending.");
        return false;
    }
    if (request.prompt.size() > AiAssistantService::maxPromptCharacters
        || request.context.size() > AiAssistantService::maxContextCharacters) {
        if (error) *error = QStringLiteral("The request is too large. Reduce the prompt or context.");
        return false;
    }
    if (request.timeoutMs < 1000 || request.timeoutMs > 300000) {
        if (error) *error = QStringLiteral("Timeout must be between 1 and 300 seconds.");
        return false;
    }
    return true;
}

AiAssistantService::AiAssistantService(QObject *parent) : QObject(parent)
{
    network = new QNetworkAccessManager(this);
    timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    connect(timeoutTimer, &QTimer::timeout, this, &AiAssistantService::timedOut);
}

QString AiAssistantService::redactText(const QString &text)
{
    QString redacted = text;
    static const QRegularExpression secretPattern(
        QStringLiteral("(?i)\\b(api[_-]?key|token|secret|password)\\s*[:=]\\s*[^\\s,;]+"));
    redacted.replace(secretPattern, QStringLiteral("\\1=[redacted]"));
    static const QRegularExpression unixPath(QStringLiteral("(?<![\\w])/(?:[^\\s/]+/)+[^\\s]+"));
    redacted.replace(unixPath, QStringLiteral("[path redacted]"));
    static const QRegularExpression windowsPath(QStringLiteral("(?<![\\w])[A-Za-z]:\\\\[^\\s]+"));
    redacted.replace(windowsPath, QStringLiteral("[path redacted]"));
    return redacted;
}

bool AiAssistantService::validateRequest(const AiRequest &request, QString *error)
{
    if (!validatePayloadFields(request, error)) return false;
    if (!request.consentToSend) {
        if (error) *error = QStringLiteral("External AI sending requires explicit confirmation.");
        return false;
    }
    return true;
}

QByteArray AiAssistantService::buildPayload(const AiRequest &request, QString *error)
{
    if (!validatePayloadFields(request, error)) return {};
    QJsonObject payload;
    payload.insert(QStringLiteral("tool"), QStringLiteral("porymap-studio-assistant"));
    payload.insert(QStringLiteral("prompt"), redactText(request.prompt));
    if (!request.context.trimmed().isEmpty())
        payload.insert(QStringLiteral("context"), redactText(request.context));
    return QJsonDocument(payload).toJson(QJsonDocument::Compact);
}

bool AiAssistantService::start(const AiRequest &request, QString *error)
{
    if (isRunning()) {
        if (error) *error = QStringLiteral("An AI request is already running.");
        return false;
    }
    if (!validateRequest(request, error)) return false;
    const QByteArray payload = buildPayload(request, error);
    if (payload.isEmpty()) return false;
    if (!request.tokenEnvironmentVariable.trimmed().isEmpty()
        && qEnvironmentVariable(request.tokenEnvironmentVariable.toUtf8().constData()).isEmpty()) {
        if (error) *error = QStringLiteral("The configured token environment variable is not set.");
        return false;
    }

    QNetworkRequest networkRequest(request.endpoint);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    networkRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    const QString token = qEnvironmentVariable(request.tokenEnvironmentVariable.toUtf8().constData());
    if (!token.isEmpty())
        networkRequest.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());

    activeRequest = request;
    responseBuffer.clear();
    canceled = false;
    timedOutFlag = false;
    responseTooLarge = false;
    startedAt = QDateTime::currentMSecsSinceEpoch();
    reply = network->post(networkRequest, payload);
    connect(reply, &QNetworkReply::readyRead, this, &AiAssistantService::readReply);
    connect(reply, &QNetworkReply::finished, this, &AiAssistantService::replyFinished);
    timeoutTimer->start(request.timeoutMs);
    return true;
}

bool AiAssistantService::isRunning() const
{
    return reply != nullptr;
}

void AiAssistantService::readReply()
{
    if (!reply) return;
    const qint64 remaining = maxResponseBytes - responseBuffer.size();
    const QByteArray chunk = reply->read(remaining + 1);
    if (chunk.size() > remaining) {
        responseBuffer.append(chunk.left(remaining));
        responseTooLarge = true;
        reply->abort();
        return;
    }
    responseBuffer.append(chunk);
}

void AiAssistantService::timedOut()
{
    if (!reply) return;
    timedOutFlag = true;
    reply->abort();
}

void AiAssistantService::cancel()
{
    if (!reply) return;
    canceled = true;
    reply->abort();
}

void AiAssistantService::replyFinished()
{
    if (!reply) return;
    QNetworkReply *completedReply = reply;
    if (completedReply->isOpen())
        readReply();
    timeoutTimer->stop();

    AiResult result;
    result.canceled = canceled;
    result.elapsedMs = QDateTime::currentMSecsSinceEpoch() - startedAt;
    result.httpStatus = completedReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (canceled) {
        result.error = QStringLiteral("AI request canceled.");
    } else if (timedOutFlag) {
        result.error = QStringLiteral("AI request timed out.");
    } else if (responseTooLarge) {
        result.error = QStringLiteral("AI response exceeded the 32 KiB limit.");
    } else if (result.httpStatus != 0 && (result.httpStatus < 200 || result.httpStatus >= 300)) {
        result.error = QStringLiteral("AI endpoint returned HTTP %1.").arg(result.httpStatus);
    } else if (completedReply->error() != QNetworkReply::NoError) {
        result.error = completedReply->errorString();
    } else {
        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBuffer, &parseError);
        if (parseError.error == QJsonParseError::NoError && document.isObject()) {
            const QJsonObject object = document.object();
            if (object.value(QStringLiteral("response")).isString())
                result.response = object.value(QStringLiteral("response")).toString();
            else if (object.value(QStringLiteral("text")).isString())
                result.response = object.value(QStringLiteral("text")).toString();
            else if (object.value(QStringLiteral("output")).isString())
                result.response = object.value(QStringLiteral("output")).toString();
            else if (object.value(QStringLiteral("choices")).isArray()
                     && !object.value(QStringLiteral("choices")).toArray().isEmpty()) {
                const QJsonObject choice = object.value(QStringLiteral("choices")).toArray().first().toObject();
                result.response = choice.value(QStringLiteral("text")).toString();
                if (result.response.isEmpty())
                    result.response = choice.value(QStringLiteral("message")).toObject().value(QStringLiteral("content")).toString();
            }
        }
        if (result.response.isEmpty())
            result.response = QString::fromUtf8(responseBuffer);
        result.success = true;
    }

    completedReply->deleteLater();
    reply = nullptr;
    emit finished(result);
}

} // namespace Studio