#pragma once

#include <QImage>
#include <QList>
#include <QSize>
#include <QString>

namespace Studio {

struct CollisionSuggestion
{
    int x = 0;
    int y = 0;
    uint16_t collision = 0;
    uint16_t elevation = 0;
    int confidence = 0;
    QString rationale;
};

struct CollisionSuggestionResult
{
    bool valid = false;
    QSize mapSize;
    QList<CollisionSuggestion> suggestions;
    QString error;
    int blockedCount = 0;
    int uncertainCount = 0;
};

class SmartCollisionSuggester
{
public:
    CollisionSuggestionResult suggest(
        const QImage &terrain,
        const QSize &mapSize,
        uint16_t maxCollision,
        uint16_t maxElevation,
        uint16_t defaultCollision,
        uint16_t defaultElevation,
        int blockedCollision) const;
};

} // namespace Studio