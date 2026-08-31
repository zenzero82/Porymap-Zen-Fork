#include "studio/porytilesdialog.h"

#include "config.h"
#include "project.h"
#include "tileset.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

namespace Studio {

namespace {

QWidget *pathRow(QLineEdit *edit, QPushButton *button)
{
    auto *widget = new QWidget;
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);
    layout->addWidget(button);
    return widget;
}

QString operationLabel(PorytilesOperation operation)
{
    return operation == PorytilesOperation::Compile ? QStringLiteral("Compile") : QStringLiteral("Decompile");
}

} // namespace

PorytilesDialog::PorytilesDialog(Project *project,
                                 Tileset *primaryTileset,
                                 Tileset *secondaryTileset,
                                 QWidget *parent)
    : QDialog(parent),
      project(project),
      primaryTileset(primaryTileset),
      secondaryTileset(secondaryTileset),
      runner(new PorytilesProcess(this))
{
    setWindowTitle(QStringLiteral("Porytiles Integration"));
    resize(760, 560);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    layout->addLayout(form);

    executableEdit = new QLineEdit(porymapConfig.porytilesExecutable);
    auto *executableButton = new QPushButton(QStringLiteral("Browse..."));
    form->addRow(QStringLiteral("Porytiles executable"), pathRow(executableEdit, executableButton));

    versionEdit = new QLineEdit(porymapConfig.porytilesExpectedVersion);
    versionEdit->setPlaceholderText(QStringLiteral("Exact version, for example 1.0.0"));
    form->addRow(QStringLiteral("Pinned version"), versionEdit);

    timeoutSpin = new QSpinBox;
    timeoutSpin->setRange(1, 3600);
    timeoutSpin->setSuffix(QStringLiteral(" seconds"));
    timeoutSpin->setValue(porymapConfig.porytilesTimeoutSeconds);
    form->addRow(QStringLiteral("Timeout"), timeoutSpin);

    operationCombo = new QComboBox;
    operationCombo->addItem(QStringLiteral("Compile Porytiles source"), static_cast<int>(PorytilesOperation::Compile));
    operationCombo->addItem(QStringLiteral("Decompile Porymap assets"), static_cast<int>(PorytilesOperation::Decompile));
    form->addRow(QStringLiteral("Operation"), operationCombo);

    tilesetCombo = new QComboBox;
    if (primaryTileset) {
        tilesetCombo->addItem(QStringLiteral("Primary — %1").arg(primaryTileset->name), false);
    }
    if (secondaryTileset) {
        tilesetCombo->addItem(QStringLiteral("Secondary — %1").arg(secondaryTileset->name), true);
    }
    form->addRow(QStringLiteral("Tileset"), tilesetCombo);

    sourceLabel = new QLabel(QStringLiteral("Porytiles source"));
    sourceEdit = new QLineEdit;
    auto *sourceButton = new QPushButton(QStringLiteral("Browse..."));
    form->addRow(sourceLabel, pathRow(sourceEdit, sourceButton));

    targetEdit = new QLineEdit;
    auto *targetButton = new QPushButton(QStringLiteral("Browse..."));
    form->addRow(QStringLiteral("Confirmed output location"), pathRow(targetEdit, targetButton));

    primaryContextLabel = new QLabel(QStringLiteral("Primary source context"));
    primaryContextEdit = new QLineEdit;
    auto *primaryContextButton = new QPushButton(QStringLiteral("Browse..."));
    primaryContextRow = pathRow(primaryContextEdit, primaryContextButton);
    form->addRow(primaryContextLabel, primaryContextRow);

    constantsEdit = new QLineEdit(
        QDir(project->root).filePath(projectConfig.getFilePath(ProjectFilePath::constants_metatile_behaviors)));
    auto *constantsButton = new QPushButton(QStringLiteral("Browse..."));
    form->addRow(QStringLiteral("Metatile behaviors"), pathRow(constantsEdit, constantsButton));

    outputEdit = new QPlainTextEdit;
    outputEdit->setReadOnly(true);
    outputEdit->setPlaceholderText(QStringLiteral("Porytiles stdout and stderr will appear here."));
    layout->addWidget(outputEdit, 1);

    statusLabel = new QLabel(QStringLiteral("Configure an exact Porytiles version, then validate and run."));
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    runButton = buttonBox->addButton(QStringLiteral("Validate && Run"), QDialogButtonBox::ActionRole);
    cancelButton = buttonBox->addButton(QStringLiteral("Cancel Run"), QDialogButtonBox::ActionRole);
    cancelButton->setEnabled(false);
    layout->addWidget(buttonBox);

    connect(executableButton, &QPushButton::clicked, this, &PorytilesDialog::browseExecutable);
    connect(sourceButton, &QPushButton::clicked, this, &PorytilesDialog::browseSource);
    connect(targetButton, &QPushButton::clicked, this, &PorytilesDialog::browseTarget);
    connect(constantsButton, &QPushButton::clicked, this, &PorytilesDialog::browseConstants);
    connect(primaryContextButton, &QPushButton::clicked, this, &PorytilesDialog::browsePrimaryContext);
    connect(operationCombo, &QComboBox::currentIndexChanged, this, &PorytilesDialog::updateOperationUi);
    connect(tilesetCombo, &QComboBox::currentIndexChanged, this, &PorytilesDialog::updateOperationUi);
    connect(runButton, &QPushButton::clicked, this, &PorytilesDialog::startRun);
    connect(cancelButton, &QPushButton::clicked, this, &PorytilesDialog::cancelRun);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(runner, &PorytilesProcess::outputReceived, this, &PorytilesDialog::appendOutput);
    connect(runner, &PorytilesProcess::finished, this, &PorytilesDialog::runFinished);
    updateOperationUi();
}

