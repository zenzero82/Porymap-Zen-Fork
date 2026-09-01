#include "studio/artworksourcedialog.h"

#include "studio/porytilesdialog.h"
#include "project.h"
#include "tileset.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QInputDialog>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QPixmap>

namespace Studio {

namespace {
QWidget *row(QLineEdit *edit, QPushButton *button)
{
    auto *widget = new QWidget;
    auto *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(edit, 1);
    layout->addWidget(button);
    return widget;
}
}

ArtworkSourceDialog::ArtworkSourceDialog(Project *project, Tileset *tileset, QWidget *parent)
    : QDialog(parent), project(project), tileset(tileset)
{
    setWindowTitle(QStringLiteral("Generate Porytiles Source from Artwork"));
    resize(700, 500);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    artworkEdit = new QLineEdit;
    auto *artworkButton = new QPushButton(QStringLiteral("Browse..."));
    form->addRow(QStringLiteral("Artwork sheet"), row(artworkEdit, artworkButton));
    outputEdit = new QLineEdit;
    auto *outputButton = new QPushButton(QStringLiteral("Browse..."));
    form->addRow(QStringLiteral("Generated source folder"), row(outputEdit, outputButton));
    if (tileset && tileset->is_secondary) {
        primaryContextEdit = new QLineEdit;
        auto *primaryButton = new QPushButton(QStringLiteral("Browse..."));
        form->addRow(QStringLiteral("Primary Porytiles source"), row(primaryContextEdit, primaryButton));
        connect(primaryButton, &QPushButton::clicked, this, &ArtworkSourceDialog::choosePrimaryContext);
    }
    layout->addLayout(form);
    layout->addWidget(new QLabel(QStringLiteral(
        "Use one PNG with bottom, middle, and top layers stacked vertically. "
        "Each layer must use the same metatile grid.")));
    previewEdit = new QPlainTextEdit;
    previewEdit->setReadOnly(true);
    previewEdit->setPlaceholderText(QStringLiteral("Validation and generated files will be shown here."));
    layout->addWidget(previewEdit, 1);
    previewTabs = new QTabWidget;
    previewTabs->setVisible(false);
    layout->addWidget(previewTabs, 2);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    generateButton = buttons->addButton(QStringLiteral("Generate Preview"), QDialogButtonBox::ActionRole);
    compileButton = buttons->addButton(QStringLiteral("Compile with Porytiles"), QDialogButtonBox::ActionRole);
    compileButton->setEnabled(false);
    layout->addWidget(buttons);
    connect(artworkButton, &QPushButton::clicked, this, &ArtworkSourceDialog::chooseArtwork);
    connect(outputButton, &QPushButton::clicked, this, &ArtworkSourceDialog::chooseOutput);
    connect(generateButton, &QPushButton::clicked, this, &ArtworkSourceDialog::generate);
    connect(compileButton, &QPushButton::clicked, this, &ArtworkSourceDialog::compileGeneratedSource);
    auto invalidate = [this] { generated = {}; compileButton->setEnabled(false); };
    connect(artworkEdit, &QLineEdit::textChanged, this, invalidate);
    connect(outputEdit, &QLineEdit::textChanged, this, invalidate);
    if (primaryContextEdit)
        connect(primaryContextEdit, &QLineEdit::textChanged, this, invalidate);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void ArtworkSourceDialog::chooseArtwork()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose Artwork Sheet"),
                                                      QString(), QStringLiteral("PNG Images (*.png)"));
    if (!path.isEmpty()) artworkEdit->setText(path);
}

void ArtworkSourceDialog::chooseOutput()
{
    const QString parent = QFileDialog::getExistingDirectory(this, QStringLiteral("Choose Parent Folder"));
    if (parent.isEmpty()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, QStringLiteral("Generated Source Folder"),
                                               QStringLiteral("New folder name"), QLineEdit::Normal,
                                               QStringLiteral("porytiles-generated"), &ok).trimmed();
    if (ok && !name.isEmpty() && !name.contains(QLatin1Char('/')) && !name.contains(QLatin1Char('\\'))) {
        outputEdit->setText(QDir(parent).filePath(name));
    }
}

