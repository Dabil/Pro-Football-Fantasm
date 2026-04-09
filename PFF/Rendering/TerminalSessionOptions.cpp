#include "Rendering/TerminalSessionOptions.h"

TerminalSessionOptions TerminalSessionOptions::FullScreenDefaults()
{
    TerminalSessionOptions options;
    options.useAlternateScreen = true;
    options.hideCursor = true;
    options.clearScreenOnEnter = true;
    options.homeCursorOnEnter = true;

    options.resetStyleOnExit = true;
    options.restoreCursorVisibilityOnExit = true;
    options.clearScreenOnExit = true;
    options.homeCursorOnExit = true;

    return options;
}

TerminalSessionOptions TerminalSessionOptions::InlineDefaults()
{
    TerminalSessionOptions options;
    options.useAlternateScreen = false;
    options.hideCursor = false;
    options.clearScreenOnEnter = false;
    options.homeCursorOnEnter = false;

    options.resetStyleOnExit = true;
    options.restoreCursorVisibilityOnExit = false;
    options.clearScreenOnExit = false;
    options.homeCursorOnExit = false;

    return options;
}