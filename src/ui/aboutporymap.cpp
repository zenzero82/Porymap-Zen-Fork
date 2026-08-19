#include "aboutporymap.h"
#include "ui_aboutporymap.h"
#include "studio/productinfo.h"

AboutPorymap::AboutPorymap(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutPorymap)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("About %1").arg(Studio::ProductInfo::displayName()));
    ui->label_Title->setText(Studio::ProductInfo::displayName());
    ui->label_Description->setText(Studio::ProductInfo::description());

    // Set font in the stylesheet as opposed to the form so that it won't be overwritten by custom user fonts.
    setStyleSheet("QLabel { font-family: \"Arial\"; }"
                  "QLabel#label_Title { font-size: 22pt; font-weight: bold; } "
                  "QLabel#label_Version { font-size: 12pt; }");

    this->ui->label_Version->setText(getVersionString());

    layout()->setSizeConstraint(QLayout::SetFixedSize);
}

QString AboutPorymap::getVersionString() {
    static const QString commitHash = QStringLiteral(PORYMAP_LATEST_COMMIT);
    static const QString versionString = QString("Version %1%2\nQt %3 (%4)\n%5")
                                        .arg(QCoreApplication::applicationVersion())
                                        .arg(commitHash.isEmpty() ? "" : QString(" (%1)").arg(commitHash))
                                        .arg(QStringLiteral(QT_VERSION_STR))
                                        .arg(QSysInfo::buildCpuArchitecture())
                                        .arg(QStringLiteral(__DATE__));
    return versionString;
}

AboutPorymap::~AboutPorymap()
{
    delete ui;
}
