#pragma once

#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"

class TerminalHostClassifier
{
public:
    static TerminalHostKind classifyCurrentHost();
    static bool shouldPreferTerminalRenderer(TerminalHostKind hostKind);
};