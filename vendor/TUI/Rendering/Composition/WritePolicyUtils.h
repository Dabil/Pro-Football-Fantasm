#pragma once

#include "Rendering/Composition/WritePolicy.h"
#include "Rendering/Objects/TextObject.h"

namespace Composition::WritePolicyUtils
{
    bool isSourceLeadingCell(const TextObjectCell& cell);
    bool sourceMaskAllows(const TextObjectCell& cell, SourceMask mask);
    bool glyphPolicyAllows(const TextObjectCell& cell, GlyphPolicy policy);

    bool sourceCellParticipates(const TextObjectCell& cell, const WritePolicy& policy);
    bool sourceCellCanWriteGlyph(const TextObjectCell& cell, const WritePolicy& policy);

    TextObjectCell resolveGlyphWriteCell(const TextObjectCell& sourceCell, const WritePolicy& policy);

    bool isBuilderCompatible(const WritePolicy& policy);
}