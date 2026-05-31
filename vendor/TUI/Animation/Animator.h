#pragma once

#include <cstddef>
#include <optional>

#include "Animation/Animation.h"
#include "Animation/TickEvent.h"

namespace Animation
{
    class Animator
    {
    public:
        Animator();
        explicit Animator(std::size_t frameCount);
        Animator(std::size_t frameCount, double framesPerSecond);

        void configure(std::size_t frameCount, double framesPerSecond);
        void setFrameCount(std::size_t frameCount);
        std::size_t frameCount() const;

        void setFramesPerSecond(double framesPerSecond);
        double framesPerSecond() const;

        void setPlaybackRate(double playbackRate);
        double playbackRate() const;

        void setLooping(bool looping);
        bool isLooping() const;

        void setPlaybackMode(PlaybackMode mode);
        PlaybackMode playbackMode() const;

        void play();
        void pause();
        void stop();
        void restart();

        void update(double deltaSeconds);
        void update(const TickEvent& event);

        PlaybackState playbackState() const;
        bool isPlaying() const;
        bool isPaused() const;
        bool isStopped() const;
        bool isFinished() const;

        void setElapsedSeconds(double seconds);
        double elapsedSeconds() const;

        double durationSeconds() const;
        double progress() const;

        void setCurrentFrameIndex(std::size_t frameIndex);
        void setExplicitFrameIndex(std::optional<std::size_t> frameIndex);
        void clearExplicitFrameIndex();

        bool hasExplicitFrameIndex() const;
        std::optional<std::size_t> explicitFrameIndex() const;

        std::size_t currentFrameIndex() const;

    private:
        static double clampNonNegative(double value);
        static double clampPositiveOrZero(double value);
        static std::size_t clampFrameIndex(std::size_t frameIndex, std::size_t frameCount);

        void syncDuration();
        void applyEndBehavior();

    private:
        std::size_t m_frameCount = 0;
        double m_framesPerSecond = 0.0;
        double m_playbackRate = 1.0;
        double m_elapsedSeconds = 0.0;

        PlaybackState m_playbackState = PlaybackState::Stopped;
        PlaybackMode m_playbackMode = PlaybackMode::Once;

        std::optional<std::size_t> m_explicitFrameIndex;
    };
}