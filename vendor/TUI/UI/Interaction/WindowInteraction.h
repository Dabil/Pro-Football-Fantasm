#pragma once

#include "Core/Point.h"
#include "Core/Rect.h"

class Window;

namespace UI
{
    enum class PointerButton
    {
        None,
        Primary,
        Secondary,
        Middle
    };

    enum class PointerAction
    {
        Move,
        Press,
        Release,
        Drag,
        Cancel
    };

    enum class CursorRegion
    {
        Outside,
        Client,
        TitleBar,
        LeftEdge,
        RightEdge,
        TopEdge,
        BottomEdge,
        TopLeftCorner,
        TopRightCorner,
        BottomLeftCorner,
        BottomRightCorner
    };

    struct PointerEvent
    {
        Point position;
        PointerButton button = PointerButton::None;
        PointerAction action = PointerAction::Move;
        int clickCount = 0;
    };

    struct WindowHitTestResult
    {
        Window* window = nullptr;
        CursorRegion region = CursorRegion::Outside;
        Point screenPosition;
        Point localPosition;

        bool hit() const;
    };

    struct PointerCaptureState
    {
        Window* window = nullptr;
        PointerButton button = PointerButton::None;
        Point origin;
        Point current;

        bool active() const;
        void clear();
    };

    struct WindowDragState
    {
        Window* window = nullptr;
        Point pointerOrigin;
        Point currentPointer;
        Rect originalBounds;

        bool active() const;
        void clear();
    };

    struct WindowResizeState
    {
        Window* window = nullptr;
        CursorRegion region = CursorRegion::Outside;
        Point pointerOrigin;
        Point currentPointer;
        Rect originalBounds;
        Size minimumSize{ 4, 3 };

        bool active() const;
        void clear();
    };

    bool isResizeRegion(CursorRegion region);
    bool isEdgeRegion(CursorRegion region);
    bool isCornerRegion(CursorRegion region);
    bool isInsideWindowRegion(CursorRegion region);

    CursorRegion hitTestWindowBounds(
        const Rect& bounds,
        Point screenPosition,
        int resizeBorderThickness = 1,
        int titleBarHeight = 1);

    Rect resizedBounds(
        const Rect& originalBounds,
        CursorRegion resizeRegion,
        Point pointerOrigin,
        Point currentPointer,
        Size minimumSize);
}