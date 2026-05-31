#pragma once

#include <optional>
#include <vector>

#include "Input/Command.h"
#include "Input/KeyEvent.h"

namespace Input
{
    struct KeyBinding
    {
        KeyCode code = KeyCode::Unknown;
        char32_t character = U'\0';
        KeyModifiers modifiers;
    };

    class CommandMap
    {
    public:
        CommandMap() = default;

        static CommandMap createDefault();

        void clear();

        void bind(const KeyBinding& binding, Command command);
        void bind(const KeyBinding& binding, CommandCode commandCode);

        void bindKey(KeyCode keyCode, CommandCode commandCode);
        void bindKey(KeyCode keyCode, const KeyModifiers& modifiers, CommandCode commandCode);

        void bindCharacter(char32_t character, CommandCode commandCode);
        void bindCharacter(char32_t character, const KeyModifiers& modifiers, CommandCode commandCode);

        bool unbind(const KeyBinding& binding);
        bool hasBinding(const KeyBinding& binding) const;

        std::optional<Command> map(const KeyEvent& keyEvent) const;

    private:
        struct Entry
        {
            KeyBinding binding;
            Command command;
        };

        static bool modifiersEqual(const KeyModifiers& left, const KeyModifiers& right);
        static bool bindingsEqual(const KeyBinding& left, const KeyBinding& right);
        static KeyBinding bindingFromEvent(const KeyEvent& keyEvent);

    private:
        std::vector<Entry> m_entries;
    };
}