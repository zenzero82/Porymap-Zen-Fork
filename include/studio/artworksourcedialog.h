#pragma once

#include "artworksourcegenerator.h"

#include <QDialog>

class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTabWidget;
class Project;
class Tileset;

namespace Studio {

class ArtworkSourceDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ArtworkSourceDialog(Project *project, Tileset *tileset, QWidget *parent = nullptr);

signals:
    void compiledOutputApplied();

private slots:
    void chooseArtwork();
    void chooseOutput();
    void choosePrimaryContext();
    void generate();
    void compileGeneratedSource();

private:
    Project *project = nullptr;
    Tileset *tileset = nullptr;
    QLineEdit *artworkEdit = nullptr;
    QLineEdit *outputEdit = nullptr;
    QLineEdit *primaryContextEdit = nullptr;
    QPlainTextEdit *previewEdit = nullptr;
    QTabWidget *previewTabs = nullptr;
    QPushButton *generateButton = nullptr;
    QPushButton *compileButton = nullptr;
    ArtworkSourceResult generated;
};

} // namespace Studio