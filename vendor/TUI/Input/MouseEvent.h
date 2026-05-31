#pragma once

#include "Core/Point.h"
#include "Input/KeyCodes.h"

namespace Input
{
    enum class MouseButton
    {
        None,
        Left,
        Right,
        Middle,
        WheelUp,
        WheelDown
    };

    enum class MouseAction
    {
        Moved,
        Pressed,
        Released,
        Dragged,
        Clicked,
        Wheel
    };

    struct MouseButtonState
    {
        bool left = false;
        bool right = false;
        bool middle = false;

        bool anyPressed() const
        {
            return left || right || middle;
        }
    };

    struct MouseEvent
    {
        Point position;
        MouseButton button = MouseButton::None;
        MouseAction action = MouseAction::Moved;
        MouseButtonState buttons;
        KeyModifiers modifiers;
        int wheelDelta = 0;
        int clickCount = 0;

        bool isPress() const
        {
            return action == MouseAction::Pressed;
        }

        bool isRelease() const
        {
            return action == MouseAction::Released;
        }

        bool isMove() const
        {
            return action == MouseAction::Moved;
        }

        bool isDrag() const
        {
            return action == MouseAction::Dragged;
        }

        bool isClick() const
        {
            return action == MouseAction::Clicked;
        }

        bool isWheel() const
        {
            return action == MouseAction::Wheel
                || button == MouseButton::WheelUp
                || button == MouseButton::WheelDown
                || wheelDelta != 0;
        }
    };
}