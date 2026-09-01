#pragma once

#include <QHash>
#include <QList>

namespace Studio {

struct TerrainRule
{
    QList<uint16_t> variants;
    QHash<int, int> maskToVariant;

    bool isValid() const;
    int variantForMask(int cardinalMask) const;
};

class TerrainRuleService
{
public:
    static TerrainRule marchingSquaresRule(const QList<uint16_t> &variants);
    static TerrainRule customRule(const QList<uint16_t> &variants,
                                  const QHash<int, int> &maskToVariant);
};

} // namespace Studio