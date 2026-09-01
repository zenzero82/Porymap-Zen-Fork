#include "studio/aiassistantservice.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <iostream>

using Studio::AiAssistantService;
using Studio::AiRequest;

struct Exchange {
    Studio::AiResult result;
    QByteArray requestBody;
};

static Exchange runExchange(AiRequest request, const QByteArray &response, bool cancel = false)
{
    QTcpServer server;
    server.listen(QHostAddress::LocalHost);
    request.endpoint = QUrl(QStringLiteral("http://127.0.0.1:%1/assist").arg(server.serverPort()));

    QByteArray incoming;
    QByteArray capturedBody;
    QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
        QTcpSocket *socket = server.nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, response] {
            incoming.append(socket->readAll());
            const int headerEnd = incoming.indexOf("\r\n\r\n");
            if (headerEnd < 0) return;
            const QByteArray headers = incoming.left(headerEnd);
            const QRegularExpression lengthPattern(QStringLiteral("(?i)Content-Length:\\s*(\\d+)"));
            const QRegularExpressionMatch match = lengthPattern.match(QString::fromLatin1(headers));
            if (!match.hasMatch()) return;
            const int contentLength = match.captured(1).toInt();
            if (incoming.size() < headerEnd + 4 + contentLength) return;
            capturedBody = incoming.mid(headerEnd + 4, contentLength);
            if (!response.isEmpty()) {
                socket->write(response);
                socket->flush();
            }
        });
    });

    Studio::AiAssistantService service;
    QEventLoop loop;
    Exchange exchange;
    QObject::connect(&service, &Studio::AiAssistantService::finished, &loop,
                     [&](const Studio::AiResult &result) {
        exchange.result = result;
        loop.quit();
    });
    QString error;
    if (!service.start(request, &error)) {
        exchange.result.error = error;
        return exchange;
    }
    if (cancel)
        QTimer::singleShot(25, &service, &Studio::AiAssistantService::cancel);
    loop.exec();
    exchange.requestBody = capturedBody;
    return exchange;
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);

    AiRequest request;
    request.endpoint = QUrl(QStringLiteral("https://example.test/assist"));
    request.prompt = QStringLiteral("Please review this map. api_key=top-secret /home/runner/private/map.png");
    request.context = QStringLiteral("Keep the route readable.");
    request.consentToSend = true;

    QString error;
    const QByteArray payload = AiAssistantService::buildPayload(request, &error);
    if (payload.isEmpty() || !error.isEmpty()) {
        std::cerr << "Valid AI request was rejected.\n";
        return 1;
    }
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    const QString prompt = object.value(QStringLiteral("prompt")).toString();
    if (prompt.contains(QStringLiteral("top-secret"))
        || prompt.contains(QStringLiteral("/home/runner"))
        || !prompt.contains(QStringLiteral("[redacted]"))
        || !prompt.contains(QStringLiteral("[path redacted]"))) {
        std::cerr << "AI payload redaction failed.\n";
        return 1;
    }

    AiRequest noConsent = request;
    noConsent.consentToSend = false;
    if (AiAssistantService::buildPayload(noConsent, &error).isEmpty()) {
        std::cerr << "Payload preview incorrectly required consent.\n";
        return 1;
    }
    if (AiAssistantService::validateRequest(noConsent, &error) || error.isEmpty()) {
        std::cerr << "Consent was not enforced.\n";
        return 1;
    }
    Studio::AiAssistantService noConsentService;
    if (noConsentService.start(noConsent, &error) || noConsentService.isRunning()) {
        std::cerr << "Transport accepted a request without consent.\n";
        return 1;
    }

    AiRequest tooLarge = request;
    tooLarge.prompt = QString(AiAssistantService::maxPromptCharacters + 1, QLatin1Char('x'));
    if (AiAssistantService::validateRequest(tooLarge, &error) || error.isEmpty()) {
        std::cerr << "Prompt size limit was not enforced.\n";
        return 1;
    }

    AiRequest badEndpoint = request;
    badEndpoint.endpoint = QUrl(QStringLiteral("file:///tmp/request"));
    if (AiAssistantService::validateRequest(badEndpoint, &error) || error.isEmpty()) {
        std::cerr << "Endpoint scheme validation failed.\n";
        return 1;
    }
    badEndpoint.endpoint = QUrl(QStringLiteral("http://example.test/assist"));
    if (AiAssistantService::validateRequest(badEndpoint, &error) || error.isEmpty()) {
        std::cerr << "Insecure remote HTTP endpoint was accepted.\n";
        return 1;
    }
    badEndpoint.endpoint = QUrl(QStringLiteral("http://127.0.0.1:8080/assist"));
    if (!AiAssistantService::validateRequest(badEndpoint, &error)) {
        std::cerr << "Loopback HTTP endpoint was rejected.\n";
        return 1;
    }

    const QByteArray successBody = R"({"response":"review only"})";
    const QByteArray successResponse =
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
        + QByteArray::number(successBody.size()) + "\r\nConnection: close\r\n\r\n" + successBody;
    const Exchange success = runExchange(request, successResponse);
    if (!success.result.success || success.result.response != QStringLiteral("review only")
        || success.requestBody != payload) {
        std::cerr << "AI success response or transmitted payload test failed.\n";
        return 1;
    }

    const QByteArray failureBody = "provider failed";
    const QByteArray failureResponse =
        "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
        + QByteArray::number(failureBody.size()) + "\r\nConnection: close\r\n\r\n" + failureBody;
    const Exchange failure = runExchange(request, failureResponse);
    if (failure.result.success || !failure.result.error.contains(QStringLiteral("500"))) {
        std::cerr << "AI HTTP failure test failed.\n";
        return 1;
    }

    const QByteArray redirectResponse =
        "HTTP/1.1 302 Found\r\nLocation: https://example.invalid/other\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
    const Exchange redirect = runExchange(request, redirectResponse);
    if (redirect.result.success || !redirect.result.error.contains(QStringLiteral("302"))) {
        std::cerr << "AI redirect policy test failed.\n";
        return 1;
    }

    const QByteArray oversizedBody(AiAssistantService::maxResponseBytes + 128, 'x');
    const QByteArray oversizedResponse =
        "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(oversizedBody.size())
        + "\r\nConnection: close\r\n\r\n" + oversizedBody;
    const Exchange oversized = runExchange(request, oversizedResponse);
    if (oversized.result.success
        || !oversized.result.error.contains(QStringLiteral("32 KiB"))) {
        std::cerr << "AI response bound test failed.\n";
        return 1;
    }

    const Exchange canceled = runExchange(request, QByteArray(), true);
    if (!canceled.result.canceled || canceled.result.success) {
        std::cerr << "AI cancellation test failed.\n";
        return 1;
    }

    AiRequest timeoutRequest = request;
    timeoutRequest.timeoutMs = 1000;
    const Exchange timeout = runExchange(timeoutRequest, QByteArray());
    if (timeout.result.success || !timeout.result.error.contains(QStringLiteral("timed out"))) {
        std::cerr << "AI timeout test failed.\n";
        return 1;
    }

    std::cout << "All AI assistant service tests passed.\n";
    return 0;
}