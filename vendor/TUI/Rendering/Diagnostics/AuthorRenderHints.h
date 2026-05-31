#pragma once

#include <string>
#include <vector>

#include "Rendering/Diagnostics/RenderDiagnostics.h"

/*
    Purpose:

    AuthorRenderHints converts structured renderer diagnostics into
    author-facing advisory messages.

    These hints do not affect rendering behavior.
    They only explain:
        - backend limitations
        - portability concerns
        - why visible output may differ from authored logical style
        - practical, conservative alternatives authors may choose

    This layer is intentionally diagnostics-only.
*/

class AuthorRenderHints
{
public:
    static std::vector<std::string> buildHints(const RenderDiagnostics& diagnostics);
};