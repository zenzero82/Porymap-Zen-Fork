#include "studio/rombuilddialog.h"

#include "config.h"
#include "project.h"

#include <QComboBox>
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVBoxLayout>

namespace Studio {

RomBuildDialog::RomBuildDialog(Project *project, QWidget *parent)
    : QDialog(parent), project(project)
{
    setWindowTitle(QStringLiteral("Build and Test ROM"));
    resize(760, 620);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    operationCombo = new QComboBox(this);
    operationCombo->addItems({QStringLiteral("Build ROM"), QStringLiteral("Run Tests")});
    executableEdit = new QLineEdit(this);
    argumentsEdit = new QPlainTextEdit(this);
    argumentsEdit->setPlaceholderText(QStringLiteral("[\"argument one\", \"--flag=value\", \"\"]"));
    argumentsEdit->setMaximumHeight(100);
    workingDirectoryEdit = new QLineEdit(this);
    timeoutEdit = new QSpinBox(this);
    timeoutEdit->setRange(1, 1440);
    timeoutEdit->setSuffix(QStringLiteral(" min"));
    timeoutEdit->setValue(10);
    auto *executableButton = new QPushButton(QStringLiteral("Choose…"), this);
    auto *directoryButton = new QPushButton(QStringLiteral("Choose…"), this);
    auto *executableRow = new QWidget(this);
    auto *executableLayout = new QHBoxLayout(executableRow);
    executableLayout->setContentsMargins(0, 0, 0, 0);
    executableLayout->addWidget(executableEdit);
    executableLayout->addWidget(executableButton);
    auto *directoryRow = new QWidget(this);
    auto *directoryLayout = new QHBoxLayout(directoryRow);
    directoryLayout->setContentsMargins(0, 0, 0, 0);
    directoryLayout->addWidget(workingDirectoryEdit);
    directoryLayout->addWidget(directoryButton);
    form->addRow(QStringLiteral("Operation:"), operationCombo);
    form->addRow(QStringLiteral("Executable:"), executableRow);
    form->addRow(QStringLiteral("Arguments:"), argumentsEdit);
    form->addRow(QStringLiteral("Working directory:"), directoryRow);
    form->addRow(QStringLiteral("Timeout:"), timeoutEdit);
    layout->addLayout(form);

    statusLabel = new QLabel(QStringLiteral("Configure a command, then run it."), this);
    layout->addWidget(statusLabel);
    outputEdit = new QPlainTextEdit(this);
    outputEdit->setReadOnly(true);
    outputEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(outputEdit, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    runButton = buttons->addButton(QStringLiteral("Run"), QDialogButtonBox::AcceptRole);
    auto *saveButton = buttons->addButton(QStringLiteral("Save Settings"), QDialogButtonBox::ActionRole);
    cancelButton = buttons->addButton(QStringLiteral("Cancel"), QDialogButtonBox::RejectRole);
    cancelButton->setEnabled(false);
    layout->addWidget(buttons);

    loadOperationFields();
    connect(operationCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &RomBuildDialog::operationChanged);
    connect(executableButton, &QPushButton::clicked, this, &RomBuildDialog::chooseExecutable);
    connect(directoryButton, &QPushButton::clicked, this, &RomBuildDialog::chooseWorkingDirectory);
    connect(runButton, &QPushButton::clicked, this, &RomBuildDialog::runCommand);
    connect(saveButton, &QPushButton::clicked, this, [this] {
        saveOperationFields(operationCombo->currentIndex());
        statusLabel->setText(QStringLiteral("Command settings saved."));
    });
    connect(cancelButton, &QPushButton::clicked, this, &RomBuildDialog::cancelCommand);
    connect(buttons, &QDialogButtonBox::rejected, this, &RomBuildDialog::reject);
    connect(&process, &RomBuildProcess::outputReceived, this, &RomBuildDialog::receiveOutput);
    connect(&process, &RomBuildProcess::finished, this, &RomBuildDialog::commandFinished);
}

RomBuildDialog::~RomBuildDialog()
{
    process.cancelAndWait();
}

QStringList RomBuildDialog::arguments() const
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(argumentsEdit->toPlainText().toUtf8(), &error);
    QStringList result;
    if (error.error != QJsonParseError::NoError || !document.isArray())
        return result;
    for (const QJsonValue &value : document.array())
        if (value.isString()) result.append(value.toString());
    return result;
}

void RomBuildDialog::loadOperationFields()
{
    const bool build = operationCombo->currentIndex() == 0;
    executableEdit->setText(build ? projectConfig.romBuildExecutable : projectConfig.romTestExecutable);
    const QStringList args = build ? projectConfig.romBuildArguments : projectConfig.romTestArguments;
    argumentsEdit->setPlainText(QString::fromUtf8(
        QJsonDocument(QJsonArray::fromStringList(args)).toJson(QJsonDocument::Indented)));
    QString directory = build ? projectConfig.romBuildWorkingDirectory : projectConfig.romTestWorkingDirectory;
    workingDirectoryEdit->setText(directory.isEmpty() && project ? project->root : directory);
    timeoutEdit->setValue(qMax(1, projectConfig.romBuildTimeoutMs / 60000));
}

void RomBuildDialog::saveOperationFields(int operationIndex)
{
    const bool build = operationIndex == 0;
    if (build) {
        projectConfig.romBuildExecutable = executableEdit->text().trimmed();
        projectConfig.romBuildArguments = arguments();
        projectConfig.romBuildWorkingDirectory = workingDirectoryEdit->text().trimmed();
    } else {
        projectConfig.romTestExecutable = executableEdit->text().trimmed();
        projectConfig.romTestArguments = arguments();
        projectConfig.romTestWorkingDirectory = workingDirectoryEdit->text().trimmed();
    }
    projectConfig.romBuildTimeoutMs = timeoutEdit->value() * 60000;
    if (project) projectConfig.save();
}

void RomBuildDialog::operationChanged(int)
{
    if (process.isRunning()) return;
    saveOperationFields(loadedOperationIndex);
    loadedOperationIndex = operationCombo->currentIndex();
    loadOperationFields();
}

void RomBuildDialog::chooseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose ROM command"), executableEdit->text());
    if (!path.isEmpty()) executableEdit->setText(path);
}

