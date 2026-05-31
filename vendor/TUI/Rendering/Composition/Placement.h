#pragma once

#include <string>

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Core/Size.h"
#include "Rendering/Composition/Alignment.h"

class TextObject;

namespace Composition
{
    struct NamedRegion
    {
        std::string name;
        Rect bounds;

        int x() const
        {
            return bounds.position.x;
        }

        int y() const
        {
            return bounds.position.y;
        }

        int width() const
        {
            return bounds.size.width;
        }

        int height() const
        {
            return bounds.size.height;
        }

        bool contains(int px, int py) const
        {
            return bounds.contains(px, py);
        }
    };

    enum class AnchorPoint
    {
        TopLeft,
        TopCenter,
        TopRight,
        CenterLeft,
        Center,
        CenterRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    struct PlacementRequest
    {
        Rect region;
        Size contentSize;
        Alignment alignment;
        bool clampToRegion = false;
    };

    struct PlacementResult
    {
        Point origin;
        Rect region;
        Size contentSize;
        Alignment alignment;
        bool clamped = false;
    };

    PlacementResult resolvePlacement(const PlacementRequest& request);

    Point resolveAlignedOrigin(
        const Rect& region,
        const Size& contentSize,
        HorizontalAlign horizontalAlign,
        VerticalAlign verticalAlign);

    Point resolveAlignedOrigin(
        const Rect& region,
        const Size& contentSize,
        const Alignment& alignment);

    Rect makeRect(int x, int y, int width, int height);
    Size measureObject(const TextObject& object);

    int centerX(const Rect& region, int contentWidth);
    int centerY(const Rect& region, int contentHeight);
    Point centerInRegion(const Rect& region, const Size& contentSize);

    Point anchorPointInRegion(const Rect& region, AnchorPoint anchor);

    PlacementRequest makePlacementRequest(
        const Rect& region,
        const Size& contentSize,
        const Alignment& alignment,
        bool clampToRegion = false);
}