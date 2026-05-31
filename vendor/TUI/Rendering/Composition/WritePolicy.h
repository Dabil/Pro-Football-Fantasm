#pragma once

#include "Rendering/Composition/DepthPolicy.h"
#include "Rendering/Composition/GlyphPolicy.h"
#include "Rendering/Composition/OverwriteRule.h"
#include "Rendering/Composition/SourceMask.h"
#include "Rendering/Composition/StylePolicy.h"

namespace Composition
{
    struct WritePolicy
    {
        GlyphPolicy glyphPolicy = GlyphPolicy::SolidObject;
        StylePolicy stylePolicy = StylePolicy::Apply;
        SourceMask sourceMask = SourceMask::AllCells;

        OverwriteRule glyphOverwriteRule = OverwriteRule::Always;
        OverwriteRule styleOverwriteRule = OverwriteRule::Always;

        DepthPolicy depthPolicy = DepthPolicy::Ignore;

        static WritePolicy VisibleObject();
        static WritePolicy AuthoredObject();
        static WritePolicy SolidObject();

        static WritePolicy StyleOnly();
        static WritePolicy NoWrite();

        bool writesGlyphs() const;
        bool writesStyles() const;
        bool canWriteAnything() const;

        bool operator==(const WritePolicy& other) const;
        bool operator!=(const WritePolicy& other) const;
    };
}