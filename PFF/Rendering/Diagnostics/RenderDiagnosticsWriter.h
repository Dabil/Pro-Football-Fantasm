#pragma once

#include "Rendering/Diagnostics/RenderDiagnostics.h"

/*
    Purpose:

    RenderDiagnosticsWriter serializes RenderDiagnostics to a readable text file.

    It is intentionally simple:
        - no console output
        - no rendering logic
        - no mutation of renderer policy
        - stable text format suitable for developer and author inspection
*/

class RenderDiagnosticsWriter
{
public:
    static bool write(const RenderDiagnostics& diagnostics);
};