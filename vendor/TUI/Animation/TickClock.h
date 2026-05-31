#pragma once

#include <chrono>

namespace Animation
{
    class TickClock
    {
    public:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;
        using Duration = Clock::duration;

        explicit TickClock(double fixedStepSeconds = 1.0 / 60.0);

        void reset();
        void tick();

        void pause();
        void resume();
        void setPaused(bool paused);
        bool isPaused() const;

        double deltaSeconds() const;
        double elapsedSeconds() const;
        double fixedStepSeconds() const;
        double fixedAccumulatorSeconds() const;

        void setFixedStepSeconds(double seconds);

        bool hasFixedStep() const;
        bool consumeFixedStep();

        int consumeFixedSteps(int maxSteps);

    private:
        static double toSeconds(Duration duration);

    private:
        TimePoint m_startTime;
        TimePoint m_lastTickTime;

        double m_deltaSeconds = 0.0;
        double m_elapsedSeconds = 0.0;

        double m_fixedStepSeconds = 1.0 / 60.0;
        double m_fixedAccumulatorSeconds = 0.0;

        bool m_paused = false;
    };
}