#include "studio/newmapfromimagedialog.h"

#include "metatile.h"
#include "project.h"
#include "tileset.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <utility>

namespace Studio {

NewMapFromImageDialog::NewMapFromImageDialog(
    Project *project,
    MetatileRenderService::RenderContext renderContext,
    QWidget *parent)
    : QDialog(parent),
      m_project(project),
      m_renderContext(std::move(renderContext))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("New Map From Image"));
    setMinimumSize(860, 650);

    auto *mainLayout = new QVBoxLayout(this);

    auto *sourceGroup = new QGroupBox(QStringLiteral("Source Image"), this);
    auto *sourceLayout = new QGridLayout(sourceGroup);
    m_imagePath = new QLineEdit(sourceGroup);
    m_imagePath->setReadOnly(true);
    auto *browseButton = new QPushButton(QStringLiteral("Browse..."), sourceGroup);
    m_imageDimensions = new QLabel(QStringLiteral("No image selected."), sourceGroup);
    sourceLayout->addWidget(new QLabel(QStringLiteral("PNG File:"), sourceGroup), 0, 0);
    sourceLayout->addWidget(m_imagePath, 0, 1);
    sourceLayout->addWidget(browseButton, 0, 2);
    sourceLayout->addWidget(new QLabel(QStringLiteral("Dimensions:"), sourceGroup), 1, 0);
    sourceLayout->addWidget(m_imageDimensions, 1, 1, 1, 2);
    mainLayout->addWidget(sourceGroup);

    auto *configurationGroup = new QGroupBox(QStringLiteral("Map Configuration"), this);
    auto *configurationLayout = new QFormLayout(configurationGroup);
    m_mapName = new QLineEdit(configurationGroup);
    m_primaryTileset = new QComboBox(configurationGroup);
    m_secondaryTileset = new QComboBox(configurationGroup);
    m_autoDimensions = new QCheckBox(QStringLiteral("Automatically detect dimensions"), configurationGroup);
    m_autoDimensions->setChecked(true);
    m_mapWidth = new QSpinBox(configurationGroup);
    m_mapHeight = new QSpinBox(configurationGroup);
    const int maxWidth = m_project ? qMax(1, m_project->getMaxMapWidth()) : 9999;
    const int maxHeight = m_project ? qMax(1, m_project->getMaxMapHeight()) : 9999;
    m_mapWidth->setRange(1, maxWidth);
    m_mapHeight->setRange(1, maxHeight);
    m_mapWidth->setSuffix(QStringLiteral(" blocks"));
    m_mapHeight->setSuffix(QStringLiteral(" blocks"));
    configurationLayout->addRow(QStringLiteral("Map Name:"), m_mapName);
    configurationLayout->addRow(QStringLiteral("Primary Tileset:"), m_primaryTileset);
    configurationLayout->addRow(QStringLiteral("Secondary Tileset:"), m_secondaryTileset);
    configurationLayout->addRow(QString(), m_autoDimensions);
    configurationLayout->addRow(QStringLiteral("Map Width:"), m_mapWidth);
    configurationLayout->addRow(QStringLiteral("Map Height:"), m_mapHeight);
    mainLayout->addWidget(configurationGroup);

    if (m_project) {
        m_primaryTileset->addItems(m_project->primaryTilesetLabels);
        m_secondaryTileset->addItems(m_project->secondaryTilesetLabels);
        m_primaryTileset->setCurrentText(m_project->getDefaultPrimaryTilesetLabel());
        m_secondaryTileset->setCurrentText(m_project->getDefaultSecondaryTilesetLabel());
    }
    updateDimensionControls();

    auto *previewAndSummary = new QHBoxLayout();
    auto *previewGroup = new QGroupBox(QStringLiteral("Image Preview"), this);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    m_previewTabs = new QTabWidget(previewGroup);
    const auto createPreview = [this](QScrollArea *&scrollArea, QLabel *&label, const QString &placeholder) {
        scrollArea = new QScrollArea(m_previewTabs);
        scrollArea->setWidgetResizable(false);
        label = new QLabel(scrollArea);
        label->setAlignment(Qt::AlignCenter);
        label->setText(placeholder);
        scrollArea->setWidget(label);
        scrollArea->viewport()->installEventFilter(this);
        return scrollArea;
    };
    m_previewTabs->addTab(
        createPreview(m_previewScrollArea, m_previewLabel, QStringLiteral("Choose a PNG image to preview it.")),
        QStringLiteral("Original")
    );
    m_previewTabs->addTab(
        createPreview(
            m_reconstructedPreviewScrollArea,
            m_reconstructedPreviewLabel,
            QStringLiteral("Run analysis to preview the reconstruction.")
        ),
        QStringLiteral("Reconstructed")
    );
    m_previewTabs->addTab(
        createPreview(
            m_differencePreviewScrollArea,
            m_differencePreviewLabel,
            QStringLiteral("Run analysis to preview unmatched cells.")
        ),
        QStringLiteral("Differences")
    );
    previewLayout->addWidget(m_previewTabs);
    previewAndSummary->addWidget(previewGroup, 3);

    auto *analysisGroup = new QGroupBox(QStringLiteral("Image Analysis"), this);
    auto *analysisLayout = new QVBoxLayout(analysisGroup);
    m_analysisSummary = new QPlainTextEdit(analysisGroup);
    m_analysisSummary->setReadOnly(true);
    m_analysisSummary->setPlainText(QStringLiteral("Choose a PNG image, select both tilesets, then run analysis."));
    analysisLayout->addWidget(m_analysisSummary);
    previewAndSummary->addWidget(analysisGroup, 2);
    mainLayout->addLayout(previewAndSummary, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_analyzeButton = buttonBox->addButton(QStringLiteral("Run Analysis"), QDialogButtonBox::ActionRole);
    m_reviewUnmatchedButton = buttonBox->addButton(QStringLiteral("Review Unmatched"), QDialogButtonBox::ActionRole);
    m_reviewUnmatchedButton->setEnabled(false);
#ifndef QT_NO_DEBUG
    m_debugExportButton = buttonBox->addButton(QStringLiteral("Export Debug Metatiles..."), QDialogButtonBox::ActionRole);
    m_debugExportButton->setEnabled(false);
#endif
    mainLayout->addWidget(buttonBox);

    connect(browseButton, &QPushButton::clicked, this, &NewMapFromImageDialog::browseForImage);
    connect(m_analyzeButton, &QPushButton::clicked, this, &NewMapFromImageDialog::runAnalysis);
    connect(m_reviewUnmatchedButton, &QPushButton::clicked, this, &NewMapFromImageDialog::reviewUnmatched);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_autoDimensions, &QCheckBox::toggled, this, [this] {
        updateDimensionControls();
        resetAnalysis(QStringLiteral("Map dimensions changed. Run analysis again."));
    });
    connect(m_mapWidth, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
        if (!m_autoDimensions->isChecked()) {
            resetAnalysis(QStringLiteral("Map dimensions changed. Run analysis again."));
        }
    });
    connect(m_mapHeight, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
        if (!m_autoDimensions->isChecked()) {
            resetAnalysis(QStringLiteral("Map dimensions changed. Run analysis again."));
        }
    });
    connect(m_primaryTileset, &QComboBox::currentTextChanged, this, [this] {
        resetAnalysis(QStringLiteral("Primary tileset changed. Run analysis again."));
    });
    connect(m_secondaryTileset, &QComboBox::currentTextChanged, this, [this] {
        resetAnalysis(QStringLiteral("Secondary tileset changed. Run analysis again."));
    });
