#pragma once

#include <cstdint>

#include "Animation/TickClock.h"

namespace Animation
{
    struct TickEvent
    {
        double deltaSeconds = 0.0;
        double elapsedSeconds = 0.0;

        double fixedStepSeconds = 0.0;
        double fixedAccumulatorSeconds = 0.0;
        int fixedStepsAvailable = 0;

        std::uint64_t frameIndex = 0;
        bool paused = false;

        bool hasDelta() const
        {
            return deltaSeconds > 0.0;
        }

        bool hasFixedSteps() const
        {
            return fixedStepsAvailable > 0;
        }

        static TickEvent fromClock(const TickClock& clock, std::uint64_t frameIndex)
        {
            TickEvent event;

            event.deltaSeconds = clock.deltaSeconds();
            event.elapsedSeconds = clock.elapsedSeconds();
            event.fixedStepSeconds = clock.fixedStepSeconds();
            event.fixedAccumulatorSeconds = clock.fixedAccumulatorSeconds();
            event.frameIndex = frameIndex;
            event.paused = clock.isPaused();

            if (event.fixedStepSeconds > 0.0)
            {
                event.fixedStepsAvailable =
                    static_cast<int>(event.fixedAccumulatorSeconds / event.fixedStepSeconds);
            }

            return event;
        }
    };
}