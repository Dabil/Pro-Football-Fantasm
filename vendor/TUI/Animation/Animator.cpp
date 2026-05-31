#include "Animation/Animator.h"

#include <algorithm>
#include <cmath>

namespace Animation
{
    Animator::Animator()
    {
    }

    Animator::Animator(std::size_t frameCount)
        : m_frameCount(frameCount)
    {
    }

    Animator::Animator(std::size_t frameCount, double framesPerSecond)
        : m_frameCount(frameCount)
        , m_framesPerSecond(clampPositiveOrZero(framesPerSecond))
    {
    }

    void Animator::configure(std::size_t frameCount, double framesPerSecond)
    {
        m_frameCount = frameCount;
        m_framesPerSecond = clampPositiveOrZero(framesPerSecond);
        m_elapsedSeconds = std::min(m_elapsedSeconds, durationSeconds());
        applyEndBehavior();
    }

    void Animator::setFrameCount(std::size_t frameCount)
    {
        m_frameCount = frameCount;

        if (m_explicitFrameIndex.has_value())
        {
            m_explicitFrameIndex = clampFrameIndex(m_explicitFrameIndex.value(), m_frameCount);
        }

        m_elapsedSeconds = std::min(m_elapsedSeconds, durationSeconds());
        applyEndBehavior();
    }

    std::size_t Animator::frameCount() const
    {
        return m_frameCount;
    }

    void Animator::setFramesPerSecond(double framesPerSecond)
    {
        m_framesPerSecond = clampPositiveOrZero(framesPerSecond);
        m_elapsedSeconds = std::min(m_elapsedSeconds, durationSeconds());
        applyEndBehavior();
    }

    double Animator::framesPerSecond() const
    {
        return m_framesPerSecond;
    }

    void Animator::setPlaybackRate(double playbackRate)
    {
        m_playbackRate = std::max(0.0, playbackRate);
    }

    double Animator::playbackRate() const
    {
        return m_playbackRate;
    }

    void Animator::setLooping(bool looping)
    {
        m_playbackMode = looping ? PlaybackMode::Loop : PlaybackMode::Once;
        applyEndBehavior();
    }

    bool Animator::isLooping() const
    {
        return m_playbackMode == PlaybackMode::Loop;
    }

    void Animator::setPlaybackMode(PlaybackMode mode)
    {
        m_playbackMode = mode;
        applyEndBehavior();
    }

    PlaybackMode Animator::playbackMode() const
    {
        return m_playbackMode;
    }

    void Animator::play()
    {
        if (m_playbackState == PlaybackState::Finished)
        {
            m_elapsedSeconds = 0.0;
        }

        m_playbackState = PlaybackState::Playing;
        applyEndBehavior();
    }

    void Animator::pause()
    {
        if (m_playbackState == PlaybackState::Playing)
        {
            m_playbackState = PlaybackState::Paused;
        }
    }

    void Animator::stop()
    {
        m_elapsedSeconds = 0.0;
        m_playbackState = PlaybackState::Stopped;
    }

    void Animator::restart()
    {
        m_elapsedSeconds = 0.0;
        m_playbackState = PlaybackState::Playing;
        applyEndBehavior();
    }

    void Animator::update(double deltaSeconds)
    {
        if (m_playbackState != PlaybackState::Playing)
        {
            return;
        }

        const double safeDeltaSeconds = clampNonNegative(deltaSeconds);
        if (safeDeltaSeconds <= 0.0 || m_playbackRate <= 0.0)
        {
            return;
        }

        m_elapsedSeconds += safeDeltaSeconds * m_playbackRate;
        applyEndBehavior();
    }

    void Animator::update(const TickEvent& event)
    {
        if (event.paused)
        {
            return;
        }

        update(event.deltaSeconds);
    }

    PlaybackState Animator::playbackState() const
    {
        return m_playbackState;
    }

    bool Animator::isPlaying() const
    {
        return m_playbackState == PlaybackState::Playing;
    }

    bool Animator::isPaused() const
    {
        return m_playbackState == PlaybackState::Paused;
    }

