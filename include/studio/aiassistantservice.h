#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace Studio {

struct AiRequest {
    QUrl endpoint;
    QString prompt;
    QString context;
    QString tokenEnvironmentVariable;
    int timeoutMs = 30000;
    bool consentToSend = false;
};

struct AiResult {
    bool success = false;
    bool canceled = false;
    QString response;
    QString error;
    int httpStatus = 0;
    qint64 elapsedMs = 0;
};

class AiAssistantService : public QObject
{
    Q_OBJECT

public:
    explicit AiAssistantService(QObject *parent = nullptr);

    static constexpr int maxPromptCharacters = 12000;
    static constexpr int maxContextCharacters = 4000;
    static constexpr int maxResponseBytes = 32768;

    static QString redactText(const QString &text);
    static QByteArray buildPayload(const AiRequest &request, QString *error = nullptr);
    static bool validateRequest(const AiRequest &request, QString *error = nullptr);

    bool start(const AiRequest &request, QString *error = nullptr);
    void cancel();
    bool isRunning() const;

signals:
    void finished(const Studio::AiResult &result);

private slots:
    void readReply();
    void replyFinished();
    void timedOut();

private:
    QNetworkAccessManager *network = nullptr;
    QNetworkReply *reply = nullptr;
    QTimer *timeoutTimer = nullptr;
    AiRequest activeRequest;
    QByteArray responseBuffer;
    qint64 startedAt = 0;
    bool canceled = false;
    bool timedOutFlag = false;
    bool responseTooLarge = false;
};

} // namespace Studio

Q_DECLARE_METATYPE(Studio::AiResult)