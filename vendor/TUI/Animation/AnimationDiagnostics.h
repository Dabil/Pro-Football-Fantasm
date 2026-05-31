#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Animation/Animation.h"

namespace Animation
{
    struct AnimatorDiagnostics
    {
        std::string controllerName;
        std::size_t frameCount = 0;
        std::size_t currentFrameIndex = 0;
        double framesPerSecond = 0.0;
        double playbackRate = 1.0;
        double elapsedSeconds = 0.0;
        double durationSeconds = 0.0;
        double progress = 0.0;
        PlaybackState playbackState = PlaybackState::Stopped;
        PlaybackMode playbackMode = PlaybackMode::Once;
        bool hasExplicitFrameIndex = false;
        std::optional<std::size_t> explicitFrameIndex;
    };

    struct AnimationBindingDiagnostics
    {
        std::string targetName;
        std::string controllerName;
        std::string targetKind;
        bool controllerResolved = false;
        bool targetReferenceValid = false;
        bool frameReferenceValid = false;

        std::optional<std::size_t> currentFrameIndex;
        std::optional<std::size_t> animatorFrameIndex;
        bool usesDurationAwareSequenceTiming = false;

        std::optional<std::size_t> availableFrameCount;
        std::string message;
    };

    struct AnimationRecompositionDiagnostics
    {
        bool recomposed = false;
        bool frameStateChanged = false;
        bool forced = false;
        bool invalidated = false;
        bool wholePageInvalidated = false;
        std::size_t dirtyRegionCount = 0;
        std::size_t bindingResultCount = 0;
        std::uint64_t deterministicSignature = 0;
    };

    struct AnimationDiagnosticsReport
    {
        bool enabled = false;
        std::vector<AnimatorDiagnostics> controllers;
        std::vector<AnimationBindingDiagnostics> bindings;
        AnimationRecompositionDiagnostics lastRecomposition;

        void clearRuntimeData();
    };

    const char* toString(PlaybackState state);
    const char* toString(PlaybackMode mode);
}