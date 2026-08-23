#include "UI/Scrolling/Scrollbar.h"

#include <algorithm>

#include "Rendering/ScreenBuffer.h"

namespace UI::Scrolling
{
    bool shouldShowVerticalScrollbar(const Viewport& viewport)
    {
        return viewport.maxScrollY() > 0
            && viewport.viewSize().height > 0;
    }

    Rect verticalScrollbarBoundsForContentBounds(
        const Rect& contentBounds,
        bool reserveColumn)
    {
        if (!reserveColumn || contentBounds.size.width <= 0 || contentBounds.size.height <= 0)
        {
            return Rect{ contentBounds.position, Size{ 0, 0 } };
        }

        return Rect{
            Point{
                contentBounds.position.x + contentBounds.size.width - 1,
                contentBounds.position.y
            },
            Size{ 1, contentBounds.size.height }
        };
    }

    Rect viewportBoundsForContentBounds(
        const Rect& contentBounds,
        bool verticalScrollbarVisible,
        bool reserveColumn)
    {
        if (!verticalScrollbarVisible || !reserveColumn || contentBounds.size.width <= 1)
        {
            return contentBounds;
        }

        return Rect{
            contentBounds.position,
            Size{ contentBounds.size.width - 1, contentBounds.size.height }
        };
    }

    void drawVerticalScrollbar(
        Surface& surface,
        const Rect& scrollbarBounds,
        const Viewport& viewport,
        const VerticalScrollbarStyle& style)
    {
        if (scrollbarBounds.size.width <= 0 || scrollbarBounds.size.height <= 0)
        {
            return;
        }

        ScreenBuffer& buffer = surface.buffer();

        for (int y = 0; y < scrollbarBounds.size.height; ++y)
        {
            buffer.writeCodePoint(
                scrollbarBounds.position.x,
                scrollbarBounds.position.y + y,
                style.glyphs.track,
                style.trackStyle);
        }

        const int trackHeight = scrollbarBounds.size.height;
        const int contentHeight = std::max(1, viewport.contentSize().height);
        const int viewHeight = std::max(1, viewport.viewSize().height);

        const int thumbHeight = std::max(
            1,
            (trackHeight * viewHeight) / contentHeight);

        const int maxThumbTop = std::max(0, trackHeight - thumbHeight);
        const int maxScrollY = std::max(1, viewport.maxScrollY());

        const int thumbTop = viewport.maxScrollY() <= 0
            ? 0
            : (viewport.scrollY() * maxThumbTop) / maxScrollY;

        for (int y = 0; y < thumbHeight; ++y)
        {
            const int drawY = scrollbarBounds.position.y + thumbTop + y;

            buffer.writeCodePoint(
                scrollbarBounds.position.x,
                drawY,
                style.glyphs.thumb,
                style.thumbStyle);
        }
    }
}