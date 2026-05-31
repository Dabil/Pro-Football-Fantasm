#include "Rendering/Composition/WritePresets.h"

namespace Composition::WritePresets
{
    WritePolicy visibleObject(DepthPolicy depthPolicy)
    {
        WritePolicy policy;
        policy.glyphPolicy = GlyphPolicy::VisibleObject;
        policy.stylePolicy = StylePolicy::Apply;
        policy.sourceMask = SourceMask::GlyphCellsOnly;
        policy.glyphOverwriteRule = OverwriteRule::Always;
        policy.styleOverwriteRule = OverwriteRule::Always;
        policy.depthPolicy = depthPolicy;
        return policy;
    }

    WritePolicy authoredObject(DepthPolicy depthPolicy)
    {
        WritePolicy policy;
        policy.glyphPolicy = GlyphPolicy::AuthoredObject;
        policy.stylePolicy = StylePolicy::Apply;
        policy.sourceMask = SourceMask::GlyphCellsOnly;
        policy.glyphOverwriteRule = OverwriteRule::Always;
        policy.styleOverwriteRule = OverwriteRule::Always;
        policy.depthPolicy = depthPolicy;
        return policy;
    }

    WritePolicy solidObject(DepthPolicy depthPolicy)
    {
        WritePolicy policy;
        policy.glyphPolicy = GlyphPolicy::SolidObject;
        policy.stylePolicy = StylePolicy::Apply;
        policy.sourceMask = SourceMask::AllCells;
        policy.glyphOverwriteRule = OverwriteRule::Always;
        policy.styleOverwriteRule = OverwriteRule::Always;
        policy.depthPolicy = depthPolicy;
        return policy;
    }

    WritePolicy glyphsOnly(DepthPolicy depthPolicy)
    {
        WritePolicy policy = authoredObject(depthPolicy);
        policy.stylePolicy = StylePolicy::None;
        policy.styleOverwriteRule = OverwriteRule::Never;
        return policy;
    }

    WritePolicy styleMask(DepthPolicy depthPolicy)
    {
        WritePolicy policy;
        policy.glyphPolicy = GlyphPolicy::None;
        policy.stylePolicy = StylePolicy::Apply;
        policy.sourceMask = SourceMask::GlyphCellsOnly;
        policy.glyphOverwriteRule = OverwriteRule::Never;
        policy.styleOverwriteRule = OverwriteRule::Always;
        policy.depthPolicy = depthPolicy;
        return policy;
    }

    WritePolicy styleBlock(DepthPolicy depthPolicy)
    {
        WritePolicy policy;
        policy.glyphPolicy = GlyphPolicy::None;
        policy.stylePolicy = StylePolicy::Apply;
        policy.sourceMask = SourceMask::AllCells;
        policy.glyphOverwriteRule = OverwriteRule::Never;
        policy.styleOverwriteRule = OverwriteRule::Always;
        policy.depthPolicy = depthPolicy;
        return policy;
    }
}