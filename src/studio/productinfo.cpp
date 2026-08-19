#include "studio/productinfo.h"

namespace Studio::ProductInfo {

const QString &displayName() {
    static const QString name = QStringLiteral("Porymap Studio");
    return name;
}

const QString &description() {
    static const QString value = QStringLiteral(
        "An enhanced Gen 3 Pokemon map editor focused on faster map creation, "
        "automated workflows, and pokeemerald-expansion development."
    );
    return value;
}

const QString &upstreamName() {
    static const QString name = QStringLiteral("Porymap");
    return name;
}

}