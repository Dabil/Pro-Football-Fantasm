#pragma once

#include "Core/Rect.h"
#include "Input/Event.h"
#include "UI/Base/Viewport.h"

namespace UI::Scrolling
{
    struct ScrollInputOptions
    {
        bool allowHorizontal = true;
        bool allowVertical = true;
        int lineAmount = 1;
        int wheelAmount = 1;
    };

    bool scrollViewportBy(Viewport& viewport, int deltaX, int deltaY);
    bool scrollViewportTo(Viewport& viewport, int x, int y);

    bool handleKeyboardScrollEvent(
        const Input::Event& event,
        Viewport& viewport,
        const ScrollInputOptions& options = {});

    bool handleMouseWheelScrollEvent(
        const Input::Event& event,
        Viewport& viewport,
        const Rect& hitBounds,
        bool focused,
        const ScrollInputOptions& options = {});

    bool handleScrollEvent(
        const Input::Event& event,
        Viewport& viewport,
        const Rect& hitBounds,
        bool focused,
        const ScrollInputOptions& options = {});
}