#include "studio/assettilesetdialog.h"

#include "config.h"
#include "project.h"
#include "tile.h"
#include "tileset.h"
#include "validator.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace Studio {

AssetTilesetDialog::AssetTilesetDialog(Project *project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("New Tileset From Assets"));
    resize(720, 560);

    auto *mainLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();
    formLayout->setHorizontalSpacing(12);
    formLayout->setVerticalSpacing(7);

    const QString prefix =
        projectConfig.getIdentifier(ProjectIdentifier::symbol_tilesets_prefix);
    m_name = new QLineEdit(this);
    m_name->setValidator(new IdentifierValidator(prefix, this));
    m_name->setText(
        m_project
            ? m_project->toUniqueIdentifier(prefix + QStringLiteral("Assets"))
            : prefix + QStringLiteral("Assets")
    );
    m_type = new QComboBox(this);
    m_type->addItems({QStringLiteral("Primary"), QStringLiteral("Secondary")});
    m_checkerboardFill = new QCheckBox(this);
    m_checkerboardFill->setChecked(porymapConfig.tilesetCheckerboardFill);
    m_splitIfNeeded = new QCheckBox(
        QStringLiteral("Create primary and secondary tilesets if one role is full"),
        this
    );
    formLayout->addRow(QStringLiteral("Tileset Name:"), m_name);
    formLayout->addRow(QStringLiteral("Tileset Role:"), m_type);
    formLayout->addRow(QStringLiteral("Checkerboard Fill:"), m_checkerboardFill);
    formLayout->addRow(QString(), m_splitIfNeeded);
    mainLayout->addLayout(formLayout);

    mainLayout->addWidget(new QLabel(
        QStringLiteral(
            "Add PNGs or other supported graphics. Images are padded to the 8 × 8 tile grid, "
            "deduplicated, reduced to the project palette format, and grouped into 16 × 16 metatiles."
        ),
        this
    ));

    m_assets = new QListWidget(this);
    m_assets->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(m_assets, 1);

    auto *assetButtons = new QHBoxLayout();
    auto *addFilesButton = new QPushButton(QStringLiteral("Add Files..."), this);
    auto *addFolderButton = new QPushButton(QStringLiteral("Add Folder..."), this);
    auto *removeButton = new QPushButton(QStringLiteral("Remove Selected"), this);
    assetButtons->addWidget(addFilesButton);
    assetButtons->addWidget(addFolderButton);
    assetButtons->addStretch();
    assetButtons->addWidget(removeButton);
    mainLayout->addLayout(assetButtons);

    m_status = new QLabel(QStringLiteral("No assets selected."), this);
    m_status->setWordWrap(true);
    mainLayout->addWidget(m_status);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Cancel | QDialogButtonBox::Ok,
        Qt::Horizontal,
        this
    );
    m_createButton = buttonBox->button(QDialogButtonBox::Ok);
    m_createButton->setText(QStringLiteral("Create Tileset"));
    m_createButton->setEnabled(false);
    mainLayout->addWidget(buttonBox);

    connect(addFilesButton, &QPushButton::clicked, this, &AssetTilesetDialog::addFiles);
    connect(addFolderButton, &QPushButton::clicked, this, &AssetTilesetDialog::addFolder);
    connect(removeButton, &QPushButton::clicked, this, &AssetTilesetDialog::removeSelected);
    connect(m_assets, &QListWidget::itemSelectionChanged, this, &AssetTilesetDialog::updateCreateButton);
    connect(m_name, &QLineEdit::textChanged, this, &AssetTilesetDialog::updateCreateButton);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AssetTilesetDialog::createTileset);
}

QStringList AssetTilesetDialog::assetPaths() const
{
    QStringList paths;
    for (int index = 0; index < m_assets->count(); index++) {
        paths.append(m_assets->item(index)->data(Qt::UserRole).toString());
    }
    return paths;
}

void AssetTilesetDialog::addFiles()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Choose Image Assets"),
        QString(),
        QStringLiteral("Image Files (*.png *.bmp *.gif *.jpg *.jpeg)")
    );
    for (const QString &path : paths) {
        if (assetPaths().contains(path)) continue;
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), m_assets);
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
    }
    updateCreateButton();
}

void AssetTilesetDialog::addFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Choose Asset Folder")
    );
    if (folder.isEmpty()) return;

    const QStringList filters = {
        QStringLiteral("*.png"),
        QStringLiteral("*.bmp"),
        QStringLiteral("*.gif"),
        QStringLiteral("*.jpg"),
        QStringLiteral("*.jpeg")
    };
    QDirIterator iterator(folder, filters, QDir::Files, QDirIterator::Subdirectories);
    QStringList existing = assetPaths();
    while (iterator.hasNext()) {
        const QString path = iterator.next();
        if (existing.contains(path)) continue;
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), m_assets);
        item->setToolTip(path);
        item->setData(Qt::UserRole, path);
        existing.append(path);
    }
    updateCreateButton();
}

void AssetTilesetDialog::removeSelected()
{
    qDeleteAll(m_assets->selectedItems());
    updateCreateButton();
}

void AssetTilesetDialog::updateCreateButton()
{
    const bool hasAssets = m_assets && m_assets->count() > 0;
    const bool validName = m_project
        && !m_name->text().trimmed().isEmpty()
        && m_project->isValidNewIdentifier(m_name->text().trimmed());
    m_createButton->setEnabled(hasAssets && validName);
    if (!hasAssets) {
        m_status->setText(QStringLiteral("No assets selected."));
    } else {
        m_status->setText(QString("%1 asset(s) ready to package.").arg(m_assets->count()));
    }
}

