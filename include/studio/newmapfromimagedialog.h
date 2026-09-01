#pragma once

#include <QDialog>
#include <QHash>

#include "studio/imagemetatilematcher.h"
#include "studio/imagemetatileapproval.h"
#include "studio/imagetilesetbuilder.h"
#include "studio/mapimageanalyzer.h"
#include "studio/metatilerenderservice.h"
#include "studio/smartcollisionsuggester.h"

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class Project;

namespace Studio {

class NewMapFromImageDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit NewMapFromImageDialog(
        Project *project,
        MetatileRenderService::RenderContext renderContext,
        QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    Project *m_project;
    MetatileRenderService::RenderContext m_renderContext;
    MapImageAnalyzer m_imageAnalyzer;
    MapImageAnalyzer::Result m_imageResult;
    MetatileRenderService m_renderService;
    MetatileRenderService::Result m_renderResult;
    ImageMetatileMatcher m_matcher;
    ImageMetatileMatcher::Result m_matchResult;
    ImageTilesetBuilder m_tilesetBuilder;
    ImageTilesetBuilder::Result m_tilesetBuildResult;
    ImageTilesetBuilder::PairResult m_tilesetPairBuildResult;
    bool m_usingGeneratedTilesetPair = false;
    QImage m_analysisReconstructedImage;
    QImage m_analysisDifferenceImage;

    using Correction = ImageMetatileCorrection;
    QHash<int, Correction> m_corrections;

    QLineEdit *m_imagePath = nullptr;
    QLabel *m_imageDimensions = nullptr;
    QLabel *m_previewLabel = nullptr;
    QScrollArea *m_previewScrollArea = nullptr;
    QLabel *m_reconstructedPreviewLabel = nullptr;
    QScrollArea *m_reconstructedPreviewScrollArea = nullptr;
    QLabel *m_differencePreviewLabel = nullptr;
    QScrollArea *m_differencePreviewScrollArea = nullptr;
    QTabWidget *m_previewTabs = nullptr;
    QLineEdit *m_mapName = nullptr;
    QComboBox *m_mapGroup = nullptr;
    QComboBox *m_primaryTileset = nullptr;
    QComboBox *m_secondaryTileset = nullptr;
    QCheckBox *m_createTilesetFromImage = nullptr;
    QCheckBox *m_splitTilesetsIfNeeded = nullptr;
    QLineEdit *m_newTilesetName = nullptr;
    QComboBox *m_newTilesetType = nullptr;
    QLabel *m_newTilesetHelp = nullptr;
    QCheckBox *m_autoDimensions = nullptr;
    QSpinBox *m_mapWidth = nullptr;
    QSpinBox *m_mapHeight = nullptr;
    QSpinBox *m_maxFuzzyDistance = nullptr;
    QSpinBox *m_minFuzzyConfidence = nullptr;
    QCheckBox *m_inferBlockedCollision = nullptr;
    QSpinBox *m_blockedCollision = nullptr;
    QPlainTextEdit *m_analysisSummary = nullptr;
    QPushButton *m_analyzeButton = nullptr;
    QPushButton *m_fuzzyMatchButton = nullptr;
    QPushButton *m_createMapButton = nullptr;
    QPushButton *m_reviewUnmatchedButton = nullptr;
    QPushButton *m_reviewCollisionButton = nullptr;
    CollisionSuggestionResult m_collisionResult;
    SmartCollisionSuggester m_collisionSuggester;
    bool m_collisionSuggestionsApplied = false;
#ifndef QT_NO_DEBUG
    QPushButton *m_debugExportButton = nullptr;
#endif

    void browseForImage();
    bool loadImage(const QString &filepath);
    void updatePreview();
    void updatePreviewForImage(const QImage &image, QScrollArea *scrollArea, QLabel *label);
    void resetAnalysis(const QString &message = QString());
    void runAnalysis();
    void runFuzzyMatching();
    void updateMatchDisplay();
    void createMapFromMatch();
    void createMapWithGeneratedTileset();
    void reviewUnmatched();
    void exportDebugMetatiles();
    void updateDimensionControls();
    void updateCreateButton();
    void updateTilesetSourceControls();
    ImageTilesetBuilder::Options tilesetBuildOptions(bool secondary) const;
    int cellIndex(const ImageMetatileMatcher::CellResult &cell) const;
    const MetatileRenderService::RenderedMetatile *findRenderedCandidate(
        const ImageMetatileMatcher::CandidateResult &candidate) const;
    bool isCorrectionValid(
        const ImageMetatileMatcher::CellResult &cell,
        const Correction &correction) const;
    bool allCellsResolved() const;
    void updateCorrectionPreviews();
    bool reviewCollisionSuggestions(Blockdata *blockdata, const QSize &mapSize);
};

} // namespace Studio