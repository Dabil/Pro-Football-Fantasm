#pragma once

#include <cstdint>

#include "Input/KeyCodes.h"

namespace Input
{
    struct KeyEvent
    {
        KeyCode code = KeyCode::Unknown;
        char32_t character = U'\0';
        KeyModifiers modifiers;
        bool pressed = false;
        std::uint16_t repeatCount = 1;
    };
}