#pragma once

#include <QDialog>

#include "studio/aiassistantservice.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace Studio {

class AiAssistantDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AiAssistantDialog(QWidget *parent = nullptr);
    ~AiAssistantDialog() override;

protected:
    void reject() override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refreshPreview();
    void sendRequest();
    void cancelRequest();
    void receiveResult(const Studio::AiResult &result);

private:
    AiRequest requestFromFields() const;
    void saveSettings();
    void setRunning(bool running);

    QLineEdit *endpointEdit = nullptr;
    QLineEdit *tokenEnvironmentEdit = nullptr;
    QSpinBox *timeoutEdit = nullptr;
    QPlainTextEdit *promptEdit = nullptr;
    QPlainTextEdit *contextEdit = nullptr;
    QCheckBox *consentCheck = nullptr;
    QPlainTextEdit *previewEdit = nullptr;
    QPlainTextEdit *responseEdit = nullptr;
    QLabel *statusLabel = nullptr;
    QPushButton *sendButton = nullptr;
    QPushButton *cancelButton = nullptr;
    AiAssistantService service;
};

} // namespace Studio