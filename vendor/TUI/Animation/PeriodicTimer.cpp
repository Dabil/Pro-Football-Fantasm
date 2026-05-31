#include "Animation/PeriodicTimer.h"

#include <algorithm>

namespace Animation
{
    PeriodicTimer::PeriodicTimer(double intervalSeconds)
        : m_intervalSeconds(std::max(0.0, intervalSeconds))
    {
    }

    void PeriodicTimer::reset()
    {
        m_accumulatorSeconds = 0.0;
    }

    void PeriodicTimer::setIntervalSeconds(double seconds)
    {
        m_intervalSeconds = std::max(0.0, seconds);

        if (m_intervalSeconds <= 0.0)
        {
            m_accumulatorSeconds = 0.0;
        }
    }

    double PeriodicTimer::intervalSeconds() const
    {
        return m_intervalSeconds;
    }

    double PeriodicTimer::accumulatorSeconds() const
    {
        return m_accumulatorSeconds;
    }

    bool PeriodicTimer::update(double deltaSeconds)
    {
        return consumeTicks(deltaSeconds, 1) > 0;
    }

    bool PeriodicTimer::update(const TickEvent& event)
    {
        return update(event.deltaSeconds);
    }

    int PeriodicTimer::consumeTicks(double deltaSeconds, int maxTicks)
    {
        if (m_intervalSeconds <= 0.0 || maxTicks <= 0 || deltaSeconds <= 0.0)
        {
            return 0;
        }

        m_accumulatorSeconds += deltaSeconds;

        int ticks = 0;

        while (ticks < maxTicks && m_accumulatorSeconds >= m_intervalSeconds)
        {
            m_accumulatorSeconds -= m_intervalSeconds;
            ++ticks;
        }

        return ticks;
    }

    int PeriodicTimer::consumeTicks(const TickEvent& event, int maxTicks)
    {
        return consumeTicks(event.deltaSeconds, maxTicks);
    }
}