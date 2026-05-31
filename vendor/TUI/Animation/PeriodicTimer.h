#pragma once

#include "Animation/TickEvent.h"

namespace Animation
{
    class PeriodicTimer
    {
    public:
        explicit PeriodicTimer(double intervalSeconds = 1.0);

        void reset();
        void setIntervalSeconds(double seconds);

        double intervalSeconds() const;
        double accumulatorSeconds() const;

        bool update(double deltaSeconds);
        bool update(const TickEvent& event);

        int consumeTicks(double deltaSeconds, int maxTicks = 1);
        int consumeTicks(const TickEvent& event, int maxTicks = 1);

    private:
        double m_intervalSeconds = 1.0;
        double m_accumulatorSeconds = 0.0;
    };
}