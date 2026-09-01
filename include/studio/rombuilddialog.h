#pragma once

#include <QDialog>

#include "studio/rombuildprocess.h"

class Project;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLabel;
class QSpinBox;
class QCloseEvent;

namespace Studio {

class RomBuildDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RomBuildDialog(Project *project, QWidget *parent = nullptr);
    ~RomBuildDialog() override;

protected:
    void reject() override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void operationChanged(int index);
    void chooseExecutable();
    void chooseWorkingDirectory();
    void runCommand();
    void cancelCommand();
    void receiveOutput(const QString &text, bool standardError);
    void commandFinished(const Studio::RomBuildResult &result);

private:
    void loadOperationFields();
    void saveOperationFields(int operationIndex);
    QStringList arguments() const;
    void setRunning(bool running);

    Project *project;
    QComboBox *operationCombo;
    QLineEdit *executableEdit;
    QPlainTextEdit *argumentsEdit;
    QLineEdit *workingDirectoryEdit;
    QSpinBox *timeoutEdit;
    QPlainTextEdit *outputEdit;
    QLabel *statusLabel;
    QPushButton *runButton;
    QPushButton *cancelButton;
    RomBuildProcess process;
    int loadedOperationIndex = 0;
};

} // namespace Studio