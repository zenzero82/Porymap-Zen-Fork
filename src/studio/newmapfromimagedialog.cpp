#include "studio/newmapfromimagedialog.h"

#include "map.h"
#include "maplayout.h"
#include "metatile.h"
#include "project.h"
#include "tileset.h"

#include <QAbstractItemView>
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
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPushButton>
#include <QProgressDialog>
#include <QSignalBlocker>
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
    m_mapGroup = new QComboBox(configurationGroup);
    m_mapGroup->setEditable(true);
    m_primaryTileset = new QComboBox(configurationGroup);
    m_secondaryTileset = new QComboBox(configurationGroup);
    m_autoDimensions = new QCheckBox(QStringLiteral("Automatically detect dimensions"), configurationGroup);
    m_autoDimensions->setChecked(true);
    m_mapWidth = new QSpinBox(configurationGroup);
    m_mapHeight = new QSpinBox(configurationGroup);
    m_maxFuzzyDistance = new QSpinBox(configurationGroup);
    m_minFuzzyConfidence = new QSpinBox(configurationGroup);
    const int maxWidth = m_project ? qMax(1, m_project->getMaxMapWidth()) : 9999;
    const int maxHeight = m_project ? qMax(1, m_project->getMaxMapHeight()) : 9999;
    m_mapWidth->setRange(1, maxWidth);
    m_mapHeight->setRange(1, maxHeight);
    m_mapWidth->setSuffix(QStringLiteral(" blocks"));
    m_mapHeight->setSuffix(QStringLiteral(" blocks"));
    m_maxFuzzyDistance->setRange(0, 100);
    m_maxFuzzyDistance->setValue(15);
    m_maxFuzzyDistance->setSuffix(QStringLiteral("%"));
    m_maxFuzzyDistance->setToolTip(QStringLiteral(
        "Maximum normalized average RGBA channel difference for a fuzzy suggestion."
    ));
    m_minFuzzyConfidence->setRange(0, 100);
    m_minFuzzyConfidence->setValue(50);
    m_minFuzzyConfidence->setSuffix(QStringLiteral("%"));
    m_minFuzzyConfidence->setToolTip(QStringLiteral(
        "Minimum confidence required to classify the best candidate as accepted."
    ));
    configurationLayout->addRow(QStringLiteral("Map Name:"), m_mapName);
    configurationLayout->addRow(QStringLiteral("Map Group:"), m_mapGroup);
    configurationLayout->addRow(QStringLiteral("Primary Tileset:"), m_primaryTileset);
    configurationLayout->addRow(QStringLiteral("Secondary Tileset:"), m_secondaryTileset);
    configurationLayout->addRow(QString(), m_autoDimensions);
    configurationLayout->addRow(QStringLiteral("Map Width:"), m_mapWidth);
    configurationLayout->addRow(QStringLiteral("Map Height:"), m_mapHeight);
    configurationLayout->addRow(QStringLiteral("Max Fuzzy Distance:"), m_maxFuzzyDistance);
    configurationLayout->addRow(QStringLiteral("Minimum Confidence:"), m_minFuzzyConfidence);
    mainLayout->addWidget(configurationGroup);

    if (m_project) {
        m_mapGroup->addItems(m_project->groupNames);
        m_mapGroup->setCurrentText(m_project->newMapSettings.group);
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
    m_fuzzyMatchButton = buttonBox->addButton(
        QStringLiteral("Run Fuzzy Matching"),
        QDialogButtonBox::ActionRole
    );
    m_fuzzyMatchButton->setEnabled(false);
    m_createMapButton = buttonBox->addButton(QStringLiteral("Create Map"), QDialogButtonBox::AcceptRole);
    m_createMapButton->setEnabled(false);
    m_reviewUnmatchedButton = buttonBox->addButton(QStringLiteral("Review Corrections"), QDialogButtonBox::ActionRole);
    m_reviewUnmatchedButton->setEnabled(false);
#ifndef QT_NO_DEBUG
    m_debugExportButton = buttonBox->addButton(QStringLiteral("Export Debug Metatiles..."), QDialogButtonBox::ActionRole);
    m_debugExportButton->setEnabled(false);
#endif
    mainLayout->addWidget(buttonBox);

    connect(browseButton, &QPushButton::clicked, this, &NewMapFromImageDialog::browseForImage);
    connect(m_analyzeButton, &QPushButton::clicked, this, &NewMapFromImageDialog::runAnalysis);
    connect(m_fuzzyMatchButton, &QPushButton::clicked, this, &NewMapFromImageDialog::runFuzzyMatching);
    connect(m_createMapButton, &QPushButton::clicked, this, &NewMapFromImageDialog::createMapFromMatch);
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
    connect(m_maxFuzzyDistance, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
        resetAnalysis(QStringLiteral("Fuzzy thresholds changed. Run analysis again."));
    });
    connect(m_minFuzzyConfidence, qOverload<int>(&QSpinBox::valueChanged), this, [this] {
        resetAnalysis(QStringLiteral("Fuzzy thresholds changed. Run analysis again."));
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
    m_analysisReconstructedImage = {};
    m_analysisDifferenceImage = {};
    m_corrections.clear();
    m_reconstructedPreviewLabel->clear();
    m_reconstructedPreviewLabel->setText(QStringLiteral("Run analysis to preview the reconstruction."));
    m_differencePreviewLabel->clear();
    m_differencePreviewLabel->setText(QStringLiteral("Run analysis to preview unmatched cells."));
    m_reviewUnmatchedButton->setEnabled(false);
    m_fuzzyMatchButton->setEnabled(false);
    updateCreateButton();
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
    ImageMetatileMatcher::MatchOptions exactOptions;
    m_matchResult = m_matcher.match(
        m_imageResult.sourceImage,
        mapSize,
        m_imageResult.metatilePixelSize,
        m_renderResult.metatiles,
        exactOptions
    );
    if (!m_matchResult.isValid()) {
        resetAnalysis(m_matchResult.errorMessage);
        return;
    }

    m_analysisReconstructedImage = m_matchResult.reconstructedImage;
    m_analysisDifferenceImage = m_matchResult.differenceImage;
    updateMatchDisplay();
}

void NewMapFromImageDialog::runFuzzyMatching()
{
    // Re-run exact analysis first so fuzzy suggestions can never use stale
    // source pixels, tilesets, dimensions, or rendered candidates.
    runAnalysis();
    if (!m_matchResult.isValid() || m_matchResult.cells.isEmpty() || m_matchResult.unmatchedCount == 0) {
        return;
    }

    ImageMetatileMatcher::MatchOptions fuzzyOptions;
    fuzzyOptions.allowFuzzy = true;
    fuzzyOptions.maximumDistance = m_maxFuzzyDistance->value() / 100.0;
    fuzzyOptions.minimumConfidence = m_minFuzzyConfidence->value() / 100.0;
    fuzzyOptions.maximumRankedCandidates = 5;
    QProgressDialog progress(
        QStringLiteral("Comparing source cells with rendered metatiles…"),
        QStringLiteral("Cancel"),
        0,
        m_matchResult.mapSize.width() * m_matchResult.mapSize.height(),
        this
    );
    progress.setWindowTitle(QStringLiteral("Fuzzy Matching"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    fuzzyOptions.progressCallback = [&progress](int completedCells, int totalCells) {
        progress.setMaximum(totalCells);
        progress.setValue(completedCells);
        QApplication::processEvents();
        return !progress.wasCanceled();
    };
    m_matchResult = m_matcher.match(
        m_imageResult.sourceImage,
        m_matchResult.mapSize,
        m_imageResult.metatilePixelSize,
        m_renderResult.metatiles,
        fuzzyOptions
    );
    if (!m_matchResult.isValid()) {
        resetAnalysis(m_matchResult.errorMessage);
        return;
    }
    progress.setValue(progress.maximum());

    m_analysisReconstructedImage = m_matchResult.reconstructedImage;
    m_analysisDifferenceImage = m_matchResult.differenceImage;
    updateMatchDisplay();
}

void NewMapFromImageDialog::updateMatchDisplay()
{
    updateCorrectionPreviews();
    const int cellCount = m_matchResult.cells.count();
    int approvedCorrections = 0;
    int editedCorrections = 0;
    for (const auto &cell : m_matchResult.cells) {
        if (cell.matched) {
            continue;
        }
        const auto correctionIt = m_corrections.constFind(cellIndex(cell));
        if (correctionIt == m_corrections.cend()) {
            continue;
        }
        if (correctionIt.value().approved && isCorrectionValid(cell, correctionIt.value())) {
            approvedCorrections++;
        } else {
            editedCorrections++;
        }
    }
    const double matchPercentage = cellCount == 0
        ? 0.0
        : (100.0 * m_matchResult.exactMatchCount) / cellCount;
    QString fuzzySummary;
    QString status;
    if (allCellsResolved() && m_matchResult.unmatchedCount > 0) {
        status = QStringLiteral(
            "CORRECTIONS APPROVED\n"
            "Every non-exact cell has an explicit, validated correction."
        );
    }
    if (m_matchResult.usedFuzzyMatching) {
        fuzzySummary = QString(
            "Fuzzy Accepted:\n"
            "%1\n\n"
            "Fuzzy Uncertain:\n"
            "%2\n\n"
            "Fuzzy Rejected:\n"
            "%3\n\n"
            "Approved Corrections:\n"
            "%4\n\n"
            "Edited Corrections:\n"
            "%5\n\n"
            "Thresholds:\n"
            "distance ≤ %6%, confidence ≥ %7%\n\n"
        ).arg(m_matchResult.fuzzyAcceptedCount)
         .arg(m_matchResult.fuzzyUncertainCount)
         .arg(m_matchResult.fuzzyRejectedCount)
         .arg(approvedCorrections)
         .arg(editedCorrections)
         .arg(m_maxFuzzyDistance->value())
         .arg(m_minFuzzyConfidence->value());
        if (status.isEmpty()) {
            status = QStringLiteral(
                "FUZZY SUGGESTIONS READY\n"
                "Review every non-exact cell and explicitly approve a candidate."
            );
        }
    } else if (m_matchResult.unmatchedCount == 0) {
        status = QStringLiteral("EXACT RECONSTRUCTION READY");
    } else if (status.isEmpty()) {
        status = QString("%1 cells require attention; fuzzy matching is available.")
            .arg(m_matchResult.unmatchedCount);
    }

    const QString summary = QString(
        "RECONSTRUCTION RESULTS\n\n"
        "Map:\n"
        "%1 × %2 blocks\n\n"
        "Cells Analyzed:\n"
        "%3\n\n"
        "Exact Matches:\n"
        "%4  %5%\n\n"
        "Non-exact Cells:\n"
        "%6\n\n"
        "%7"
        "Primary Tileset:\n"
        "%8 exact matches\n\n"
        "Secondary Tileset:\n"
        "%9 exact matches\n\n"
        "Rendering Context:\n"
        "%10\n\n"
        "Status:\n"
        "%11"
    ).arg(m_matchResult.mapSize.width())
     .arg(m_matchResult.mapSize.height())
     .arg(cellCount)
     .arg(m_matchResult.exactMatchCount)
     .arg(QString::number(matchPercentage, 'f', 1))
     .arg(m_matchResult.unmatchedCount)
     .arg(fuzzySummary)
     .arg(m_matchResult.primaryMatchCount)
     .arg(m_matchResult.secondaryMatchCount)
     .arg(m_renderContext.description)
     .arg(status);
    m_analysisSummary->setPlainText(summary);
    updatePreview();
    m_reviewUnmatchedButton->setEnabled(m_matchResult.unmatchedCount > 0);
    m_fuzzyMatchButton->setEnabled(m_matchResult.unmatchedCount > 0);
    updateCreateButton();
#ifndef QT_NO_DEBUG
    m_debugExportButton->setEnabled(!m_renderResult.metatiles.isEmpty());
#endif
}

void NewMapFromImageDialog::createMapFromMatch()
{
    if (!m_project) {
        QMessageBox::warning(this, QStringLiteral("Cannot Create Map"), QStringLiteral("No project is loaded."));
        return;
    }

    // Rendering and matching are intentionally repeated at commit time so the
    // map can never be created from stale tileset or rendering state. Keep the
    // explicit corrections so they can be validated against the fresh result.
    const auto corrections = m_corrections;
    runAnalysis();
    m_corrections = corrections;
    if (m_matchResult.isValid()) {
        updateMatchDisplay();
    }
    if (!m_matchResult.isValid()
        || m_matchResult.cells.isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QStringLiteral("Run analysis and resolve every unmatched cell before creating a map.")
        );
        updateCreateButton();
        return;
    }
    if (!allCellsResolved()) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QStringLiteral("Approve a candidate for every non-exact cell before creating the map.")
        );
        updateCreateButton();
        return;
    }

    const QString mapName = m_mapName->text().trimmed();
    if (mapName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Cannot Create Map"), QStringLiteral("Enter a map name."));
        return;
    }
    if (!m_project->isValidNewIdentifier(mapName)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QString("Map name '%1' must be a valid, unused identifier.").arg(mapName)
        );
        return;
    }

    const QString mapGroup = m_mapGroup->currentText().trimmed();
    if (mapGroup.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Cannot Create Map"), QStringLiteral("Select or enter a map group."));
        return;
    }
    if (!m_project->groupNames.contains(mapGroup) && !m_project->isValidNewIdentifier(mapGroup)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QString("Map group '%1' must be an existing group or a valid, unused identifier.").arg(mapGroup)
        );
        return;
    }

    const QSize mapSize(m_mapWidth->value(), m_mapHeight->value());
    if (mapSize != m_matchResult.mapSize || !m_project->mapDimensionsValid(mapSize.width(), mapSize.height())) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QStringLiteral("The map dimensions no longer match the analyzed image. Run analysis again.")
        );
        resetAnalysis(QStringLiteral("Map dimensions changed. Run analysis again."));
        return;
    }

    const QString primaryTilesetLabel = m_primaryTileset->currentText();
    const QString secondaryTilesetLabel = m_secondaryTileset->currentText();
    if (!m_project->getTileset(primaryTilesetLabel) || !m_project->getTileset(secondaryTilesetLabel)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QStringLiteral("One or both selected tilesets are no longer available. Run analysis again.")
        );
        resetAnalysis(QStringLiteral("Selected tilesets changed. Run analysis again."));
        return;
    }

    const QString layoutId = Layout::layoutConstantFromName(mapName);
    if (!m_project->isValidNewIdentifier(layoutId)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            QString("Layout ID '%1' is already in use or invalid. Choose another map name.").arg(layoutId)
        );
        return;
    }

    Blockdata importedBlockdata;
    QString blockdataError;
    if (!ImageMetatileApproval::buildBlockdata(
            m_matchResult.cells,
            m_corrections,
            mapSize,
            m_renderResult.metatiles,
            &importedBlockdata,
            &blockdataError)) {
        QMessageBox::warning(
            this,
            QStringLiteral("Cannot Create Map"),
            blockdataError.isEmpty()
                ? QStringLiteral("The match result is no longer valid. Run analysis again.")
                : blockdataError
        );
        resetAnalysis(QStringLiteral("Match results are no longer valid. Run analysis again."));
        return;
    }
    int approvedCorrectionCount = 0;
    for (const auto &cell : m_matchResult.cells) {
        if (!cell.matched) {
            approvedCorrectionCount++;
        }
    }

    const auto confirmation = QMessageBox::question(
        this,
        QStringLiteral("Create Map From Image"),
        QString(
            "Create map '%1' in group '%2' using %3 × %4 cells (%5 exact matches, %6 approved corrections)?\n\n"
            "Collision and elevation will use their default values. No project files will be written until you save."
        ).arg(mapName)
         .arg(mapGroup)
         .arg(mapSize.width())
         .arg(mapSize.height())
         .arg(m_matchResult.exactMatchCount)
         .arg(approvedCorrectionCount),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel
    );
    if (confirmation != QMessageBox::Yes) {
        return;
    }

    Project::NewMapSettings settings = m_project->newMapSettings;
    settings.name = mapName;
    settings.group = mapGroup;
    settings.canFlyTo = false;
    settings.layout.id = layoutId;
    settings.layout.name = m_project->toUniqueIdentifier(mapName + QStringLiteral("_Layout"));
    settings.layout.folderName = mapName;
    settings.layout.width = mapSize.width();
    settings.layout.height = mapSize.height();
    settings.layout.primaryTilesetLabel = primaryTilesetLabel;
    settings.layout.secondaryTilesetLabel = secondaryTilesetLabel;

    Map *map = m_project->createNewMap(settings, importedBlockdata);
    if (!map) {
        QMessageBox::critical(
            this,
            QStringLiteral("Map Creation Failed"),
            QStringLiteral("Porymap could not create the map. Check the application log for details.")
        );
        return;
    }

    m_project->newMapSettings = settings;
    QDialog::accept();
}

