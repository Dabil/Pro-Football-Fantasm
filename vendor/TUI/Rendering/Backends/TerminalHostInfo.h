#pragma once

#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"

/*
    Purpose:

    TerminalHostInfo is a small, explicit host-side model used during
    startup selection.

    It centralizes:
        - detected host classification
        - whether stdout is a character device or redirected
        - whether Windows Terminal session hints are present
        - whether the current host should prefer TerminalRenderer

    This keeps host identification logic separate from renderer logic.
*/

struct TerminalHostInfo
{
    TerminalHostKind hostKind = TerminalHostKind::Unknown;

    bool stdOutputIsCharacterDevice = false;
    bool isRedirectedOrPipe = false;

    bool windowsTerminalSessionHint = false;

    bool prefersTerminalRenderer = false;
};

class TerminalHostInfoDetector
{
public:
    static TerminalHostInfo detectCurrentHostInfo();
    static bool shouldPreferTerminalRenderer(const TerminalHostInfo& hostInfo);
};