    bool Animator::isStopped() const
    {
        return m_playbackState == PlaybackState::Stopped;
    }

    bool Animator::isFinished() const
    {
        return m_playbackState == PlaybackState::Finished;
    }

    void Animator::setElapsedSeconds(double seconds)
    {
        m_elapsedSeconds = clampNonNegative(seconds);
        applyEndBehavior();
    }

    double Animator::elapsedSeconds() const
    {
        return m_elapsedSeconds;
    }

    double Animator::durationSeconds() const
    {
        if (m_frameCount == 0 || m_framesPerSecond <= 0.0)
        {
            return 0.0;
        }

        return static_cast<double>(m_frameCount) / m_framesPerSecond;
    }

    double Animator::progress() const
    {
        const double duration = durationSeconds();
        if (duration <= 0.0)
        {
            return 0.0;
        }

        if (m_playbackMode == PlaybackMode::Loop)
        {
            return std::fmod(m_elapsedSeconds, duration) / duration;
        }

        return std::max(0.0, std::min(1.0, m_elapsedSeconds / duration));
    }

    void Animator::setCurrentFrameIndex(std::size_t frameIndex)
    {
        setExplicitFrameIndex(frameIndex);
    }

    void Animator::setExplicitFrameIndex(std::optional<std::size_t> frameIndex)
    {
        if (!frameIndex.has_value())
        {
            m_explicitFrameIndex.reset();
            return;
        }

        m_explicitFrameIndex = clampFrameIndex(frameIndex.value(), m_frameCount);
    }

    void Animator::clearExplicitFrameIndex()
    {
        m_explicitFrameIndex.reset();
    }

    bool Animator::hasExplicitFrameIndex() const
    {
        return m_explicitFrameIndex.has_value();
    }

    std::optional<std::size_t> Animator::explicitFrameIndex() const
    {
        return m_explicitFrameIndex;
    }

    std::size_t Animator::currentFrameIndex() const
    {
        if (m_frameCount == 0)
        {
            return 0;
        }

        if (m_explicitFrameIndex.has_value())
        {
            return clampFrameIndex(m_explicitFrameIndex.value(), m_frameCount);
        }

        if (m_framesPerSecond <= 0.0)
        {
            return 0;
        }

        const double duration = durationSeconds();
        if (duration <= 0.0)
        {
            return 0;
        }

        double effectiveElapsed = m_elapsedSeconds;

        if (m_playbackMode == PlaybackMode::Loop)
        {
            effectiveElapsed = std::fmod(effectiveElapsed, duration);
        }
        else
        {
            effectiveElapsed = std::min(effectiveElapsed, duration);
        }

        std::size_t frameIndex =
            static_cast<std::size_t>(std::floor(effectiveElapsed * m_framesPerSecond));

        if (frameIndex >= m_frameCount)
        {
            frameIndex = m_frameCount - 1;
        }

        return frameIndex;
    }

    double Animator::clampNonNegative(double value)
    {
        return std::max(0.0, value);
    }

    double Animator::clampPositiveOrZero(double value)
    {
        return std::max(0.0, value);
    }

    std::size_t Animator::clampFrameIndex(std::size_t frameIndex, std::size_t frameCount)
    {
        if (frameCount == 0)
        {
            return 0;
        }

        return std::min(frameIndex, frameCount - 1);
    }

    void Animator::syncDuration()
    {
        m_elapsedSeconds = std::min(m_elapsedSeconds, durationSeconds());
    }

    void Animator::applyEndBehavior()
    {
        const double duration = durationSeconds();

        if (duration <= 0.0 || m_frameCount == 0)
        {
            if (m_playbackState == PlaybackState::Playing)
            {
                m_playbackState = PlaybackState::Finished;
            }

            m_elapsedSeconds = 0.0;
            return;
        }

        if (m_playbackMode == PlaybackMode::Loop)
        {
            return;
        }

        if (m_elapsedSeconds >= duration)
        {
            m_elapsedSeconds = duration;

            if (m_playbackState == PlaybackState::Playing)
            {
                m_playbackState = PlaybackState::Finished;
            }
        }
    }
}