Tileset *PorytilesDialog::selectedTileset() const
{
    return tilesetCombo->currentData().toBool() ? secondaryTileset : primaryTileset;
}

QString PorytilesDialog::compiledDirectory(const Tileset *tileset) const
{
    return tileset ? QFileInfo(tileset->tilesImagePath).absolutePath() : QString();
}

void PorytilesDialog::updateOperationUi()
{
    const bool compile = operationCombo->currentData().toInt() == static_cast<int>(PorytilesOperation::Compile);
    const bool secondary = tilesetCombo->currentData().toBool();
    Tileset *tileset = selectedTileset();
    sourceLabel->setText(compile ? QStringLiteral("Porytiles source") : QStringLiteral("Compiled Porymap assets"));
    primaryContextLabel->setText(compile ? QStringLiteral("Primary source context")
                                        : QStringLiteral("Primary compiled context"));
    primaryContextLabel->setVisible(secondary);
    primaryContextRow->setVisible(secondary);

    if (!compile) {
        sourceEdit->setText(compiledDirectory(tileset));
    }
    if (compile) {
        targetEdit->setText(compiledDirectory(tileset));
    }
    targetEdit->setReadOnly(compile);
    if (secondary && !compile) {
        primaryContextEdit->setText(compiledDirectory(primaryTileset));
    }
}

void PorytilesDialog::browseExecutable()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose Porytiles Executable"),
                                                      executableEdit->text());
    if (!path.isEmpty()) executableEdit->setText(path);
}

void PorytilesDialog::browseSource()
{
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose Porytiles Source"),
                                                           sourceEdit->text());
    if (!path.isEmpty()) sourceEdit->setText(path);
}

void PorytilesDialog::browseTarget()
{
    if (operationCombo->currentData().toInt() == static_cast<int>(PorytilesOperation::Compile)) {
        return;
    }
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose Confirmed Output Location"),
                                                           QFileInfo(targetEdit->text()).absolutePath());
    if (path.isEmpty()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("New Porytiles Source Directory"),
                                               QStringLiteral("Directory name"), QLineEdit::Normal,
                                               QStringLiteral("porytiles-source"), &ok).trimmed();
    if (ok && !name.isEmpty() && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))) {
        targetEdit->setText(QDir(path).filePath(name));
    }
}

