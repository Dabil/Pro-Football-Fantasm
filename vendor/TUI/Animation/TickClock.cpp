#include "Animation/TickClock.h"

#include <algorithm>

namespace Animation
{
    TickClock::TickClock(double fixedStepSeconds)
        : m_startTime(Clock::now())
        , m_lastTickTime(m_startTime)
        , m_fixedStepSeconds(std::max(0.0, fixedStepSeconds))
    {
    }

    void TickClock::reset()
    {
        const TimePoint now = Clock::now();

        m_startTime = now;
        m_lastTickTime = now;

        m_deltaSeconds = 0.0;
        m_elapsedSeconds = 0.0;
        m_fixedAccumulatorSeconds = 0.0;
    }

    void TickClock::tick()
    {
        const TimePoint now = Clock::now();

        if (m_paused)
        {
            m_lastTickTime = now;
            m_deltaSeconds = 0.0;
            return;
        }

        m_deltaSeconds = toSeconds(now - m_lastTickTime);
        m_lastTickTime = now;

        if (m_deltaSeconds < 0.0)
        {
            m_deltaSeconds = 0.0;
        }

        m_elapsedSeconds += m_deltaSeconds;
        m_fixedAccumulatorSeconds += m_deltaSeconds;
    }

    void TickClock::pause()
    {
        setPaused(true);
    }

    void TickClock::resume()
    {
        setPaused(false);
    }

    void TickClock::setPaused(bool paused)
    {
        if (m_paused == paused)
        {
            return;
        }

        m_paused = paused;
        m_lastTickTime = Clock::now();
        m_deltaSeconds = 0.0;
    }

    bool TickClock::isPaused() const
    {
        return m_paused;
    }

    double TickClock::deltaSeconds() const
    {
        return m_deltaSeconds;
    }

    double TickClock::elapsedSeconds() const
    {
        return m_elapsedSeconds;
    }

    double TickClock::fixedStepSeconds() const
    {
        return m_fixedStepSeconds;
    }

    double TickClock::fixedAccumulatorSeconds() const
    {
        return m_fixedAccumulatorSeconds;
    }

    void TickClock::setFixedStepSeconds(double seconds)
    {
        m_fixedStepSeconds = std::max(0.0, seconds);

        if (m_fixedStepSeconds <= 0.0)
        {
            m_fixedAccumulatorSeconds = 0.0;
        }
    }

    bool TickClock::hasFixedStep() const
    {
        return m_fixedStepSeconds > 0.0
            && m_fixedAccumulatorSeconds >= m_fixedStepSeconds;
    }

    bool TickClock::consumeFixedStep()
    {
        if (!hasFixedStep())
        {
            return false;
        }

        m_fixedAccumulatorSeconds -= m_fixedStepSeconds;
        return true;
    }

    int TickClock::consumeFixedSteps(int maxSteps)
    {
        if (maxSteps <= 0 || m_fixedStepSeconds <= 0.0)
        {
            return 0;
        }

        int consumedSteps = 0;

        while (consumedSteps < maxSteps && hasFixedStep())
        {
            m_fixedAccumulatorSeconds -= m_fixedStepSeconds;
            ++consumedSteps;
        }

        return consumedSteps;
    }

    double TickClock::toSeconds(Duration duration)
    {
        return std::chrono::duration<double>(duration).count();
    }
}