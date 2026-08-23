#include "UI/Scrolling/ScrollBehavior.h"

#include <algorithm>

#include "Input/Command.h"
#include "Input/MouseEvent.h"

namespace UI::Scrolling
{
    bool scrollViewportBy(Viewport& viewport, int deltaX, int deltaY)
    {
        const Point before = viewport.scrollOffset();
        viewport.scrollBy(deltaX, deltaY);
        const Point after = viewport.scrollOffset();

        return before.x != after.x || before.y != after.y;
    }

    bool scrollViewportTo(Viewport& viewport, int x, int y)
    {
        const Point before = viewport.scrollOffset();
        viewport.scrollTo(x, y);
        const Point after = viewport.scrollOffset();

        return before.x != after.x || before.y != after.y;
    }

    bool handleKeyboardScrollEvent(
        const Input::Event& event,
        Viewport& viewport,
        const ScrollInputOptions& options)
    {
        const Input::CommandEvent* commandEvent = event.asCommand();
        if (commandEvent == nullptr)
        {
            return false;
        }

        const int lineAmount = std::max(1, options.lineAmount);
        const int pageHeight = std::max(1, viewport.viewSize().height);
        const int pageWidth = std::max(1, viewport.viewSize().width);

        switch (commandEvent->command.code)
        {
        case Input::CommandCode::MoveUp:
            return options.allowVertical
                && scrollViewportBy(viewport, 0, -lineAmount);

        case Input::CommandCode::MoveDown:
            return options.allowVertical
                && scrollViewportBy(viewport, 0, lineAmount);

        case Input::CommandCode::MoveLeft:
            return options.allowHorizontal
                && scrollViewportBy(viewport, -lineAmount, 0);

        case Input::CommandCode::MoveRight:
            return options.allowHorizontal
                && scrollViewportBy(viewport, lineAmount, 0);

        case Input::CommandCode::PageUp:
            return options.allowVertical
                && scrollViewportBy(viewport, 0, -pageHeight);

        case Input::CommandCode::PageDown:
            return options.allowVertical
                && scrollViewportBy(viewport, 0, pageHeight);

        case Input::CommandCode::MoveHome:
            return scrollViewportTo(
                viewport,
                options.allowHorizontal ? 0 : viewport.scrollX(),
                options.allowVertical ? 0 : viewport.scrollY());

        case Input::CommandCode::MoveEnd:
            return scrollViewportTo(
                viewport,
                options.allowHorizontal ? viewport.maxScrollX() : viewport.scrollX(),
                options.allowVertical ? viewport.maxScrollY() : viewport.scrollY());

        default:
            return false;
        }
    }

    bool handleMouseWheelScrollEvent(
        const Input::Event& event,
        Viewport& viewport,
        const Rect& hitBounds,
        bool focused,
        const ScrollInputOptions& options)
    {
        const Input::MouseEvent* mouseEvent = event.asMouse();
        if (mouseEvent == nullptr || !mouseEvent->isWheel())
        {
            return false;
        }

        const bool hovered = hitBounds.contains(
            mouseEvent->position.x,
            mouseEvent->position.y);

        if (!hovered && !focused)
        {
            return false;
        }

        if (!options.allowVertical)
        {
            return false;
        }

        const int wheelAmount = std::max(1, options.wheelAmount);

        if (mouseEvent->button == Input::MouseButton::WheelUp
            || mouseEvent->wheelDelta > 0)
        {
            return scrollViewportBy(viewport, 0, -wheelAmount);
        }

        if (mouseEvent->button == Input::MouseButton::WheelDown
            || mouseEvent->wheelDelta < 0)
        {
            return scrollViewportBy(viewport, 0, wheelAmount);
        }

        return false;
    }

    bool handleScrollEvent(
        const Input::Event& event,
        Viewport& viewport,
        const Rect& hitBounds,
        bool focused,
        const ScrollInputOptions& options)
    {
        if (handleMouseWheelScrollEvent(event, viewport, hitBounds, focused, options))
        {
            return true;
        }

        return handleKeyboardScrollEvent(event, viewport, options);
    }
}