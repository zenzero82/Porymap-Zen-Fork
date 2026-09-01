#pragma once

#include "studio/porytilesprocess.h"

#include <QDialog>

class Project;
class Tileset;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

namespace Studio {

class PorytilesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PorytilesDialog(Project *project,
                            Tileset *primaryTileset,
                            Tileset *secondaryTileset,
                            QWidget *parent = nullptr);
    void setSourceDirectory(const QString &directory);
    void setPrimaryContextDirectory(const QString &directory);

signals:
    void compiledOutputApplied();

private slots:
    void browseExecutable();
    void browseSource();
    void browseTarget();
    void browseConstants();
    void browsePrimaryContext();
    void updateOperationUi();
    void startRun();
    void cancelRun();
    void appendOutput(const QString &text, bool standardError);
    void runFinished(const Studio::PorytilesResult &result);

private:
    PorytilesRequest requestFromUi() const;
    Tileset *selectedTileset() const;
    QString compiledDirectory(const Tileset *tileset) const;
    void setRunning(bool running);
    bool fieldsComplete() const;

    Project *project = nullptr;
    Tileset *primaryTileset = nullptr;
    Tileset *secondaryTileset = nullptr;
    PorytilesProcess *runner = nullptr;
    PorytilesRequest activeRequest;

    QLineEdit *executableEdit = nullptr;
    QLineEdit *versionEdit = nullptr;
    QSpinBox *timeoutSpin = nullptr;
    QComboBox *operationCombo = nullptr;
    QComboBox *tilesetCombo = nullptr;
    QLabel *sourceLabel = nullptr;
    QLineEdit *sourceEdit = nullptr;
    QLineEdit *targetEdit = nullptr;
    QLabel *primaryContextLabel = nullptr;
    QWidget *primaryContextRow = nullptr;
    QLineEdit *primaryContextEdit = nullptr;
    QLineEdit *constantsEdit = nullptr;
    QPlainTextEdit *outputEdit = nullptr;
    QLabel *statusLabel = nullptr;
    QPushButton *runButton = nullptr;
    QPushButton *cancelButton = nullptr;
    QDialogButtonBox *buttonBox = nullptr;
};

} // namespace Studio