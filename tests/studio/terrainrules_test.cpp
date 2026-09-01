#include "studio/terrainrules.h"

#include <iostream>

using namespace Studio;

int main()
{
    QList<uint16_t> variants;
    for (uint16_t i = 0; i < 9; ++i) variants.append(i + 10);
    const TerrainRule rule = TerrainRuleService::marchingSquaresRule(variants);
    const int expected[16] = {
        4, 4, 4, 6, 4, 4, 0, 3,
        4, 8, 4, 7, 2, 5, 1, 4
    };
    if (!rule.isValid()) {
        std::cerr << "Marching-squares rule mapping failed.\n";
        return 1;
    }
    for (int mask = 0; mask < 16; ++mask) {
        if (rule.variantForMask(mask) != expected[mask]) {
            std::cerr << "Marching-squares mask " << mask << " failed.\n";
            return 1;
        }
    }
    const TerrainRule custom = TerrainRuleService::customRule(
        variants, {{0, 2}, {15, 8}});
    if (!custom.isValid() || custom.variantForMask(0) != 2 || custom.variantForMask(15) != 8) {
        std::cerr << "Custom terrain rule mapping failed.\n";
        return 1;
    }
    if (TerrainRuleService::customRule({1, 2}, {}).isValid()) {
        std::cerr << "Invalid terrain rules should be rejected.\n";
        return 1;
    }
    if (TerrainRuleService::customRule(variants, {{-1, 0}}).isValid()
        || TerrainRuleService::customRule(variants, {{16, 0}}).isValid()
        || TerrainRuleService::customRule(variants, {{0, 9}}).isValid()) {
        std::cerr << "Out-of-range masks and variants should be rejected.\n";
        return 1;
    }
    std::cout << "All terrain rule tests passed.\n";
    return 0;
}