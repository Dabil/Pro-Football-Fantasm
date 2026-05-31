#pragma once

#include "Rendering/Capabilities/RendererCapabilities.h"

#define NOMINMAX
#include <windows.h>

/*
    Purpose:

    TerminalCapabilityDetector is the terminal-sequence backend capability probe.

    It is responsible for:
        - attempting to enable virtual terminal processing on the active output
        - configuring compatible console input/output modes
        - describing the active renderer path as a true VT/terminal path only
          when VT output is actually available
        - keeping backend activation logic out of ScreenBuffer/theme code

    Important:
        Unlike ConsoleCapabilityDetector, this detector is intended for a
        renderer that will physically emit terminal control sequences.
        Therefore VT availability is not just advisory here; it is a required
        backend contract for TerminalRenderer.
*/

struct TerminalCapabilityDetectionResult
{
    RendererCapabilities capabilities = RendererCapabilities::Conservative();

    bool terminalOutputReady = false;

    bool virtualTerminalWasEnabled = false;
    bool virtualTerminalEnableAttempted = false;
    bool virtualTerminalEnableSucceeded = false;

    DWORD configuredOutputMode = 0;
    DWORD configuredInputMode = 0;

    bool hasConfiguredOutputMode = false;
    bool hasConfiguredInputMode = false;
};

class TerminalCapabilityDetector
{
public:
    static TerminalCapabilityDetectionResult detectAndConfigure(
        HANDLE hOut,
        HANDLE hIn,
        DWORD originalOutputMode,
        DWORD originalInputMode,
        bool haveOriginalOutputMode,
        bool haveOriginalInputMode);
};