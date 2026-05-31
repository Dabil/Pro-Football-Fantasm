#include "Animation/Animation.h"

#include <algorithm>
#include <cmath>

namespace Animation
{
    bool AnimationTarget::hasTarget() const
    {
        return !targetId.empty()
            && property != TargetProperty::None;
    }

    Animation::Animation(double durationSeconds)
        : m_durationSeconds(clampNonNegative(durationSeconds))
    {
    }

    Animation::Animation(double durationSeconds, const AnimationTarget& target)
        : m_durationSeconds(clampNonNegative(durationSeconds))
        , m_target(target)
    {
    }

    void Animation::reset()
    {
        m_elapsedSeconds = 0.0;
        m_playbackState = PlaybackState::Stopped;
    }

    void Animation::play()
    {
        if (m_playbackState == PlaybackState::Finished)
        {
            m_elapsedSeconds = 0.0;
        }

        m_playbackState = PlaybackState::Playing;
        applyEndBehavior();
    }

    void Animation::pause()
    {
        if (m_playbackState == PlaybackState::Playing)
        {
            m_playbackState = PlaybackState::Paused;
        }
    }

    void Animation::resume()
    {
        if (m_playbackState == PlaybackState::Paused)
        {
            m_playbackState = PlaybackState::Playing;
        }
    }

    void Animation::stop()
    {
        m_elapsedSeconds = 0.0;
        m_playbackState = PlaybackState::Stopped;
    }

    void Animation::finish()
    {
        m_elapsedSeconds = m_durationSeconds;
        m_playbackState = PlaybackState::Finished;
    }

    void Animation::update(double deltaSeconds)
    {
        if (m_playbackState != PlaybackState::Playing)
        {
            return;
        }

        const double safeDeltaSeconds = clampNonNegative(deltaSeconds);
        if (safeDeltaSeconds <= 0.0)
        {
            return;
        }

        m_elapsedSeconds += safeDeltaSeconds;
        applyEndBehavior();
    }

    void Animation::update(const TickEvent& event)
    {
        if (event.paused)
        {
            return;
        }

        update(event.deltaSeconds);
    }

    void Animation::setDurationSeconds(double seconds)
    {
        m_durationSeconds = clampNonNegative(seconds);

        if (m_playbackMode != PlaybackMode::Loop)
        {
            m_elapsedSeconds = std::min(m_elapsedSeconds, m_durationSeconds);
        }

        applyEndBehavior();
    }

    double Animation::durationSeconds() const
    {
        return m_durationSeconds;
    }

    void Animation::setElapsedSeconds(double seconds)
    {
        m_elapsedSeconds = clampNonNegative(seconds);
        applyEndBehavior();
    }

    double Animation::elapsedSeconds() const
    {
        return m_elapsedSeconds;
    }

    double Animation::progress() const
    {
        if (m_durationSeconds <= 0.0)
        {
            return m_elapsedSeconds > 0.0 || m_playbackState == PlaybackState::Finished
                ? 1.0
                : 0.0;
        }

        if (m_playbackMode == PlaybackMode::Loop)
        {
            return std::fmod(m_elapsedSeconds, m_durationSeconds) / m_durationSeconds;
        }

        return clamp01(m_elapsedSeconds / m_durationSeconds);
    }

    double Animation::pingPongProgress() const
    {
        if (m_durationSeconds <= 0.0)
        {
            return progress();
        }

        const double cycleSeconds = m_durationSeconds * 2.0;
        const double cyclePosition = std::fmod(m_elapsedSeconds, cycleSeconds);

        if (cyclePosition <= m_durationSeconds)
        {
            return cyclePosition / m_durationSeconds;
        }

        return 1.0 - ((cyclePosition - m_durationSeconds) / m_durationSeconds);
    }

    PlaybackState Animation::playbackState() const
    {
        return m_playbackState;
    }

    void Animation::setPlaybackMode(PlaybackMode mode)
    {
        m_playbackMode = mode;
        applyEndBehavior();
    }

    PlaybackMode Animation::playbackMode() const
    {
        return m_playbackMode;
    }

