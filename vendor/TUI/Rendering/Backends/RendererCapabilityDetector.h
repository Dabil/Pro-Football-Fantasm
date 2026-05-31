#pragma once

#define NOMINMAX
#include <windows.h>

/*
    Purpose:

    RendererCapabilityDetector contains the shared Win32 backend probing
    and console mode configuration used by multiple renderer-specific
    capability detectors.

    It does NOT decide which capability profile should be reported.
    It only performs the common host probing/configuration work and
    returns the raw results.

    Renderer-specific detectors remain responsible for interpreting
    the probe result conservatively according to the active backend path.
*/

struct RendererCapabilityProbeResult
{
    bool virtualTerminalWasEnabled = false;
    bool virtualTerminalEnableAttempted = false;
    bool virtualTerminalEnableSucceeded = false;

    DWORD configuredOutputMode = 0;
    DWORD configuredInputMode = 0;

    bool hasConfiguredOutputMode = false;
    bool hasConfiguredInputMode = false;
};

class RendererCapabilityDetector
{
public:
    static RendererCapabilityProbeResult detectAndConfigure(
        HANDLE hOut,
        HANDLE hIn,
        DWORD originalOutputMode,
        DWORD originalInputMode,
        bool haveOriginalOutputMode,
        bool haveOriginalInputMode,
        bool requireVirtualTerminalOutput,
        bool enableVirtualTerminalInput);
};