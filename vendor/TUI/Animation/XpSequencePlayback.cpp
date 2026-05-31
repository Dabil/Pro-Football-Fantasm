#include "Animation/XpSequencePlayback.h"

#include <algorithm>

#include "Rendering/Objects/XpSequenceAnimationAdapter.h"

namespace
{
    constexpr double kMillisecondsPerSecond = 1000.0;

    double sanitizeFallbackFramesPerSecond(double fallbackFramesPerSecond)
    {
        return fallbackFramesPerSecond > 0.0
            ? fallbackFramesPerSecond
            : 12.0;
    }

    double resolveFramesPerSecond(
        const XpArtLoader::XpSequence& sequence,
        double fallbackFramesPerSecond)
    {
        if (sequence.metadata.defaultFramesPerSecond.has_value() &&
            *sequence.metadata.defaultFramesPerSecond > 0)
        {
            return static_cast<double>(*sequence.metadata.defaultFramesPerSecond);
        }

        if (sequence.metadata.defaultFrameDurationMilliseconds.has_value() &&
            *sequence.metadata.defaultFrameDurationMilliseconds > 0)
        {
            return kMillisecondsPerSecond /
                static_cast<double>(*sequence.metadata.defaultFrameDurationMilliseconds);
        }

        const XpSequenceAnimationAdapter::AnimationFrameTiming firstFrameTiming =
            XpSequenceAnimationAdapter::getFrameTiming(sequence, 0);

        if (firstFrameTiming.durationMilliseconds.has_value() &&
            *firstFrameTiming.durationMilliseconds > 0)
        {
            return kMillisecondsPerSecond /
                static_cast<double>(*firstFrameTiming.durationMilliseconds);
        }

        if (firstFrameTiming.framesPerSecond.has_value() &&
            *firstFrameTiming.framesPerSecond > 0)
        {
            return static_cast<double>(*firstFrameTiming.framesPerSecond);
        }

        return sanitizeFallbackFramesPerSecond(fallbackFramesPerSecond);
    }
}

namespace Animation
{
    XpSequencePlaybackConfiguration configureAnimatorForXpSequence(
        Animator& animator,
        const XpArtLoader::XpSequence& sequence,
        double fallbackFramesPerSecond)
    {
        XpSequencePlaybackConfiguration result;

        if (!sequence.isValid())
        {
            animator.configure(0, 0.0);
            animator.setLooping(false);
            return result;
        }

        result.frameCount = static_cast<std::size_t>(
            std::max(0, sequence.getFrameCount()));

        result.framesPerSecond = resolveFramesPerSecond(
            sequence,
            fallbackFramesPerSecond);

        result.looping = sequence.metadata.loop.value_or(false);
        result.success = result.frameCount > 0 && result.framesPerSecond > 0.0;

        animator.configure(result.frameCount, result.framesPerSecond);
        animator.setLooping(result.looping);

        return result;
    }
}