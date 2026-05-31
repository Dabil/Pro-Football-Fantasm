#include "Animation/AnimationDiagnostics.h"

namespace Animation
{
    void AnimationDiagnosticsReport::clearRuntimeData()
    {
        controllers.clear();
        bindings.clear();
        lastRecomposition = AnimationRecompositionDiagnostics{};
    }

    const char* toString(PlaybackState state)
    {
        switch (state)
        {
        case PlaybackState::Stopped:
            return "Stopped";
        case PlaybackState::Playing:
            return "Playing";
        case PlaybackState::Paused:
            return "Paused";
        case PlaybackState::Finished:
            return "Finished";
        default:
            return "Unknown";
        }
    }

    const char* toString(PlaybackMode mode)
    {
        switch (mode)
        {
        case PlaybackMode::Once:
            return "Once";
        case PlaybackMode::Loop:
            return "Loop";
        case PlaybackMode::PingPong:
            return "PingPong";
        default:
            return "Unknown";
        }
    }
}