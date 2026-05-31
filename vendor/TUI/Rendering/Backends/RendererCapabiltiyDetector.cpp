#include "Rendering/Backends/RendererCapabilityDetector.h"

RendererCapabilityProbeResult RendererCapabilityDetector::detectAndConfigure(
    HANDLE hOut,
    HANDLE hIn,
    DWORD originalOutputMode,
    DWORD originalInputMode,
    bool haveOriginalOutputMode,
    bool haveOriginalInputMode,
    bool requireVirtualTerminalOutput,
    bool enableVirtualTerminalInput)
{
    RendererCapabilityProbeResult result;

    bool virtualTerminalEnabled = false;

    if (!haveOriginalOutputMode)
    {
        return result;
    }

    DWORD outMode = originalOutputMode;
    outMode |= ENABLE_PROCESSED_OUTPUT;
    outMode |= ENABLE_WRAP_AT_EOL_OUTPUT;
    outMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    result.virtualTerminalEnableAttempted = true;

    if (SetConsoleMode(hOut, outMode))
    {
        virtualTerminalEnabled = true;
        result.virtualTerminalEnableSucceeded = true;
        result.virtualTerminalWasEnabled = true;
        result.configuredOutputMode = outMode;
        result.hasConfiguredOutputMode = true;
    }
    else
    {
        if (requireVirtualTerminalOutput)
        {
            return result;
        }

        DWORD fallbackMode = originalOutputMode;
        fallbackMode |= ENABLE_PROCESSED_OUTPUT;
        fallbackMode |= ENABLE_WRAP_AT_EOL_OUTPUT;

        if (!SetConsoleMode(hOut, fallbackMode))
        {
            return result;
        }

        result.configuredOutputMode = fallbackMode;
        result.hasConfiguredOutputMode = true;
    }

    if (haveOriginalInputMode)
    {
        DWORD inMode = originalInputMode;
        inMode |= ENABLE_PROCESSED_INPUT;
        inMode |= ENABLE_WINDOW_INPUT;
        inMode |= ENABLE_EXTENDED_FLAGS;
        inMode &= ~ENABLE_QUICK_EDIT_MODE;

        if (enableVirtualTerminalInput)
        {
            inMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
        }

        if (!SetConsoleMode(hIn, inMode))
        {
            return result;
        }

        result.configuredInputMode = inMode;
        result.hasConfiguredInputMode = true;
    }

    if (virtualTerminalEnabled)
    {
        result.virtualTerminalWasEnabled = true;
    }

    return result;
}