#ifndef QT_NO_DEBUG
    connect(m_debugExportButton, &QPushButton::clicked, this, &NewMapFromImageDialog::exportDebugMetatiles);
#endif
}

bool NewMapFromImageDialog::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_previewScrollArea->viewport()
            || watched == m_reconstructedPreviewScrollArea->viewport()
            || watched == m_differencePreviewScrollArea->viewport())
        && event->type() == QEvent::Resize) {
        updatePreview();
    }
    return QDialog::eventFilter(watched, event);
}

void NewMapFromImageDialog::browseForImage()
{
    const QString filepath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Choose Map Image"),
        m_project ? m_project->root : QString(),
        QStringLiteral("PNG Images (*.png)")
    );
    if (!filepath.isEmpty()) {
        loadImage(filepath);
    }
}

bool NewMapFromImageDialog::loadImage(const QString &filepath)
{
    m_imageResult = m_imageAnalyzer.analyzePng(filepath);
    m_imagePath->setText(filepath);
    m_renderResult = {};
#ifndef QT_NO_DEBUG
    m_debugExportButton->setEnabled(false);
#endif

    if (!m_imageResult.loaded) {
        m_imageDimensions->setText(QStringLiteral("Unable to load image."));
        m_previewLabel->clear();
        m_previewLabel->setText(m_imageResult.errorMessage);
        resetAnalysis(m_imageResult.errorMessage);
        return false;
    }

    m_imageDimensions->setText(QString("%1 × %2 px")
        .arg(m_imageResult.imageSize.width())
        .arg(m_imageResult.imageSize.height()));
    updatePreview();

    if (m_autoDimensions->isChecked() && m_imageResult.gridAligned) {
        m_mapWidth->setValue(m_imageResult.mapSize.width());
        m_mapHeight->setValue(m_imageResult.mapSize.height());
    }

    if (!m_imageResult.gridAligned) {
        resetAnalysis(m_imageResult.alignmentMessage);
        return false;
    }

    resetAnalysis(QStringLiteral("Image loaded. Select tilesets and run analysis."));
    return true;
}