void PorytilesDialog::browseConstants()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose Metatile Behaviors Header"),
                                                      constantsEdit->text(),
                                                      QStringLiteral("Header Files (*.h);;All Files (*)"));
    if (!path.isEmpty()) constantsEdit->setText(path);
}

void PorytilesDialog::browsePrimaryContext()
{
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose Primary Tileset Context"),
                                                           primaryContextEdit->text());
    if (!path.isEmpty()) primaryContextEdit->setText(path);
}

PorytilesRequest PorytilesDialog::requestFromUi() const
{
    PorytilesRequest request;
    request.executable = executableEdit->text().trimmed();
    request.expectedVersion = versionEdit->text().trimmed();
    request.timeoutMs = timeoutSpin->value() * 1000;
    request.operation = static_cast<PorytilesOperation>(operationCombo->currentData().toInt());
    request.tilesetKind = tilesetCombo->currentData().toBool()
        ? PorytilesTilesetKind::Secondary : PorytilesTilesetKind::Primary;
    request.sourceDirectory = QDir::cleanPath(sourceEdit->text().trimmed());
    request.targetDirectory = QDir::cleanPath(targetEdit->text().trimmed());
    request.constantsFile = QDir::cleanPath(constantsEdit->text().trimmed());
    if (Tileset *tileset = selectedTileset()) {
        request.expectedPaletteCount = tileset->palettePaths.size();
        for (const QString &palettePath : tileset->palettePaths) {
            request.expectedPaletteFiles.append(QFileInfo(palettePath).fileName());
        }
        request.expectedPaletteFiles.sort();
    }
    if (request.tilesetKind == PorytilesTilesetKind::Secondary) {
        if (request.operation == PorytilesOperation::Compile) {
            request.primarySourceDirectory = QDir::cleanPath(primaryContextEdit->text().trimmed());
        } else {
            request.primaryCompiledDirectory = QDir::cleanPath(primaryContextEdit->text().trimmed());
        }
    }
    return request;
}

bool PorytilesDialog::fieldsComplete() const
{
    const PorytilesRequest request = requestFromUi();
    return !request.executable.isEmpty()
        && !request.expectedVersion.isEmpty()
        && !request.sourceDirectory.isEmpty()
        && !request.targetDirectory.isEmpty()
        && !request.constantsFile.isEmpty()
        && (request.tilesetKind == PorytilesTilesetKind::Primary
            || !primaryContextEdit->text().trimmed().isEmpty());
}

void PorytilesDialog::setRunning(bool running)
{
    const QList<QWidget *> controls = {
        executableEdit, versionEdit, timeoutSpin, operationCombo, tilesetCombo,
        sourceEdit, targetEdit, primaryContextEdit, constantsEdit
    };
    for (QWidget *widget : controls) {
        widget->setEnabled(!running);
    }
    cancelButton->setEnabled(running);
    runButton->setEnabled(!running);
    buttonBox->button(QDialogButtonBox::Close)->setEnabled(!running);
}

