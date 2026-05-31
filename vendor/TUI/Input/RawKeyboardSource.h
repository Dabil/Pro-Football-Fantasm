#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "Input/KeyCodes.h"
#include "Input/MouseEvent.h"

namespace Input
{
    struct RawKeyEvent
    {
        std::uint16_t virtualKey = 0;
        char32_t character = U'\0';
        KeyModifiers modifiers;
        bool pressed = false;
        std::uint16_t repeatCount = 1;
    };

    using RawInputEvent = std::variant<RawKeyEvent, MouseEvent>;

    class IRawKeyboardSource
    {
    public:
        virtual ~IRawKeyboardSource() = default;

        virtual bool initialize() = 0;
        virtual void shutdown() = 0;

        virtual std::optional<RawInputEvent> pollRawInput() = 0;

        virtual std::optional<RawKeyEvent> pollRawKey()
        {
            while (true)
            {
                std::optional<RawInputEvent> event = pollRawInput();
                if (!event.has_value())
                {
                    return std::nullopt;
                }

                if (const RawKeyEvent* key = std::get_if<RawKeyEvent>(&event.value()))
                {
                    return *key;
                }
            }
        }
    };
}