    bool Animation::isPlaying() const
    {
        return m_playbackState == PlaybackState::Playing;
    }

    bool Animation::isPaused() const
    {
        return m_playbackState == PlaybackState::Paused;
    }

    bool Animation::isStopped() const
    {
        return m_playbackState == PlaybackState::Stopped;
    }

    bool Animation::isFinished() const
    {
        return m_playbackState == PlaybackState::Finished;
    }

    void Animation::setTarget(const AnimationTarget& target)
    {
        m_target = target;
    }

    const AnimationTarget& Animation::target() const
    {
        return m_target;
    }

    void Animation::setExplicitFrameIndexOverride(std::optional<std::size_t> frameIndex)
    {
        m_explicitFrameIndexOverride = frameIndex;
    }

    void Animation::clearExplicitFrameIndexOverride()
    {
        m_explicitFrameIndexOverride.reset();
    }

    bool Animation::hasExplicitFrameIndexOverride() const
    {
        return m_explicitFrameIndexOverride.has_value();
    }

    std::optional<std::size_t> Animation::explicitFrameIndexOverride() const
    {
        return m_explicitFrameIndexOverride;
    }

    std::size_t Animation::selectFrameIndex(const FrameSelectionInput& input) const
    {
        if (input.frameCount == 0)
        {
            return input.firstFrameIndex;
        }

        if (m_explicitFrameIndexOverride.has_value())
        {
            return clampFrameIndex(m_explicitFrameIndexOverride.value(), input);
        }

        if (input.framesPerSecond > 0.0)
        {
            const double rawFrame =
                effectiveFrameProgress() * static_cast<double>(input.frameCount);

            std::size_t selectedFrame = static_cast<std::size_t>(std::floor(rawFrame));

            if (selectedFrame >= input.frameCount)
            {
                selectedFrame = input.frameCount - 1;
            }

            return input.firstFrameIndex + selectedFrame;
        }

        const double rawFrame =
            effectiveFrameProgress() * static_cast<double>(input.frameCount - 1);

        return input.firstFrameIndex
            + static_cast<std::size_t>(std::round(rawFrame));
    }

    std::size_t Animation::selectFrameIndex(std::size_t frameCount, double framesPerSecond) const
    {
        FrameSelectionInput input;
        input.frameCount = frameCount;
        input.framesPerSecond = framesPerSecond;

        return selectFrameIndex(input);
    }

    double Animation::clampNonNegative(double value)
    {
        return std::max(0.0, value);
    }

    double Animation::clamp01(double value)
    {
        return std::max(0.0, std::min(1.0, value));
    }

    std::size_t Animation::clampFrameIndex(std::size_t frameIndex, const FrameSelectionInput& input)
    {
        if (input.frameCount == 0)
        {
            return input.firstFrameIndex;
        }

        const std::size_t firstFrame = input.firstFrameIndex;
        const std::size_t lastFrame = input.firstFrameIndex + input.frameCount - 1;

        if (frameIndex < firstFrame)
        {
            return firstFrame;
        }

        if (frameIndex > lastFrame)
        {
            return input.clampToLastFrame
                ? lastFrame
                : firstFrame + ((frameIndex - firstFrame) % input.frameCount);
        }

        return frameIndex;
    }

    double Animation::effectiveFrameProgress() const
    {
        if (m_playbackMode == PlaybackMode::PingPong)
        {
            return pingPongProgress();
        }

        return progress();
    }

    void Animation::applyEndBehavior()
    {
        if (m_durationSeconds <= 0.0)
        {
            if (m_playbackState == PlaybackState::Playing)
            {
                m_playbackState = PlaybackState::Finished;
            }

            return;
        }

        if (m_playbackMode == PlaybackMode::Loop || m_playbackMode == PlaybackMode::PingPong)
        {
            return;
        }

        if (m_elapsedSeconds >= m_durationSeconds)
        {
            m_elapsedSeconds = m_durationSeconds;

            if (m_playbackState == PlaybackState::Playing)
            {
                m_playbackState = PlaybackState::Finished;
            }
        }
    }
}