void RomBuildDialog::chooseWorkingDirectory()
{
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose working directory"),
                                                            workingDirectoryEdit->text());
    if (!path.isEmpty()) workingDirectoryEdit->setText(path);
}

void RomBuildDialog::setRunning(bool running)
{
    operationCombo->setEnabled(!running);
    executableEdit->setEnabled(!running);
    argumentsEdit->setEnabled(!running);
    workingDirectoryEdit->setEnabled(!running);
    timeoutEdit->setEnabled(!running);
    runButton->setEnabled(!running);
    cancelButton->setEnabled(running);
}

void RomBuildDialog::runCommand()
{
    QJsonParseError parseError;
    const QJsonDocument parsedArguments = QJsonDocument::fromJson(argumentsEdit->toPlainText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !parsedArguments.isArray()) {
        QMessageBox::warning(this, QStringLiteral("Invalid arguments"),
                             QStringLiteral("Arguments must be a JSON array of strings."));
        return;
    }
    for (const QJsonValue &value : parsedArguments.array()) {
        if (!value.isString()) {
            QMessageBox::warning(this, QStringLiteral("Invalid arguments"),
                                 QStringLiteral("Every argument must be a JSON string."));
            return;
        }
    }
    saveOperationFields(operationCombo->currentIndex());
    RomBuildRequest request;
    request.executable = executableEdit->text().trimmed();
    request.arguments = arguments();
    request.workingDirectory = workingDirectoryEdit->text().trimmed();
    request.timeoutMs = timeoutEdit->value() * 60000;
    request.operation = operationCombo->currentIndex() == 0 ? RomBuildOperation::Build : RomBuildOperation::Test;
    QString error;
    if (!process.start(request, &error)) {
        QMessageBox::warning(this, QStringLiteral("Unable to run ROM command"), error);
        statusLabel->setText(error);
        return;
    }
    outputEdit->clear();
    statusLabel->setText(QStringLiteral("Running…"));
    setRunning(true);
}

void RomBuildDialog::reject()
{
    if (process.isRunning()) {
        QMessageBox::information(this, QStringLiteral("Command still running"),
                                 QStringLiteral("Cancel the running command before closing this window."));
        return;
    }
    saveOperationFields(operationCombo->currentIndex());
    QDialog::reject();
}

void RomBuildDialog::closeEvent(QCloseEvent *event)
{
    if (process.isRunning()) {
        event->ignore();
        QMessageBox::information(this, QStringLiteral("Command still running"),
                                 QStringLiteral("Cancel the running command before closing this window."));
        return;
    }
    saveOperationFields(operationCombo->currentIndex());
    QDialog::closeEvent(event);
}

void RomBuildDialog::cancelCommand()
{
    process.cancel();
    statusLabel->setText(QStringLiteral("Canceling…"));
}

void RomBuildDialog::receiveOutput(const QString &text, bool standardError)
{
    outputEdit->moveCursor(QTextCursor::End);
    outputEdit->insertPlainText(standardError ? QStringLiteral("[stderr] ") + text : text);
    outputEdit->moveCursor(QTextCursor::End);
}

void RomBuildDialog::commandFinished(const RomBuildResult &result)
{
    setRunning(false);
    if (result.success) {
        statusLabel->setText(QStringLiteral("Completed successfully in %1 ms.").arg(result.elapsedMs));
    } else {
        statusLabel->setText(result.error);
        if (!result.error.isEmpty()) outputEdit->appendPlainText(QStringLiteral("\nERROR: ") + result.error);
    }
}

} // namespace Studio