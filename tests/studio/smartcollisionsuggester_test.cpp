#include "studio/smartcollisionsuggester.h"

#include <QCoreApplication>
#include <QPainter>
#include <iostream>

using namespace Studio;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QImage image(32, 16, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.fillRect(QRect(0, 0, 16, 16), Qt::transparent);
    painter.fillRect(QRect(16, 0, 16, 16), QColor(230, 230, 230));
    painter.end();
    const auto result = SmartCollisionSuggester().suggest(image, QSize(2, 1), 3, 3, 0, 2, 3);
    const auto repeated = SmartCollisionSuggester().suggest(image, QSize(2, 1), 3, 3, 0, 2, 3);
    if (!result.valid || result.suggestions.size() != 2
        || result.suggestions.at(0).collision != 3
        || result.suggestions.at(1).collision != 0
        || result.suggestions.at(0).confidence <= result.suggestions.at(1).confidence
        || repeated.suggestions.at(0).collision != result.suggestions.at(0).collision
        || repeated.suggestions.at(1).elevation != result.suggestions.at(1).elevation) {
        std::cerr << "Smart collision suggestion test failed.\n";
        return 1;
    }
    const auto invalid = SmartCollisionSuggester().suggest(image, QSize(1, 1), 3, 3, 0, 2, 3);
    if (invalid.valid) {
        std::cerr << "Invalid dimensions should be rejected.\n";
        return 1;
    }
    const auto zeroMax = SmartCollisionSuggester().suggest(image, QSize(2, 1), 0, 0, 0, 0, -1);
    if (!zeroMax.valid || zeroMax.suggestions.at(0).collision != 0
        || zeroMax.suggestions.at(0).elevation != 0) {
        std::cerr << "Zero-width project fields should retain representable defaults.\n";
        return 1;
    }
    std::cout << "All smart collision suggestion tests passed.\n";
    return 0;
}