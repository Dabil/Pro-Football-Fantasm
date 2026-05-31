#include "Input/CommandMap.h"

/*
    Dafault shared bindings:
        Arrow keys       -> MoveUp / MoveDown / MoveLeft / MoveRight
        Home / End       -> MoveHome / MoveEnd
        PageUp/PageDown  -> PageUp / PageDown
        Enter / Space    -> Confirm
        Escape           -> Cancel
        Tab              -> NextFocus
        Shift+Tab        -> PreviousFocus
        Backtab          -> PreviousFocus
        F1               -> OpenHelp
        F5               -> Refresh
        Ctrl+Q           -> Quit
        Ctrl+R           -> Refresh
*/

namespace Input
{
    namespace
    {
        KeyModifiers noModifiers()
        {
            return KeyModifiers{};
        }

        KeyModifiers shiftModifier()
        {
            KeyModifiers modifiers;
            modifiers.shift = true;
            return modifiers;
        }

        KeyModifiers ctrlModifier()
        {
            KeyModifiers modifiers;
            modifiers.ctrl = true;
            return modifiers;
        }
    }

    // Remember than command codes will overwrite previous behavior. 
    // so you can only have each KeyCode defined once. 
    // Because if you redefine, it will just overwrite the 1st binding
    CommandMap CommandMap::createDefault()
    {
        CommandMap map;

        map.bindKey(KeyCode::Up, CommandCode::MoveUp);
        map.bindKey(KeyCode::Down, CommandCode::MoveDown);
        map.bindKey(KeyCode::Left, CommandCode::MoveLeft);
        map.bindKey(KeyCode::Right, CommandCode::MoveRight);

        map.bindKey(KeyCode::Home, CommandCode::MoveHome);
        map.bindKey(KeyCode::End, CommandCode::MoveEnd);
        map.bindKey(KeyCode::PageUp, CommandCode::PageUp);
        map.bindKey(KeyCode::PageDown, CommandCode::PageDown);

        map.bindKey(KeyCode::Enter, CommandCode::Confirm);
        map.bindKey(KeyCode::Space, CommandCode::Confirm);
        map.bindKey(KeyCode::Escape, CommandCode::Cancel);

        map.bindKey(KeyCode::Tab, CommandCode::NextFocus);

        // Different terminals/input paths may report Shift+Tab as either:
        // - Tab + shift modifier
        // - Backtab with no modifier
        // - Backtab + shift modifier
        //
        // Bind all supported forms so reverse focus traversal works reliably.
        map.bindKey(KeyCode::Tab, shiftModifier(), CommandCode::PreviousFocus);
        map.bindKey(KeyCode::Backtab, CommandCode::PreviousFocus);
        map.bindKey(KeyCode::Backtab, shiftModifier(), CommandCode::PreviousFocus);

        map.bindKey(KeyCode::F1, CommandCode::OpenHelp);
        map.bindKey(KeyCode::F5, CommandCode::Refresh);

        map.bindCharacter(U'q', ctrlModifier(), CommandCode::Quit);
        map.bindCharacter(U'r', ctrlModifier(), CommandCode::Refresh);

        return map;
    }

    void CommandMap::clear()
    {
        m_entries.clear();
    }

    void CommandMap::bind(const KeyBinding& binding, Command command)
    {
        for (Entry& entry : m_entries)
        {
            if (bindingsEqual(entry.binding, binding))
            {
                entry.command = command;
                return;
            }
        }

        Entry entry;
        entry.binding = binding;
        entry.command = command;
        m_entries.push_back(entry);
    }

    void CommandMap::bind(const KeyBinding& binding, CommandCode commandCode)
    {
        Command command;
        command.code = commandCode;
        bind(binding, command);
    }

    void CommandMap::bindKey(KeyCode keyCode, CommandCode commandCode)
    {
        bindKey(keyCode, noModifiers(), commandCode);
    }

    void CommandMap::bindKey(KeyCode keyCode, const KeyModifiers& modifiers, CommandCode commandCode)
    {
        KeyBinding binding;
        binding.code = keyCode;
        binding.character = U'\0';
        binding.modifiers = modifiers;

        bind(binding, commandCode);
    }

    void CommandMap::bindCharacter(char32_t character, CommandCode commandCode)
    {
        bindCharacter(character, noModifiers(), commandCode);
    }

    void CommandMap::bindCharacter(char32_t character, const KeyModifiers& modifiers, CommandCode commandCode)
    {
        KeyBinding binding;
        binding.code = KeyCode::Character;
        binding.character = character;
        binding.modifiers = modifiers;

        bind(binding, commandCode);
    }

    bool CommandMap::unbind(const KeyBinding& binding)
    {
        for (auto it = m_entries.begin(); it != m_entries.end(); ++it)
        {
            if (bindingsEqual(it->binding, binding))
            {
                m_entries.erase(it);
                return true;
            }
        }

        return false;
    }

    bool CommandMap::hasBinding(const KeyBinding& binding) const
    {
        for (const Entry& entry : m_entries)
        {
            if (bindingsEqual(entry.binding, binding))
            {
                return true;
            }
        }

        return false;
    }

    std::optional<Command> CommandMap::map(const KeyEvent& keyEvent) const
    {
        if (!keyEvent.pressed)
        {
            return std::nullopt;
        }

        const KeyBinding binding = bindingFromEvent(keyEvent);

        for (const Entry& entry : m_entries)
        {
            if (bindingsEqual(entry.binding, binding))
            {
                if (isValidCommand(entry.command.code))
                {
                    return entry.command;
                }

                return std::nullopt;
            }
        }

        return std::nullopt;
    }

    bool CommandMap::modifiersEqual(const KeyModifiers& left, const KeyModifiers& right)
    {
        return left.shift == right.shift
            && left.ctrl == right.ctrl
            && left.alt == right.alt;
    }

    bool CommandMap::bindingsEqual(const KeyBinding& left, const KeyBinding& right)
    {
        return left.code == right.code
            && left.character == right.character
            && modifiersEqual(left.modifiers, right.modifiers);
    }

    KeyBinding CommandMap::bindingFromEvent(const KeyEvent& keyEvent)
    {
        KeyBinding binding;
        binding.code = keyEvent.code;
        binding.character = keyEvent.code == KeyCode::Character ? keyEvent.character : U'\0';
        binding.modifiers = keyEvent.modifiers;

        return binding;
    }
}