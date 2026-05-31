#pragma once

#include "Rendering/Composition/WritePolicy.h"

namespace Composition
{
    /*
        WritePolicyBuilder is a small ergonomic helper for constructing
        explicit WritePolicy values.

        It is intentionally local, mutable-only-during-construction state.
        It does not perform composition, does not hold target/buffer/backend
        references, and does not introduce any new execution path.

        The final engine-facing result is always a plain WritePolicy value.
    */
    class WritePolicyBuilder
    {
    public:
        WritePolicyBuilder();
        explicit WritePolicyBuilder(const WritePolicy& policy);

        static WritePolicyBuilder fromPolicy(const WritePolicy& policy);
        static WritePolicyBuilder fromPreset(const WritePolicy& presetPolicy);

        WritePolicyBuilder& reset();

        WritePolicyBuilder& glyphPolicy(GlyphPolicy value);
        WritePolicyBuilder& stylePolicy(StylePolicy value);
        WritePolicyBuilder& sourceMask(SourceMask value);
        WritePolicyBuilder& glyphOverwrite(OverwriteRule value);
        WritePolicyBuilder& styleOverwrite(OverwriteRule value);
        WritePolicyBuilder& glyphOverwriteRule(OverwriteRule value);
        WritePolicyBuilder& styleOverwriteRule(OverwriteRule value);
        WritePolicyBuilder& depthPolicy(DepthPolicy value);

        const WritePolicy& policy() const;
        WritePolicy build() const;

    private:
        WritePolicy m_policy;
    };
}