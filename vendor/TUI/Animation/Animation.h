#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "Animation/TickEvent.h"

namespace Animation
{
    enum class PlaybackState
    {
        Stopped,
        Playing,
        Paused,
        Finished
    };

    enum class PlaybackMode
    {
        Once,
        Loop,
        PingPong
    };

    enum class TargetProperty
    {
        None,
        FrameIndex,
        OffsetX,
        OffsetY,
        Opacity,
        Color,
        Style,
        Glyph,
        Custom
    };

    struct AnimationTarget
    {
        std::string targetId;
        TargetProperty property = TargetProperty::None;
        std::string customPropertyName;

        bool hasTarget() const;
    };

    struct FrameSelectionInput
    {
        std::size_t frameCount = 0;
        double framesPerSecond = 0.0;
        bool clampToLastFrame = true;
        std::size_t firstFrameIndex = 0;
    };

    class Animation
    {
    public:
        explicit Animation(double durationSeconds = 0.0);
        Animation(double durationSeconds, const AnimationTarget& target);

        void reset();
        void play();
        void pause();
        void resume();
        void stop();
        void finish();

        void update(double deltaSeconds);
        void update(const TickEvent& event);

        void setDurationSeconds(double seconds);
        double durationSeconds() const;

        void setElapsedSeconds(double seconds);
        double elapsedSeconds() const;

        double progress() const;
        double pingPongProgress() const;

        PlaybackState playbackState() const;
        void setPlaybackMode(PlaybackMode mode);
        PlaybackMode playbackMode() const;

        bool isPlaying() const;
        bool isPaused() const;
        bool isStopped() const;
        bool isFinished() const;

        void setTarget(const AnimationTarget& target);
        const AnimationTarget& target() const;

        void setExplicitFrameIndexOverride(std::optional<std::size_t> frameIndex);
        void clearExplicitFrameIndexOverride();
        bool hasExplicitFrameIndexOverride() const;
        std::optional<std::size_t> explicitFrameIndexOverride() const;

        std::size_t selectFrameIndex(const FrameSelectionInput& input) const;
        std::size_t selectFrameIndex(std::size_t frameCount, double framesPerSecond) const;

    private:
        static double clampNonNegative(double value);
        static double clamp01(double value);
        static std::size_t clampFrameIndex(std::size_t frameIndex, const FrameSelectionInput& input);

        double effectiveFrameProgress() const;
        void applyEndBehavior();

    private:
        double m_durationSeconds = 0.0;
        double m_elapsedSeconds = 0.0;

        PlaybackState m_playbackState = PlaybackState::Stopped;
        PlaybackMode m_playbackMode = PlaybackMode::Once;

        AnimationTarget m_target;
        std::optional<std::size_t> m_explicitFrameIndexOverride;
    };
}