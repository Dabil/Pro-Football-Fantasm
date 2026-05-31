#pragma once

#include <string>

#include "App/StartupConfig.h"
#include "Rendering/Diagnostics/RendererSelectionTrace.h"

enum class TerminalHostKind
{
    Unknown = 0,
    ClassicConsoleWindow,
    WindowsTerminal,
    VsCodeIntegratedTerminal,
    ConEmu,
    Mintty,
    RedirectedOrPipe,
    OtherTerminalHost
};

enum class RendererKind
{
    Unknown = 0,
    ConsoleRenderer,
    TerminalRenderer
};

struct StartupDiagnosticsContext
{
    StartupHostPreference configuredHostPreference = StartupHostPreference::Auto;
    StartupRendererPreference configuredRendererPreference = StartupRendererPreference::Auto;

    bool startupConfigFileFound = false;
    std::string startupConfigSource = "startup.config";

    TerminalHostKind requestedHost = TerminalHostKind::Unknown;
    TerminalHostKind actualHost = TerminalHostKind::Unknown;

    RendererKind requestedRenderer = RendererKind::Unknown;
    RendererKind actualRenderer = RendererKind::Unknown;

    bool relaunchAttempted = false;
    bool relaunchPerformed = false;

    bool launchedByWindowsTerminalFlag = false;
    bool windowsTerminalSessionHint = false;

    RendererSelectionTrace selectionTrace{};
};

inline const char* toString(TerminalHostKind hostKind)
{
    switch (hostKind)
    {
    case TerminalHostKind::ClassicConsoleWindow:
        return "ClassicConsoleWindow";

    case TerminalHostKind::WindowsTerminal:
        return "WindowsTerminal";

    case TerminalHostKind::VsCodeIntegratedTerminal:
        return "VsCodeIntegratedTerminal";

    case TerminalHostKind::ConEmu:
        return "ConEmu";

    case TerminalHostKind::Mintty:
        return "Mintty";

    case TerminalHostKind::RedirectedOrPipe:
        return "RedirectedOrPipe";

    case TerminalHostKind::OtherTerminalHost:
        return "OtherTerminalHost";

    case TerminalHostKind::Unknown:
    default:
        return "Unknown";
    }
}

inline const char* toString(RendererKind rendererKind)
{
    switch (rendererKind)
    {
    case RendererKind::ConsoleRenderer:
        return "ConsoleRenderer";

    case RendererKind::TerminalRenderer:
        return "TerminalRenderer";

    case RendererKind::Unknown:
    default:
        return "Unknown";
    }
}

inline const char* toString(StartupHostPreference preference)
{
    switch (preference)
    {
    case StartupHostPreference::Console:
        return "Console";

    case StartupHostPreference::Terminal:
        return "Terminal";

    case StartupHostPreference::Auto:
    default:
        return "Auto";
    }
}

inline const char* toString(StartupRendererPreference preference)
{
    switch (preference)
    {
    case StartupRendererPreference::Console:
        return "Console";

    case StartupRendererPreference::Terminal:
        return "Terminal";

    case StartupRendererPreference::Auto:
    default:
        return "Auto";
    }
}