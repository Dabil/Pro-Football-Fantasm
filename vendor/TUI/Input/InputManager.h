#pragma once

#include <memory>
#include <optional>
#include <vector>

#include "Input/Event.h"
#include "Input/KeyEvent.h"
#include "Input/RawKeyboardSource.h"

namespace Input
{
    class InputManager
    {
    public:
        InputManager();
        explicit InputManager(std::unique_ptr<IRawKeyboardSource> keyboardSource);
        ~InputManager();

        bool initialize();
        void shutdown();

        void poll();

        bool hasEvents() const;
        std::optional<Event> popEvent();
        std::vector<Event> drainEvents();

        std::optional<KeyEvent> popKeyEvent();
        std::vector<KeyEvent> drainKeyEvents();

        void clear();

    private:
        KeyEvent normalizeRawKey(const RawKeyEvent& raw) const;

    private:
        std::unique_ptr<IRawKeyboardSource> m_keyboardSource;
        std::vector<Event> m_events;
        bool m_initialized = false;
    };
}