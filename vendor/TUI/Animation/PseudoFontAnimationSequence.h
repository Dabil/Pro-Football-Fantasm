#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "Animation/AnimatedTextAssetSequence.h"
#include "Rendering/Objects/LayeredTextObject.h"

namespace Animation
{
    struct PseudoFontAnimationFrame
    {
        std::string frameName;
        std::vector<std::string> visibleLayerNames;
        double durationSeconds = 0.0;
    };

    enum class PseudoFontAnimationFrameBuildKind
    {
        Hidden,
        SingleLayer,
        Composited,
        Invalid
    };

    struct PseudoFontAnimationFrameDiagnostic
    {
        std::string frameName;
        PseudoFontAnimationFrameBuildKind kind = PseudoFontAnimationFrameBuildKind::Invalid;
        bool success = false;
        std::string message;
    };

    struct PseudoFontAnimationSequenceBuildResult
    {
        AnimatedTextAssetSequence sequence;
        std::vector<PseudoFontAnimationFrameDiagnostic> diagnostics;

        bool success = false;
        std::size_t requestedFrameCount = 0;
        std::size_t builtFrameCount = 0;

        bool hasDiagnostics() const;
    };

    PseudoFontAnimationSequenceBuildResult buildPseudoFontAnimationSequenceWithDiagnostics(
        const Rendering::LayeredTextObject& source,
        const std::vector<PseudoFontAnimationFrame>& frames,
        std::string sequenceName = {});

    AnimatedTextAssetSequence buildPseudoFontAnimationSequence(
        const Rendering::LayeredTextObject& source,
        const std::vector<PseudoFontAnimationFrame>& frames,
        std::string sequenceName = {});

    const char* toString(PseudoFontAnimationFrameBuildKind kind);
}