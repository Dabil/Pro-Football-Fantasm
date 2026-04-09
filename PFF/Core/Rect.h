#pragma once

#include "Core/Point.h"
#include "Core/Size.h"

struct Rect
{
    Point position;
    Size size;

    bool contains(int px, int py) const
    {
        return px >= position.x && px < (position.x + size.width)
            && py >= position.y && py < (position.y + size.height);
    }

    bool intersects(const Rect& other) const
    {
        return !(position.x + size.width <= other.position.x
            || other.position.x + other.size.width <= position.x
            || position.y + size.height <= other.position.y
            || other.position.y + other.size.height <= position.y);
    }
};