void ArtworkSourceDialog::choosePrimaryContext()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Choose Primary Porytiles Source"), primaryContextEdit->text());
    if (!path.isEmpty()) primaryContextEdit->setText(path);
}

void ArtworkSourceDialog::generate()
{
    ArtworkSourceRequest request{artworkEdit->text().trimmed(), outputEdit->text().trimmed()};
    if (tileset) {
        request.maxUniqueTiles = tileset->maxTiles();
        request.maxColors = tileset->palettePaths.size() * 16;
        request.maxMetatiles = tileset->maxMetatiles();
        request.maxPalettes = tileset->palettePaths.size();
    }
    request.forbiddenRoot = project ? project->root : QString();
    request.secondary = tileset && tileset->is_secondary;
    request.primarySourceDirectory = primaryContextEdit ? primaryContextEdit->text().trimmed() : QString();
    generated = ArtworkSourceGenerator().generate(request);
    if (!generated.success) {
        compileButton->setEnabled(false);
        previewEdit->setPlainText(generated.error);
        QMessageBox::critical(this, QStringLiteral("Artwork Validation Failed"), generated.error);
        return;
    }
    QString text = QStringLiteral("Validated artwork preview\n\nMetatiles: %1\nUnique 8×8 tiles: %2\nColors: %3\n\nGenerated files:\n%4")
                       .arg(generated.metatileCount).arg(generated.uniqueTileCount)
                       .arg(generated.colorCount).arg(generated.generatedFiles.join(QLatin1Char('\n')));
    previewEdit->setPlainText(text);
    previewTabs->clear();
    const QStringList imageNames = {QStringLiteral("bottom.png"), QStringLiteral("middle.png"), QStringLiteral("top.png")};
    for (const QString &name : imageNames) {
        auto *label = new QLabel;
        label->setAlignment(Qt::AlignCenter);
        label->setPixmap(QPixmap(QDir(generated.outputDirectory).filePath(name)));
        auto *scroll = new QScrollArea;
        scroll->setWidget(label);
        scroll->setWidgetResizable(true);
        previewTabs->addTab(scroll, name);
    }
    auto *metadata = new QPlainTextEdit;
    metadata->setReadOnly(true);
    QFile attributes(QDir(generated.outputDirectory).filePath(QStringLiteral("attributes.csv")));
    if (attributes.open(QIODevice::ReadOnly | QIODevice::Text))
        metadata->setPlainText(QString::fromUtf8(attributes.readAll()));
    previewTabs->addTab(metadata, QStringLiteral("attributes.csv"));
    previewTabs->setVisible(true);
    compileButton->setEnabled(true);
}

void ArtworkSourceDialog::compileGeneratedSource()
{
    if (!generated.success) return;
    if (primaryContextEdit && primaryContextEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Primary Context Required"),
                             QStringLiteral("Choose the paired primary Porytiles source before compiling a secondary tileset."));
        return;
    }
    if (QMessageBox::question(this, QStringLiteral("Confirm Generated Source"),
                              QStringLiteral("Compile the reviewed layer images and attributes with Porytiles?"))
        != QMessageBox::Yes) return;
    PorytilesDialog dialog(project, tileset && !tileset->is_secondary ? tileset : nullptr,
                           tileset && tileset->is_secondary ? tileset : nullptr, this);
    dialog.setSourceDirectory(generated.outputDirectory);
    if (primaryContextEdit)
        dialog.setPrimaryContextDirectory(primaryContextEdit->text().trimmed());
    connect(&dialog, &PorytilesDialog::compiledOutputApplied,
            this, &ArtworkSourceDialog::compiledOutputApplied);
    dialog.exec();
}

} // namespace Studio