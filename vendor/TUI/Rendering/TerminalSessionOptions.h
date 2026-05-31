#pragma once

struct TerminalSessionOptions
{
    bool useAlternateScreen = true;
    bool hideCursor = true;
    bool clearScreenOnEnter = true;
    bool homeCursorOnEnter = true;

    bool resetStyleOnExit = true;
    bool restoreCursorVisibilityOnExit = true;
    bool clearScreenOnExit = true;
    bool homeCursorOnExit = true;

    static TerminalSessionOptions FullScreenDefaults();
    static TerminalSessionOptions InlineDefaults();
};