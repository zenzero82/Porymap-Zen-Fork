#pragma once

#include <QDialog>

#include "studio/imagemetatilematcher.h"
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
    QComboBox *m_primaryTileset = nullptr;
    QComboBox *m_secondaryTileset = nullptr;
    QCheckBox *m_autoDimensions = nullptr;
    QSpinBox *m_mapWidth = nullptr;
    QSpinBox *m_mapHeight = nullptr;
    QPlainTextEdit *m_analysisSummary = nullptr;
    QPushButton *m_analyzeButton = nullptr;
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
    void reviewUnmatched();
    void exportDebugMetatiles();
    void updateDimensionControls();
};

} // namespace Studio