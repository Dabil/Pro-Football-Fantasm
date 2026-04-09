#include "Rendering/Backends/ConsoleCapabilityDetector.h"

#include "Rendering/Backends/RendererCapabilityDetector.h"

namespace
{
    RendererCapabilities buildConservativeCapabilities(bool virtualTerminalEnabled)
    {
        /*
            Important architectural choice:

            The current ConsoleRenderer still renders through Win32 console
            attributes via SetConsoleTextAttribute, not through VT escape output.

            Therefore, even if VT processing can be enabled, the active output
            path should still be described conservatively as BasicWin32 until a
            true VT presentation path is added later.

            In particular, VT enablement alone must not be treated as evidence
            that the current renderer can defensibly promise richer direct style
            semantics. The capability object should describe the active render
            path, not the broadest mode bit that happened to be enabled.
        */
        RendererCapabilities capabilities = RendererCapabilities::BasicWin32();
        capabilities.virtualTerminalProcessing = virtualTerminalEnabled;
        return capabilities;
    }
}

ConsoleCapabilityDetectionResult ConsoleCapabilityDetector::detectAndConfigure(
    HANDLE hOut,
    HANDLE hIn,
    DWORD originalOutputMode,
    DWORD originalInputMode,
    bool haveOriginalOutputMode,
    bool haveOriginalInputMode)
{
    ConsoleCapabilityDetectionResult result;

    const RendererCapabilityProbeResult probe =
        RendererCapabilityDetector::detectAndConfigure(
            hOut,
            hIn,
            originalOutputMode,
            originalInputMode,
            haveOriginalOutputMode,
            haveOriginalInputMode,
            false,
            false);

    result.virtualTerminalWasEnabled = probe.virtualTerminalWasEnabled;
    result.virtualTerminalEnableAttempted = probe.virtualTerminalEnableAttempted;
    result.virtualTerminalEnableSucceeded = probe.virtualTerminalEnableSucceeded;
    result.configuredOutputMode = probe.configuredOutputMode;
    result.configuredInputMode = probe.configuredInputMode;
    result.hasConfiguredOutputMode = probe.hasConfiguredOutputMode;
    result.hasConfiguredInputMode = probe.hasConfiguredInputMode;

    result.capabilities = buildConservativeCapabilities(probe.virtualTerminalWasEnabled);
    return result;
}