void NewMapFromImageDialog::reviewUnmatched()
{
    if (m_matchResult.unmatchedCount == 0) {
        return;
    }

    QDialog reviewDialog(this);
    reviewDialog.setWindowTitle(QStringLiteral("Review Fuzzy Corrections"));
    reviewDialog.setMinimumSize(900, 520);
    auto *layout = new QHBoxLayout(&reviewDialog);
    auto *cellList = new QListWidget(&reviewDialog);
    auto *sourcePreviewLabel = new QLabel(&reviewDialog);
    sourcePreviewLabel->setAlignment(Qt::AlignCenter);
    sourcePreviewLabel->setMinimumSize(180, 180);
    auto *candidatePreviewLabel = new QLabel(&reviewDialog);
    candidatePreviewLabel->setAlignment(Qt::AlignCenter);
    candidatePreviewLabel->setMinimumSize(180, 180);
    auto *candidateList = new QListWidget(&reviewDialog);
    candidateList->setSelectionMode(QAbstractItemView::SingleSelection);
    auto *detailsLabel = new QLabel(&reviewDialog);
    detailsLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailsLabel->setWordWrap(true);
    auto *approveButton = new QPushButton(QStringLiteral("Approve Selected"), &reviewDialog);
    auto *clearButton = new QPushButton(QStringLiteral("Clear Correction"), &reviewDialog);
    approveButton->setEnabled(false);
    clearButton->setEnabled(false);

    QList<int> unmatchedCellIndices;
    for (int index = 0; index < m_matchResult.cells.count(); index++) {
        const auto &cell = m_matchResult.cells.at(index);
        if (!cell.matched) {
            unmatchedCellIndices.append(index);
            cellList->addItem(QString());
        }
    }

    layout->addWidget(cellList, 2);
    auto *sourceLayout = new QVBoxLayout();
    sourceLayout->addWidget(new QLabel(QStringLiteral("Source Cell"), &reviewDialog));
    sourceLayout->addWidget(sourcePreviewLabel);
    layout->addLayout(sourceLayout, 1);
    auto *candidateLayout = new QVBoxLayout();
    candidateLayout->addWidget(new QLabel(QStringLiteral("Candidate Preview"), &reviewDialog));
    candidateLayout->addWidget(candidatePreviewLabel);
    candidateLayout->addWidget(candidateList, 1);
    candidateLayout->addWidget(approveButton);
    candidateLayout->addWidget(clearButton);
    layout->addLayout(candidateLayout, 2);
    auto *detailsLayout = new QVBoxLayout();
    detailsLayout->addWidget(detailsLabel, 1);
    layout->addLayout(detailsLayout, 2);

    const auto statusForCell = [this](int index) {
        const auto &cell = m_matchResult.cells.at(index);
        const auto correctionIt = m_corrections.constFind(index);
        if (correctionIt != m_corrections.cend()) {
            if (!correctionIt.value().approved) {
                return QStringLiteral("Edited");
            }
            return isCorrectionValid(cell, correctionIt.value())
                ? QStringLiteral("Approved")
                : QStringLiteral("Stale");
        }
        switch (cell.status) {
        case ImageMetatileMatcher::MatchStatus::FuzzyAccepted:
            return QStringLiteral("Accepted suggestion");
        case ImageMetatileMatcher::MatchStatus::FuzzyUncertain:
            return QStringLiteral("Uncertain suggestion");
        case ImageMetatileMatcher::MatchStatus::FuzzyRejected:
            return QStringLiteral("Rejected");
        default:
            return QStringLiteral("Unresolved");
        }
    };
    const auto updateCellListItem = [&, statusForCell](int index) {
        const auto &cell = m_matchResult.cells.at(index);
        const int row = unmatchedCellIndices.indexOf(index);
        if (row >= 0) {
            cellList->item(row)->setText(QString("Column %1, Row %2 — %3")
                .arg(cell.position.x())
                .arg(cell.position.y())
                .arg(statusForCell(index)));
        }
    };
    const auto candidateMatches = [](const ImageMetatileMatcher::CandidateResult &left,
                                     const ImageMetatileMatcher::CandidateResult &right) {
        return left.metatileId == right.metatileId
            && left.sourceTileset == right.sourceTileset
            && left.sourceTilesetName == right.sourceTilesetName;
    };
    const auto scaledPixmap = [](const QImage &image) {
        return QPixmap::fromImage(image.scaled(
            180,
            180,
            Qt::KeepAspectRatio,
            Qt::FastTransformation
        ));
    };
    const auto updateDetails = [&, statusForCell, candidateMatches](int index, int candidateRow) {
        if (index < 0 || index >= m_matchResult.cells.count()) {
            return;
        }
        const auto &cell = m_matchResult.cells.at(index);
        sourcePreviewLabel->setPixmap(scaledPixmap(cell.sourceImage));
        const auto correctionIt = m_corrections.constFind(index);
        const bool hasCorrection = correctionIt != m_corrections.cend();
        approveButton->setEnabled(candidateRow >= 0 && candidateRow < cell.rankedCandidates.count());
        clearButton->setEnabled(hasCorrection);

        QString details = QString("Status: %1").arg(statusForCell(index));
        if (candidateRow >= 0 && candidateRow < cell.rankedCandidates.count()) {
            const auto &candidate = cell.rankedCandidates.at(candidateRow);
            const auto *rendered = findRenderedCandidate(candidate);
            if (rendered) {
                candidatePreviewLabel->setPixmap(scaledPixmap(rendered->image));
            } else {
                candidatePreviewLabel->clear();
            }
            details.append(QString(
                "\n\nSelected candidate: %1\nTileset: %2\nMetatile ID: 0x%3\nDistance: %4%"
            ).arg(candidate.sourceTilesetName)
             .arg(candidate.sourceTileset == MetatileRenderService::SourceTileset::Primary
                 ? QStringLiteral("primary")
                 : QStringLiteral("secondary"))
             .arg(candidate.metatileId, 4, 16, QLatin1Char('0'))
             .arg(QString::number(candidate.distance * 100.0, 'f', 2)));
        } else {
            candidatePreviewLabel->setText(QStringLiteral("No ranked candidate selected."));
        }
        if (!cell.rankedCandidates.isEmpty()) {
            details.append(QString(
                "\n\nBest distance: %1%\nConfidence: %2%"
            ).arg(QString::number(cell.bestDistance * 100.0, 'f', 2))
             .arg(QString::number(cell.confidence * 100.0, 'f', 1)));
        }
        detailsLabel->setText(details);
    };

    const auto populateCandidates = [&, candidateMatches, updateDetails](int row) {
        if (row < 0 || row >= unmatchedCellIndices.count()) {
            return;
        }
        const int index = unmatchedCellIndices.at(row);
        const auto &cell = m_matchResult.cells.at(index);
        const QSignalBlocker blocker(candidateList);
        candidateList->clear();
        int selectedRow = cell.rankedCandidates.isEmpty() ? -1 : 0;
        const auto correctionIt = m_corrections.constFind(index);
        for (int candidateIndex = 0; candidateIndex < cell.rankedCandidates.count(); candidateIndex++) {
            const auto &candidate = cell.rankedCandidates.at(candidateIndex);
            if (correctionIt != m_corrections.cend()
                && candidateMatches(candidate, correctionIt.value().candidate)) {
                selectedRow = candidateIndex;
            }
            candidateList->addItem(QString("%1. %2 — ID 0x%3 — distance %4%")
                .arg(candidateIndex + 1)
                .arg(candidate.sourceTilesetName)
                .arg(candidate.metatileId, 4, 16, QLatin1Char('0'))
                .arg(QString::number(candidate.distance * 100.0, 'f', 2)));
        }
        if (selectedRow >= 0) {
            candidateList->setCurrentRow(selectedRow);
        }
        updateDetails(index, selectedRow);
    };

    for (int index : unmatchedCellIndices) {
        updateCellListItem(index);
    }
    connect(
        cellList,
        &QListWidget::currentRowChanged,
        &reviewDialog,
        [populateCandidates](int row) {
        if (row < 0) {
            return;
        }
        populateCandidates(row);
    });
    connect(candidateList, &QListWidget::currentRowChanged, &reviewDialog, [&](int candidateRow) {
        const int cellRow = cellList->currentRow();
        if (cellRow < 0 || candidateRow < 0 || cellRow >= unmatchedCellIndices.count()) {
            return;
        }
        const int index = unmatchedCellIndices.at(cellRow);
        const auto &cell = m_matchResult.cells.at(index);
        const auto &candidate = cell.rankedCandidates.at(candidateRow);
        const auto *rendered = findRenderedCandidate(candidate);
        if (!rendered) {
            return;
        }
        Correction correction;
        correction.candidate = candidate;
        correction.sourceImage = cell.sourceImage;
        correction.renderedImage = rendered->image;
        correction.approved = false;
        m_corrections.insert(index, correction);
        updateCellListItem(index);
        updateDetails(index, candidateRow);
    });
    connect(approveButton, &QPushButton::clicked, &reviewDialog, [&] {
        const int cellRow = cellList->currentRow();
        const int candidateRow = candidateList->currentRow();
        if (cellRow < 0 || candidateRow < 0 || cellRow >= unmatchedCellIndices.count()) {
            return;
        }
        const int index = unmatchedCellIndices.at(cellRow);
        const auto &cell = m_matchResult.cells.at(index);
        if (candidateRow >= cell.rankedCandidates.count()) {
            return;
        }
        const auto &candidate = cell.rankedCandidates.at(candidateRow);
        const auto *rendered = findRenderedCandidate(candidate);
        if (!rendered) {
            return;
        }
        Correction correction;
        correction.candidate = candidate;
        correction.sourceImage = cell.sourceImage;
        correction.renderedImage = rendered->image;
        correction.approved = true;
        m_corrections.insert(index, correction);
        auto correctionIt = m_corrections.find(index);
        correctionIt.value().approved = true;
        updateCellListItem(index);
        updateDetails(index, candidateRow);
    });
    connect(clearButton, &QPushButton::clicked, &reviewDialog, [&] {
        const int cellRow = cellList->currentRow();
        if (cellRow < 0 || cellRow >= unmatchedCellIndices.count()) {
            return;
        }
        const int index = unmatchedCellIndices.at(cellRow);
        m_corrections.remove(index);
        updateCellListItem(index);
        const int candidateRow = candidateList->currentRow();
        updateDetails(index, candidateRow);
    });

    cellList->setCurrentRow(0);
    reviewDialog.exec();
    updateMatchDisplay();
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

void NewMapFromImageDialog::updateCreateButton()
{
    m_createMapButton->setEnabled(allCellsResolved());
}

int NewMapFromImageDialog::cellIndex(const ImageMetatileMatcher::CellResult &cell) const
{
    return cell.position.y() * m_matchResult.mapSize.width() + cell.position.x();
}

const MetatileRenderService::RenderedMetatile *NewMapFromImageDialog::findRenderedCandidate(
    const ImageMetatileMatcher::CandidateResult &candidate) const
{
    for (const auto &rendered : m_renderResult.metatiles) {
        if (rendered.metatileId == candidate.metatileId
            && rendered.source == candidate.sourceTileset
            && rendered.sourceTilesetName == candidate.sourceTilesetName) {
            return &rendered;
        }
    }
    return nullptr;
}

bool NewMapFromImageDialog::isCorrectionValid(
    const ImageMetatileMatcher::CellResult &cell,
    const Correction &correction) const
{
    return ImageMetatileApproval::isCorrectionValid(
        cell,
        correction,
        m_renderResult.metatiles
    );
}

bool NewMapFromImageDialog::allCellsResolved() const
{
    if (!m_matchResult.isValid()) {
        return false;
    }
    return ImageMetatileApproval::allCellsResolved(
        m_matchResult.cells,
        m_corrections,
        m_matchResult.mapSize,
        m_renderResult.metatiles
    );
}

void NewMapFromImageDialog::updateCorrectionPreviews()
{
    if (m_analysisReconstructedImage.isNull() || m_analysisDifferenceImage.isNull()) {
        return;
    }

    m_matchResult.reconstructedImage = m_analysisReconstructedImage;
    m_matchResult.differenceImage = m_analysisDifferenceImage;
    QPainter reconstructedPainter(&m_matchResult.reconstructedImage);
    reconstructedPainter.setCompositionMode(QPainter::CompositionMode_Source);
    QPainter differencePainter(&m_matchResult.differenceImage);
    for (const auto &cell : m_matchResult.cells) {
        if (cell.matched) {
            continue;
        }
        const auto correctionIt = m_corrections.constFind(cellIndex(cell));
        if (correctionIt == m_corrections.cend()
            || correctionIt.value().sourceImage != cell.sourceImage) {
            continue;
        }
        const auto *rendered = findRenderedCandidate(correctionIt.value().candidate);
        if (!rendered || rendered->image != correctionIt.value().renderedImage) {
            continue;
        }
        const QRect cellRect(
            cell.position.x() * cell.sourceImage.width(),
            cell.position.y() * cell.sourceImage.height(),
            cell.sourceImage.width(),
            cell.sourceImage.height()
        );
        reconstructedPainter.drawImage(cellRect.topLeft(), rendered->image);
        differencePainter.drawImage(cellRect.topLeft(), cell.sourceImage);
        differencePainter.fillRect(
            cellRect,
            correctionIt.value().approved
                ? QColor(24, 170, 90, 120)
                : QColor(60, 120, 220, 150)
        );
    }
}

} // namespace Studio