void PorytilesDialog::startRun()
{
    if (!fieldsComplete()) {
        QMessageBox::warning(this, QStringLiteral("Porytiles"),
                             QStringLiteral("Complete every required path and configure an exact Porytiles version."));
        return;
    }

    activeRequest = requestFromUi();
    if (activeRequest.operation == PorytilesOperation::Compile) {
        Tileset *tileset = selectedTileset();
        const QString root = compiledDirectory(tileset);
        bool coherent = tileset
            && QFileInfo(tileset->metatiles_path).absolutePath() == root
            && QFileInfo(tileset->metatile_attrs_path).absolutePath() == root;
        for (const QString &palettePath : tileset ? tileset->palettePaths : QStringList()) {
            coherent = coherent
                && QFileInfo(palettePath).absolutePath() == QDir(root).filePath(QStringLiteral("palettes"));
        }
        if (!coherent || activeRequest.targetDirectory != root) {
            QMessageBox::critical(
                this, QStringLiteral("Unsupported Tileset Layout"),
                QStringLiteral("This tileset stores compiled assets in multiple or custom directories. "
                               "Porytiles cannot apply output safely until all resolved assets share the "
                               "standard tileset directory layout."));
            return;
        }
    } else {
        const QString target = QFileInfo(activeRequest.targetDirectory).absoluteFilePath();
        const QString primaryCompiled = QFileInfo(compiledDirectory(primaryTileset)).absoluteFilePath();
        const QString secondaryCompiled = QFileInfo(compiledDirectory(secondaryTileset)).absoluteFilePath();
        if (target == primaryCompiled || target.startsWith(primaryCompiled + QDir::separator())
            || target == secondaryCompiled || target.startsWith(secondaryCompiled + QDir::separator())) {
            QMessageBox::critical(this, QStringLiteral("Unsafe Output Location"),
                                  QStringLiteral("Choose a new source directory outside the compiled project tilesets."));
            return;
        }
    }
    porymapConfig.porytilesExecutable = activeRequest.executable;
    porymapConfig.porytilesExpectedVersion = activeRequest.expectedVersion;
    porymapConfig.porytilesTimeoutSeconds = timeoutSpin->value();
    porymapConfig.save();

    outputEdit->clear();
    statusLabel->setText(QStringLiteral("Validating and staging Porytiles inputs..."));
    QString error;
    if (!runner->start(activeRequest, &error)) {
        statusLabel->setText(error);
        QMessageBox::critical(this, QStringLiteral("Porytiles Validation Failed"), error);
        return;
    }
    setRunning(true);
    statusLabel->setText(QStringLiteral("%1 running in an isolated staging directory...")
                             .arg(operationLabel(activeRequest.operation)));
}

void PorytilesDialog::cancelRun()
{
    runner->cancel();
    statusLabel->setText(QStringLiteral("Canceling Porytiles..."));
}

void PorytilesDialog::appendOutput(const QString &text, bool standardError)
{
    if (text.isEmpty()) return;
    outputEdit->appendPlainText((standardError ? QStringLiteral("[stderr] ") : QString()) + text.trimmed());
}

void PorytilesDialog::runFinished(const PorytilesResult &result)
{
    setRunning(false);
    if (!result.success) {
        statusLabel->setText(QStringLiteral("Porytiles did not produce an applicable preview: %1").arg(result.error));
        QMessageBox::critical(this, QStringLiteral("Porytiles Failed"), result.error);
        return;
    }

    const QString summary = QStringLiteral(
        "Porytiles %1 completed in %2 ms.\n\n"
        "Validated staged files:\n%3\n\n"
        "Nothing in the project has changed yet. Apply this preview to:\n%4")
        .arg(result.version)
        .arg(result.elapsedMs)
        .arg(result.outputFiles.join(QLatin1Char('\n')))
        .arg(result.targetDirectory);

    QMessageBox preview(QMessageBox::Question,
                        QStringLiteral("Apply Porytiles Preview?"),
                        summary,
                        QMessageBox::Yes | QMessageBox::No,
                        this);
    preview.setDefaultButton(QMessageBox::No);
    preview.setDetailedText(QStringLiteral("stdout:\n%1\n\nstderr:\n%2")
                                .arg(result.standardOutput, result.standardError));
    if (preview.exec() != QMessageBox::Yes) {
        statusLabel->setText(QStringLiteral("Preview rejected. Project files were not changed."));
        return;
    }

    QString error;
    if (!PorytilesProcess::applyStagedOutput(activeRequest, result.stagedOutputDirectory,
                                             result.targetDirectory, &error)) {
        statusLabel->setText(error);
        QMessageBox::critical(this, QStringLiteral("Could Not Apply Porytiles Output"), error);
        return;
    }

    statusLabel->setText(QStringLiteral("Validated Porytiles output applied successfully."));
    if (activeRequest.operation == PorytilesOperation::Compile) {
        emit compiledOutputApplied();
    }
    QMessageBox::information(this, QStringLiteral("Porytiles"),
                             QStringLiteral("The confirmed Porytiles output was applied successfully."));
}

} // namespace Studio