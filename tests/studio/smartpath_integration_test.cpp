#include "block.h"
#include "layoutpixmapitem.h"
#include "maplayout.h"
#include "metatile.h"
#include "metatileselector.h"
#include "settings.h"

#include <QApplication>
#include <QGraphicsSceneMouseEvent>
#include <iostream>

static void setEventPosition(QGraphicsSceneMouseEvent &event, int x, int y)
{
    event.setPos(QPointF(x * Metatile::pixelWidth() + 1, y * Metatile::pixelHeight() + 1));
}

static MetatileSelection terrainSelection()
{
    MetatileSelection selection;
    selection.dimensions = QSize(3, 3);
    selection.hasCollision = true;
    for (int i = 0; i < 9; ++i) {
        selection.metatileItems.append(MetatileSelectionItem{true, static_cast<uint16_t>(10 + i)});
        selection.collisionItems.append(
            CollisionSelectionItem{true, static_cast<uint16_t>(i % 4), static_cast<uint16_t>(i)});
    }
    return selection;
}

static bool isBlock(const Layout &layout, int x, int y,
                    uint16_t metatile, uint16_t collision, uint16_t elevation)
{
    Block block;
    return layout.getBlock(x, y, &block) && block.metatileId() == metatile
        && block.collision() == collision && block.elevation() == elevation;
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    Layout layout;
    layout.setDimensions(7, 7);
    Settings settings;
    settings.smartPathsEnabled = true;
    MetatileSelector selector(3, &layout);
    selector.setPrefabSelection(terrainSelection());
    LayoutPixmapItem item(&layout, &selector, &settings);

    QHash<int, int> custom;
    for (int mask = 0; mask < 16; ++mask) custom.insert(mask, 8);
    item.setTerrainRuleMapping(custom);

    const Blockdata empty = layout.blockdata;
    QGraphicsSceneMouseEvent press(QEvent::GraphicsSceneMousePress);
    QGraphicsSceneMouseEvent move(QEvent::GraphicsSceneMouseMove);
    QGraphicsSceneMouseEvent release(QEvent::GraphicsSceneMouseRelease);
    setEventPosition(press, 2, 2);
    setEventPosition(move, 3, 2);
    setEventPosition(release, 3, 2);
    item.paint(&press);
    item.paint(&move);
    item.paint(&release);
    if (layout.editHistory.count() != 1 || !isBlock(layout, 2, 2, 18, 0, 8)) {
        std::cerr << "Custom pencil rule, payload, or gesture grouping failed.\n";
        return 1;
    }
    const Blockdata painted = layout.blockdata;
    layout.editHistory.undo();
    if (layout.blockdata != empty) {
        std::cerr << "Pencil undo failed.\n";
        return 1;
    }
    layout.editHistory.redo();
    if (layout.blockdata != painted) {
        std::cerr << "Pencil redo failed.\n";
        return 1;
    }

    layout.editHistory.clear();
    layout.setBlockdata(empty);
    for (int y = 1; y <= 3; ++y)
        for (int x = 1; x <= 3; ++x)
            layout.setBlock(x, y, Block(5, 0, 0), false);
    const Blockdata beforeFill = layout.blockdata;
    QGraphicsSceneMouseEvent fill(QEvent::GraphicsSceneMousePress);
    setEventPosition(fill, 2, 2);
    item.floodFill(&fill);
    if (layout.editHistory.count() != 1 || !isBlock(layout, 1, 1, 18, 0, 8)
        || !isBlock(layout, 3, 3, 18, 0, 8) || !isBlock(layout, 0, 0, 0, 0, 0)) {
        std::cerr << "Connected fill boundary or payload failed.\n";
        return 1;
    }
    const Blockdata filled = layout.blockdata;
    layout.editHistory.undo();
    if (layout.blockdata != beforeFill) {
        std::cerr << "Fill undo failed.\n";
        return 1;
    }
    layout.editHistory.redo();
    if (layout.blockdata != filled) {
        std::cerr << "Fill redo failed.\n";
        return 1;
    }

    std::cout << "All Smart Paths integration tests passed.\n";
    return 0;
}