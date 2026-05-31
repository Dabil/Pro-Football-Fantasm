#pragma once

namespace Input
{
    enum class CommandCode
    {
        None,

        MoveUp,
        MoveDown,
        MoveLeft,
        MoveRight,

        MoveHome,
        MoveEnd,
        PageUp,
        PageDown,

        Confirm,
        Cancel,

        NextFocus,
        PreviousFocus,

        OpenHelp,
        Close,
        Quit,
        Refresh,

        Back,
        Forward
    };

    struct Command
    {
        CommandCode code = CommandCode::None;
    };

    inline bool isNavigationCommand(CommandCode code)
    {
        switch (code)
        {
        case CommandCode::MoveUp:
        case CommandCode::MoveDown:
        case CommandCode::MoveLeft:
        case CommandCode::MoveRight:
        case CommandCode::MoveHome:
        case CommandCode::MoveEnd:
        case CommandCode::PageUp:
        case CommandCode::PageDown:
            return true;

        default:
            return false;
        }
    }

    inline bool isFocusCommand(CommandCode code)
    {
        return code == CommandCode::NextFocus
            || code == CommandCode::PreviousFocus;
    }

    inline bool isApplicationCommand(CommandCode code)
    {
        switch (code)
        {
        case CommandCode::OpenHelp:
        case CommandCode::Close:
        case CommandCode::Quit:
        case CommandCode::Refresh:
        case CommandCode::Back:
        case CommandCode::Forward:
            return true;

        default:
            return false;
        }
    }

    inline bool isActionCommand(CommandCode code)
    {
        return code == CommandCode::Confirm
            || code == CommandCode::Cancel;
    }

    inline bool isValidCommand(CommandCode code)
    {
        return code != CommandCode::None;
    }
}