void NewMapFromImageDialog::updatePreview()
{
    updatePreviewForImage(m_imageResult.sourceImage, m_previewScrollArea, m_previewLabel);
    updatePreviewForImage(
        m_matchResult.reconstructedImage,
        m_reconstructedPreviewScrollArea,
        m_reconstructedPreviewLabel
    );
    updatePreviewForImage(
        m_matchResult.differenceImage,
        m_differencePreviewScrollArea,
        m_differencePreviewLabel
    );
}

void NewMapFromImageDialog::updatePreviewForImage(
    const QImage &image,
    QScrollArea *scrollArea,
    QLabel *label)
{
    if (image.isNull()) {
        return;
    }

    QSize targetSize = scrollArea->viewport()->size();
    targetSize = targetSize.boundedTo(image.size());
    if (targetSize.isEmpty()) {
        return;
    }

    const QPixmap preview = QPixmap::fromImage(image.scaled(
        targetSize,
        Qt::KeepAspectRatio,
        Qt::FastTransformation
    ));
    label->setPixmap(preview);
    label->resize(preview.size());
}

void NewMapFromImageDialog::resetAnalysis(const QString &message)
{
    if (!message.isEmpty()) {
        m_analysisSummary->setPlainText(QString("STATUS\n%1").arg(message));
    }
    m_renderResult = {};
    m_matchResult = {};
    m_reconstructedPreviewLabel->clear();
    m_reconstructedPreviewLabel->setText(QStringLiteral("Run analysis to preview the reconstruction."));
    m_differencePreviewLabel->clear();
    m_differencePreviewLabel->setText(QStringLiteral("Run analysis to preview unmatched cells."));
    m_reviewUnmatchedButton->setEnabled(false);
#ifndef QT_NO_DEBUG
    m_debugExportButton->setEnabled(false);
#endif
}

