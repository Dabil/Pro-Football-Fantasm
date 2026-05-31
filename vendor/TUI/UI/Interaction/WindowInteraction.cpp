#include "UI/Interaction/WindowInteraction.h"

#include <algorithm>

namespace
{
    int rightExclusive(const Rect& rect)
    {
        return rect.position.x + rect.size.width;
    }

    int bottomExclusive(const Rect& rect)
    {
        return rect.position.y + rect.size.height;
    }
}

namespace UI
{
    bool WindowHitTestResult::hit() const
    {
        return window != nullptr && region != CursorRegion::Outside;
    }

    bool PointerCaptureState::active() const
    {
        return window != nullptr;
    }

    void PointerCaptureState::clear()
    {
        window = nullptr;
        button = PointerButton::None;
        origin = Point{};
        current = Point{};
    }

    bool WindowDragState::active() const
    {
        return window != nullptr;
    }

    void WindowDragState::clear()
    {
        window = nullptr;
        pointerOrigin = Point{};
        currentPointer = Point{};
        originalBounds = Rect{};
    }

    bool WindowResizeState::active() const
    {
        return window != nullptr;
    }

    void WindowResizeState::clear()
    {
        window = nullptr;
        region = CursorRegion::Outside;
        pointerOrigin = Point{};
        currentPointer = Point{};
        originalBounds = Rect{};
        minimumSize = Size{ 4, 3 };
    }

    bool isResizeRegion(CursorRegion region)
    {
        switch (region)
        {
        case CursorRegion::LeftEdge:
        case CursorRegion::RightEdge:
        case CursorRegion::TopEdge:
        case CursorRegion::BottomEdge:
        case CursorRegion::TopLeftCorner:
        case CursorRegion::TopRightCorner:
        case CursorRegion::BottomLeftCorner:
        case CursorRegion::BottomRightCorner:
            return true;

        default:
            return false;
        }
    }

    bool isEdgeRegion(CursorRegion region)
    {
        switch (region)
        {
        case CursorRegion::LeftEdge:
        case CursorRegion::RightEdge:
        case CursorRegion::TopEdge:
        case CursorRegion::BottomEdge:
            return true;

        default:
            return false;
        }
    }

    bool isCornerRegion(CursorRegion region)
    {
        switch (region)
        {
        case CursorRegion::TopLeftCorner:
        case CursorRegion::TopRightCorner:
        case CursorRegion::BottomLeftCorner:
        case CursorRegion::BottomRightCorner:
            return true;

        default:
            return false;
        }
    }

    bool isInsideWindowRegion(CursorRegion region)
    {
        return region != CursorRegion::Outside;
    }

    CursorRegion hitTestWindowBounds(
        const Rect& bounds,
        Point screenPosition,
        int resizeBorderThickness,
        int titleBarHeight)
    {
        if (bounds.size.width <= 0 || bounds.size.height <= 0)
        {
            return CursorRegion::Outside;
        }

        if (!bounds.contains(screenPosition.x, screenPosition.y))
        {
            return CursorRegion::Outside;
        }

        resizeBorderThickness = std::max(0, resizeBorderThickness);
        titleBarHeight = std::max(0, titleBarHeight);

        const int left = bounds.position.x;
        const int top = bounds.position.y;
        const int right = rightExclusive(bounds) - 1;
        const int bottom = bottomExclusive(bounds) - 1;

        const bool onLeft = resizeBorderThickness > 0
            && screenPosition.x < left + resizeBorderThickness;
        const bool onRight = resizeBorderThickness > 0
            && screenPosition.x > right - resizeBorderThickness;
        const bool onTop = resizeBorderThickness > 0
            && screenPosition.y < top + resizeBorderThickness;
        const bool onBottom = resizeBorderThickness > 0
            && screenPosition.y > bottom - resizeBorderThickness;

        if (onTop && onLeft)
        {
            return CursorRegion::TopLeftCorner;
        }

        if (onTop && onRight)
        {
            return CursorRegion::TopRightCorner;
        }

        if (onBottom && onLeft)
        {
            return CursorRegion::BottomLeftCorner;
        }

        if (onBottom && onRight)
        {
            return CursorRegion::BottomRightCorner;
        }

        if (onLeft)
        {
            return CursorRegion::LeftEdge;
        }

        if (onRight)
        {
            return CursorRegion::RightEdge;
        }

        if (onTop)
        {
            return CursorRegion::TopEdge;
        }

        if (onBottom)
        {
            return CursorRegion::BottomEdge;
        }

        if (titleBarHeight > 0 && screenPosition.y < top + titleBarHeight)
        {
            return CursorRegion::TitleBar;
        }

        return CursorRegion::Client;
    }

    Rect resizedBounds(
        const Rect& originalBounds,
        CursorRegion resizeRegion,
        Point pointerOrigin,
        Point currentPointer,
        Size minimumSize)
    {
        const int deltaX = currentPointer.x - pointerOrigin.x;
        const int deltaY = currentPointer.y - pointerOrigin.y;

        const int originalRight = originalBounds.position.x + originalBounds.size.width;
        const int originalBottom = originalBounds.position.y + originalBounds.size.height;

        Rect result = originalBounds;

        minimumSize.width = std::max(1, minimumSize.width);
        minimumSize.height = std::max(1, minimumSize.height);

        switch (resizeRegion)
        {
        case CursorRegion::LeftEdge:
        case CursorRegion::TopLeftCorner:
        case CursorRegion::BottomLeftCorner:
        {
            const int maximumLeft = originalRight - minimumSize.width;
            const int requestedLeft = originalBounds.position.x + deltaX;
            result.position.x = std::max(0, std::min(requestedLeft, maximumLeft));
            result.size.width = originalRight - result.position.x;
            break;
        }

        case CursorRegion::RightEdge:
        case CursorRegion::TopRightCorner:
        case CursorRegion::BottomRightCorner:
            result.size.width = std::max(
                minimumSize.width,
                originalBounds.size.width + deltaX);
            break;

        default:
            break;
        }

        switch (resizeRegion)
        {
        case CursorRegion::TopEdge:
        case CursorRegion::TopLeftCorner:
        case CursorRegion::TopRightCorner:
        {
            const int maximumTop = originalBottom - minimumSize.height;
            const int requestedTop = originalBounds.position.y + deltaY;
            result.position.y = std::max(0, std::min(requestedTop, maximumTop));
            result.size.height = originalBottom - result.position.y;
            break;
        }

        case CursorRegion::BottomEdge:
        case CursorRegion::BottomLeftCorner:
        case CursorRegion::BottomRightCorner:
            result.size.height = std::max(
                minimumSize.height,
                originalBounds.size.height + deltaY);
            break;

        default:
            break;
        }

        return result;
    }
}