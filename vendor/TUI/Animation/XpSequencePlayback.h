#pragma once

#include "Animation/Animator.h"
#include "Rendering/Objects/XpArtLoader.h"

namespace Animation
{
    struct XpSequencePlaybackConfiguration
    {
        bool success = false;
        std::size_t frameCount = 0;
        double framesPerSecond = 0.0;
        bool looping = false;
    };

    XpSequencePlaybackConfiguration configureAnimatorForXpSequence(
        Animator& animator,
        const XpArtLoader::XpSequence& sequence,
        double fallbackFramesPerSecond = 12.0);
}