void NewMapFromImageDialog::runAnalysis()
{
    if (!m_project) {
        resetAnalysis(QStringLiteral("No project is loaded."));
        return;
    }
    if (!loadImage(m_imagePath->text())) {
        return;
    }
    if (!m_imageResult.gridAligned) {
        resetAnalysis(m_imageResult.alignmentMessage);
        return;
    }
    if (!m_autoDimensions->isChecked()
        && QSize(m_mapWidth->value(), m_mapHeight->value()) != m_imageResult.mapSize) {
        resetAnalysis(QString(
            "Manual dimensions must match the unchanged image: %1 × %2 blocks."
        ).arg(m_imageResult.mapSize.width())
         .arg(m_imageResult.mapSize.height()));
        return;
    }
    if (m_primaryTileset->currentText().isEmpty() || m_secondaryTileset->currentText().isEmpty()) {
        resetAnalysis(QStringLiteral("Select both a primary and secondary tileset."));
        return;
    }

    Tileset *primaryTileset = m_project->getTileset(m_primaryTileset->currentText());
    Tileset *secondaryTileset = m_project->getTileset(m_secondaryTileset->currentText());
    if (!primaryTileset || !secondaryTileset) {
        resetAnalysis(QStringLiteral("One or both selected tilesets could not be loaded."));
        return;
    }

    m_renderResult = m_renderService.renderAll(primaryTileset, secondaryTileset, m_renderContext);
    if (!m_renderResult.isValid()) {
        QString message = m_renderResult.errorMessage;
        if (!m_renderResult.warnings.isEmpty()) {
            message.append(QString("\n\n%1").arg(m_renderResult.warnings.join('\n')));
        }
        resetAnalysis(message);
        return;
    }

    const QSize mapSize = m_autoDimensions->isChecked()
        ? m_imageResult.mapSize
        : QSize(m_mapWidth->value(), m_mapHeight->value());
    if (!m_project->mapDimensionsValid(mapSize.width(), mapSize.height())) {
        resetAnalysis(QString(
            "Porymap rejects map dimensions %1 × %2 blocks for this project."
        ).arg(mapSize.width())
         .arg(mapSize.height()));
        return;
    }
    m_matchResult = m_matcher.match(
        m_imageResult.sourceImage,
        mapSize,
        m_imageResult.metatilePixelSize,
        m_renderResult.metatiles
    );
    if (!m_matchResult.isValid()) {
        resetAnalysis(m_matchResult.errorMessage);
        return;
    }

    const int cellCount = m_matchResult.cells.count();
    const double matchPercentage = cellCount == 0
        ? 0.0
        : (100.0 * m_matchResult.exactMatchCount) / cellCount;
    const QString summary = QString(
        "RECONSTRUCTION RESULTS\n\n"
        "Map:\n"
        "%1 × %2 blocks\n\n"
        "Cells Analyzed:\n"
        "%3\n\n"
        "Exact Matches:\n"
        "%4  %5%\n\n"
        "Unmatched:\n"
        "%6\n\n"
        "Primary Tileset:\n"
        "%7 matches\n\n"
        "Secondary Tileset:\n"
        "%8 matches\n\n"
        "Rendering Context:\n"
        "%9\n\n"
        "Status:\n"
        "%10"
    ).arg(mapSize.width())
     .arg(mapSize.height())
     .arg(cellCount)
     .arg(m_matchResult.exactMatchCount)
     .arg(QString::number(matchPercentage, 'f', 1))
     .arg(m_matchResult.unmatchedCount)
     .arg(m_matchResult.primaryMatchCount)
     .arg(m_matchResult.secondaryMatchCount)
     .arg(m_renderContext.description)
     .arg(m_matchResult.unmatchedCount == 0
        ? QStringLiteral("EXACT RECONSTRUCTION READY")
        : QString("%1 cells require attention").arg(m_matchResult.unmatchedCount));
    m_analysisSummary->setPlainText(summary);
    updatePreview();
    m_reviewUnmatchedButton->setEnabled(m_matchResult.unmatchedCount > 0);
#ifndef QT_NO_DEBUG
    m_debugExportButton->setEnabled(!m_renderResult.metatiles.isEmpty());
#endif
}

void NewMapFromImageDialog::reviewUnmatched()
{
    if (m_matchResult.unmatchedCount == 0) {
        return;
    }

    QDialog reviewDialog(this);
    reviewDialog.setWindowTitle(QStringLiteral("Review Unmatched Cells"));
    reviewDialog.setMinimumSize(520, 360);
    auto *layout = new QHBoxLayout(&reviewDialog);
    auto *cellList = new QListWidget(&reviewDialog);
    auto *previewLabel = new QLabel(&reviewDialog);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setMinimumSize(180, 180);

    QList<const ImageMetatileMatcher::CellResult *> unmatchedCells;
    for (const auto &cell : m_matchResult.cells) {
        if (!cell.matched) {
            unmatchedCells.append(&cell);
            cellList->addItem(QString("Column %1, Row %2")
                .arg(cell.position.x())
                .arg(cell.position.y()));
        }
    }

    layout->addWidget(cellList, 2);
    layout->addWidget(previewLabel, 1);
    connect(cellList, &QListWidget::currentRowChanged, &reviewDialog, [previewLabel, unmatchedCells](int row) {
        if (row < 0 || row >= unmatchedCells.count()) {
            previewLabel->clear();
            return;
        }
        previewLabel->setPixmap(QPixmap::fromImage(unmatchedCells.at(row)->sourceImage.scaled(
            160,
            160,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        )));
    });

    cellList->setCurrentRow(0);
    reviewDialog.exec();
}

void NewMapFromImageDialog::exportDebugMetatiles()
{
#ifndef QT_NO_DEBUG
    if (m_renderResult.metatiles.isEmpty()) {
        return;
    }

    const QString directory = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Choose Debug Metatile Export Folder"),
        m_project ? QDir(m_project->root).filePath(QStringLiteral("debug_metatiles")) : QString()
    );
    if (directory.isEmpty()) {
        return;
    }

    QString error;
    if (!m_renderService.exportPngs(m_renderResult, directory, &error)) {
        resetAnalysis(error);
        return;
    }
    m_analysisSummary->appendPlainText(QString("\n\nDebug metatiles exported to:\n%1").arg(directory));
#endif
}

void NewMapFromImageDialog::updateDimensionControls()
{
    const bool automatic = m_autoDimensions->isChecked();
    m_mapWidth->setEnabled(!automatic);
    m_mapHeight->setEnabled(!automatic);
}

} // namespace Studio