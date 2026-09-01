#include "studio/terrainrules.h"

namespace Studio {

bool TerrainRule::isValid() const
{
    if (variants.size() != 9) return false;
    for (auto it = maskToVariant.constBegin(); it != maskToVariant.constEnd(); ++it) {
        if (it.key() < 0 || it.key() > 15 || it.value() < 0 || it.value() >= variants.size())
            return false;
    }
    return true;
}

int TerrainRule::variantForMask(int cardinalMask) const
{
    if (!isValid()) return -1;
    return maskToVariant.value(cardinalMask, 4);
}

TerrainRule TerrainRuleService::customRule(const QList<uint16_t> &variants,
                                           const QHash<int, int> &maskToVariant)
{
    TerrainRule rule{variants, maskToVariant};
    return rule.isValid() ? rule : TerrainRule{};
}

TerrainRule TerrainRuleService::marchingSquaresRule(const QList<uint16_t> &variants)
{
    static const int offsets[16] = {
        4, 4, 4, 6, 4, 4, 0, 3,
        4, 8, 4, 7, 2, 5, 1, 4
    };
    QHash<int, int> mapping;
    for (int mask = 0; mask < 16; ++mask) mapping.insert(mask, offsets[mask]);
    return customRule(variants, mapping);
}

} // namespace Studio