ImageTilesetBuilder::Options AssetTilesetDialog::options(bool secondary) const
{
    ImageTilesetBuilder::Options result;
    result.maxTiles = secondary
        ? Project::getNumTilesSecondary()
        : Project::getNumTilesPrimary();
    result.maxMetatiles = secondary
        ? Project::getNumMetatilesSecondary()
        : Project::getNumMetatilesPrimary();
    result.tileIdBase = secondary ? Project::getNumTilesPrimary() : 0;
    result.metatileIdBase = secondary ? Project::getNumMetatilesPrimary() : 0;
    result.paletteId = secondary ? Project::getNumPalettesPrimary() : 0;
    result.tilesPerMetatile = projectConfig.getNumTilesInMetatile();
    return result;
}

void AssetTilesetDialog::createTileset()
{
    const QString name = m_name->text().trimmed();
    const bool secondary = m_type->currentIndex() == 1;
    if (!m_project || assetPaths().isEmpty()) return;
    const QString prefix =
        projectConfig.getIdentifier(ProjectIdentifier::symbol_tilesets_prefix);
    if (!name.startsWith(prefix) || !m_project->isValidNewIdentifier(name)) {
        m_status->setText(QStringLiteral("Choose a valid, unused tileset name with the project prefix."));
        return;
    }

    const AssetTilesetBuilder::Result result =
        m_builder.build(assetPaths(), options(secondary));
    ImageTilesetBuilder::PairResult pairResult;
    bool usingPair = false;
    if (!result.isValid() && m_splitIfNeeded->isChecked() && !result.sourceImage.isNull()) {
        pairResult = ImageTilesetBuilder().buildPair(
            result.sourceImage,
            options(false),
            options(true)
        );
        usingPair = pairResult.isValid();
    }
    if (!result.isValid() && !usingPair) {
        m_status->setText(
            !pairResult.errorMessage.isEmpty() ? pairResult.errorMessage : result.errorMessage
        );
        return;
    }

    const QString primaryName = usingPair ? name + QStringLiteral("Primary") : name;
    const QString secondaryName = usingPair ? name + QStringLiteral("Secondary") : name;
    if (usingPair
        && (!m_project->isValidNewIdentifier(primaryName)
            || !m_project->isValidNewIdentifier(secondaryName))) {
        m_status->setText(QString(
            "Generated names '%1' and '%2' must both be valid and unused. Choose another base name."
        ).arg(primaryName, secondaryName));
        return;
    }
    const QString summary = usingPair
        ? QString(
            "Create primary tileset '%1' (%2 tiles, %3 metatiles) and secondary tileset "
            "'%4' (%5 tiles, %6 metatiles)?"
          ).arg(primaryName)
           .arg(pairResult.primary.uniqueTileCount)
           .arg(pairResult.primary.uniqueMetatileCount)
           .arg(secondaryName)
           .arg(pairResult.secondary.uniqueTileCount)
           .arg(pairResult.secondary.uniqueMetatileCount)
        : QString(
            "Create %1 tileset '%2' with %3 unique tiles and %4 metatiles?"
          ).arg(secondary ? QStringLiteral("secondary") : QStringLiteral("primary"))
           .arg(name)
           .arg(result.tileset.uniqueTileCount)
           .arg(result.tileset.uniqueMetatileCount);
    const auto answer = QMessageBox::question(
        this,
        QStringLiteral("Create Tileset From Assets"),
        QString(
            "%1\n\n"
            "The generated tileset will be available from the standard New Map dialog."
        ).arg(summary),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel
    );
    if (answer != QMessageBox::Yes) return;

    const auto createOne = [this](
        const QString &tilesetName,
        bool isSecondary,
        const ImageTilesetBuilder::Result &built) {
        return m_project->createNewTileset(
        tilesetName,
        isSecondary,
        m_checkerboardFill->isChecked(),
        [&built](Tileset *created) {
            QImage tilesImage = built.tilesImage;
            if (!created->loadTilesImage(&tilesImage)) return false;
            QList<Metatile *> metatiles;
            metatiles.reserve(built.metatiles.size());
            for (const Metatile &metatile : built.metatiles) {
                metatiles.append(new Metatile(metatile));
            }
            created->setMetatiles(metatiles);
            const int paletteId = created->is_secondary
                ? Project::getNumPalettesPrimary()
                : 0;
            if (paletteId < 0
                || paletteId >= created->palettes.size()
                || paletteId >= created->palettePreviews.size()) {
                return false;
            }
            created->palettes[paletteId] = built.palette;
            created->palettePreviews[paletteId] = built.palette;
            return true;
        }
        );
    };

    Tileset *createdPrimary = nullptr;
    Tileset *createdSecondary = nullptr;
    if (usingPair) {
        createdPrimary = createOne(primaryName, false, pairResult.primary);
        if (createdPrimary) {
            createdSecondary = createOne(secondaryName, true, pairResult.secondary);
        }
    } else if (secondary) {
        createdSecondary = createOne(name, true, result.tileset);
    } else {
        createdPrimary = createOne(name, false, result.tileset);
    }
    if ((usingPair && (!createdPrimary || !createdSecondary))
        || (!usingPair && !createdPrimary && !createdSecondary)) {
        m_status->setText(QStringLiteral(
            "Tileset creation failed. Check the application log for details."
        ));
        return;
    }

    porymapConfig.tilesetCheckerboardFill = m_checkerboardFill->isChecked();
    emit applied(createdPrimary ? createdPrimary : createdSecondary);
    accept();
}

} // namespace Studio