#pragma once

#include <QDialog>
#include <QHash>

#include "studio/imagemetatilematcher.h"
#include "studio/imagemetatileapproval.h"
#include "studio/mapimageanalyzer.h"
#include "studio/metatilerenderservice.h"

class QCheckBox;
class QComboBox;
class QEvent;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
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
    QCheckBox *m_autoDimensions = nullptr;
    QSpinBox *m_mapWidth = nullptr;
    QSpinBox *m_mapHeight = nullptr;
    QSpinBox *m_maxFuzzyDistance = nullptr;
    QSpinBox *m_minFuzzyConfidence = nullptr;
    QPlainTextEdit *m_analysisSummary = nullptr;
    QPushButton *m_analyzeButton = nullptr;
    QPushButton *m_fuzzyMatchButton = nullptr;
    QPushButton *m_createMapButton = nullptr;
    QPushButton *m_reviewUnmatchedButton = nullptr;
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
    void reviewUnmatched();
    void exportDebugMetatiles();
    void updateDimensionControls();
    void updateCreateButton();
    int cellIndex(const ImageMetatileMatcher::CellResult &cell) const;
    const MetatileRenderService::RenderedMetatile *findRenderedCandidate(
        const ImageMetatileMatcher::CandidateResult &candidate) const;
    bool isCorrectionValid(
        const ImageMetatileMatcher::CellResult &cell,
        const Correction &correction) const;
    bool allCellsResolved() const;
    void updateCorrectionPreviews();
};

} // namespace Studio