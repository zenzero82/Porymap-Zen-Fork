#include "studio/aiassistantdialog.h"

#include "config.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Studio {

AiAssistantDialog::AiAssistantDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Optional AI Assistant"));
    resize(780, 720);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    endpointEdit = new QLineEdit(this);
    endpointEdit->setPlaceholderText(QStringLiteral("https://your-approved-ai-endpoint.example/v1/assist"));
    tokenEnvironmentEdit = new QLineEdit(this);
    tokenEnvironmentEdit->setPlaceholderText(QStringLiteral("Optional environment variable name"));
    timeoutEdit = new QSpinBox(this);
    timeoutEdit->setRange(1, 300);
    timeoutEdit->setSuffix(QStringLiteral(" sec"));
    form->addRow(QStringLiteral("Endpoint:"), endpointEdit);
    form->addRow(QStringLiteral("Token variable:"), tokenEnvironmentEdit);
    form->addRow(QStringLiteral("Timeout:"), timeoutEdit);
    layout->addLayout(form);

    auto *notice = new QLabel(
        QStringLiteral("This tool sends only the text below after you explicitly enable sending. "
                       "Project files, images, paths, and map data are never attached automatically."),
        this);
    notice->setWordWrap(true);
    layout->addWidget(notice);

    promptEdit = new QPlainTextEdit(this);
    promptEdit->setPlaceholderText(QStringLiteral("Describe what you want help with…"));
    promptEdit->setMaximumHeight(110);
    contextEdit = new QPlainTextEdit(this);
    contextEdit->setPlaceholderText(QStringLiteral("Optional non-sensitive context (no files or paths)."));
    contextEdit->setMaximumHeight(85);
    layout->addWidget(new QLabel(QStringLiteral("Request:"), this));
    layout->addWidget(promptEdit);
    layout->addWidget(new QLabel(QStringLiteral("Context:"), this));
    layout->addWidget(contextEdit);

    consentCheck = new QCheckBox(QStringLiteral("I reviewed the preview and allow sending this text externally"), this);
    layout->addWidget(consentCheck);
    layout->addWidget(new QLabel(QStringLiteral("Payload preview:"), this));
    previewEdit = new QPlainTextEdit(this);
    previewEdit->setReadOnly(true);
    previewEdit->setMaximumHeight(125);
    previewEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(previewEdit);
    statusLabel = new QLabel(QStringLiteral("AI tools are optional and disabled until enabled for this request."), this);
    layout->addWidget(statusLabel);
    layout->addWidget(new QLabel(QStringLiteral("Assistant response (suggestion only):"), this));
    responseEdit = new QPlainTextEdit(this);
    responseEdit->setReadOnly(true);
    layout->addWidget(responseEdit, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    sendButton = buttons->addButton(QStringLiteral("Send Request"), QDialogButtonBox::AcceptRole);
    cancelButton = buttons->addButton(QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
    cancelButton->setEnabled(false);
    layout->addWidget(buttons);

    endpointEdit->setText(porymapConfig.aiEndpoint);
    tokenEnvironmentEdit->setText(porymapConfig.aiTokenEnvironmentVariable);
    timeoutEdit->setValue(qBound(1, porymapConfig.aiTimeoutSeconds, 300));
    consentCheck->setChecked(false);
    sendButton->setEnabled(false);
    const auto requestChanged = [this] {
        if (consentCheck->isChecked())
            consentCheck->setChecked(false);
        else
            refreshPreview();
    };
    connect(endpointEdit, &QLineEdit::textChanged, this, requestChanged);
    connect(tokenEnvironmentEdit, &QLineEdit::textChanged, this, requestChanged);
    connect(timeoutEdit, qOverload<int>(&QSpinBox::valueChanged), this, requestChanged);
    connect(promptEdit, &QPlainTextEdit::textChanged, this, requestChanged);
    connect(contextEdit, &QPlainTextEdit::textChanged, this, requestChanged);
    connect(consentCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        sendButton->setEnabled(enabled && !service.isRunning());
        refreshPreview();
    });
    connect(sendButton, &QPushButton::clicked, this, &AiAssistantDialog::sendRequest);
    connect(cancelButton, &QPushButton::clicked, this, &AiAssistantDialog::cancelRequest);
    connect(buttons, &QDialogButtonBox::rejected, this, &AiAssistantDialog::reject);
    connect(&service, &AiAssistantService::finished, this, &AiAssistantDialog::receiveResult);
    refreshPreview();
}

AiAssistantDialog::~AiAssistantDialog()
{
    service.cancel();
}

AiRequest AiAssistantDialog::requestFromFields() const
{
    AiRequest request;
    request.endpoint = QUrl(endpointEdit->text().trimmed());
    request.prompt = promptEdit->toPlainText();
    request.context = contextEdit->toPlainText();
    request.tokenEnvironmentVariable = tokenEnvironmentEdit->text().trimmed();
    request.timeoutMs = timeoutEdit->value() * 1000;
    request.consentToSend = consentCheck->isChecked();
    return request;
}

void AiAssistantDialog::refreshPreview()
{
    QString error;
    const QByteArray payload = AiAssistantService::buildPayload(requestFromFields(), &error);
    previewEdit->setPlainText(payload.isEmpty()
                                  ? QStringLiteral("Preview unavailable until the request is valid and sending is enabled:\n") + error
                                  : QString::fromUtf8(payload));
}

void AiAssistantDialog::saveSettings()
{
    porymapConfig.aiEndpoint = endpointEdit->text().trimmed();
    porymapConfig.aiTokenEnvironmentVariable = tokenEnvironmentEdit->text().trimmed();
    porymapConfig.aiTimeoutSeconds = timeoutEdit->value();
    porymapConfig.save();
}

void AiAssistantDialog::setRunning(bool running)
{
    endpointEdit->setEnabled(!running);
    tokenEnvironmentEdit->setEnabled(!running);
    timeoutEdit->setEnabled(!running);
    promptEdit->setEnabled(!running);
    contextEdit->setEnabled(!running);
    consentCheck->setEnabled(!running);
    sendButton->setEnabled(!running && consentCheck->isChecked());
    cancelButton->setEnabled(running);
}

void AiAssistantDialog::sendRequest()
{
    const AiRequest request = requestFromFields();
    QString error;
    if (!AiAssistantService::validateRequest(request, &error)) {
        statusLabel->setText(error);
        refreshPreview();
        return;
    }
    saveSettings();
    responseEdit->clear();
    if (!service.start(request, &error)) {
        statusLabel->setText(error);
        return;
    }
    setRunning(true);
    statusLabel->setText(QStringLiteral("Sending approved text…"));
}

void AiAssistantDialog::cancelRequest()
{
    service.cancel();
    statusLabel->setText(QStringLiteral("Canceling…"));
}

void AiAssistantDialog::receiveResult(const AiResult &result)
{
    setRunning(false);
    if (result.success) {
        responseEdit->setPlainText(result.response);
        statusLabel->setText(QStringLiteral("Suggestion received in %1 ms. Nothing was changed.").arg(result.elapsedMs));
    } else {
        statusLabel->setText(result.error);
        responseEdit->setPlainText(result.error);
    }
}

void AiAssistantDialog::reject()
{
    if (service.isRunning()) {
        statusLabel->setText(QStringLiteral("Cancel the request before closing this window."));
        return;
    }
    QDialog::reject();
}

void AiAssistantDialog::closeEvent(QCloseEvent *event)
{
    if (service.isRunning()) {
        event->ignore();
        statusLabel->setText(QStringLiteral("Cancel the request before closing this window."));
        return;
    }
    QDialog::closeEvent(event);
}

} // namespace Studio