#pragma once

#include <QDialog>

#include "studio/assettilesetbuilder.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class Project;
class QPushButton;
class Tileset;

namespace Studio {

class AssetTilesetDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit AssetTilesetDialog(Project *project, QWidget *parent = nullptr);

signals:
    void applied(Tileset *tileset);

private:
    Project *m_project = nullptr;
    AssetTilesetBuilder m_builder;
    QLineEdit *m_name = nullptr;
    QComboBox *m_type = nullptr;
    QCheckBox *m_checkerboardFill = nullptr;
    QCheckBox *m_splitIfNeeded = nullptr;
    QListWidget *m_assets = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_createButton = nullptr;

    QStringList assetPaths() const;
    ImageTilesetBuilder::Options options(bool secondary) const;
    void addFiles();
    void addFolder();
    void removeSelected();
    void createTileset();
    void updateCreateButton();
};

} // namespace Studio