#include "Rendering/Composition/WritePolicyBuilder.h"

/*
    Usage Example:

    using namespace Composition;

    WritePolicy customPolicy = WritePolicyBuilder()
        .glyphPolicy(GlyphPolicy::VisibleObject)
        .stylePolicy(StylePolicy::Apply)
        .sourceMask(SourceMask::GlyphCellsOnly)
        .glyphOverwrite(OverwriteRule::IfTargetEmpty)
        .styleOverwrite(OverwriteRule::Never)
        .depthPolicy(DepthPolicy::BehindOnly)
        .build();

    Usage Example 2:

    using namespace Composition;

    WritePolicy visibleBehind = WritePolicyBuilder::fromPreset(WritePresets::visibleObject())
        .depthPolicy(DepthPolicy::BehindOnly)
        .build();
*/

namespace Composition
{
    WritePolicyBuilder::WritePolicyBuilder()
        : m_policy()
    {
    }

    WritePolicyBuilder::WritePolicyBuilder(const WritePolicy& policy)
        : m_policy(policy)
    {
    }

    WritePolicyBuilder WritePolicyBuilder::fromPolicy(const WritePolicy& policy)
    {
        return WritePolicyBuilder(policy);
    }

    WritePolicyBuilder WritePolicyBuilder::fromPreset(const WritePolicy& presetPolicy)
    {
        return WritePolicyBuilder(presetPolicy);
    }

    WritePolicyBuilder& WritePolicyBuilder::reset()
    {
        m_policy = WritePolicy();
        return *this;
    }

    WritePolicyBuilder& WritePolicyBuilder::glyphPolicy(GlyphPolicy value)
    {
        m_policy.glyphPolicy = value;
        return *this;
    }

    WritePolicyBuilder& WritePolicyBuilder::stylePolicy(StylePolicy value)
    {
        m_policy.stylePolicy = value;
        return *this;
    }

    WritePolicyBuilder& WritePolicyBuilder::sourceMask(SourceMask value)
    {
        m_policy.sourceMask = value;
        return *this;
    }

    WritePolicyBuilder& WritePolicyBuilder::glyphOverwrite(OverwriteRule value)
    {
        m_policy.glyphOverwriteRule = value;
        return *this;
    }

    WritePolicyBuilder& WritePolicyBuilder::styleOverwrite(OverwriteRule value)
    {
        m_policy.styleOverwriteRule = value;
        return *this;
    }

    WritePolicyBuilder& WritePolicyBuilder::glyphOverwriteRule(OverwriteRule value)
    {
        return glyphOverwrite(value);
    }

    WritePolicyBuilder& WritePolicyBuilder::styleOverwriteRule(OverwriteRule value)
    {
        return styleOverwrite(value);
    }

    WritePolicyBuilder& WritePolicyBuilder::depthPolicy(DepthPolicy value)
    {
        m_policy.depthPolicy = value;
        return *this;
    }

    const WritePolicy& WritePolicyBuilder::policy() const
    {
        return m_policy;
    }

    WritePolicy WritePolicyBuilder::build() const
    {
        return m_policy;
    }
}