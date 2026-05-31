#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/XpArtLoader.h"

namespace Animation
{
    class Animator;
    class AnimatedTextAssetSequence;

    enum class AnimatedTextAssetFrameSourceKind
    {
        Empty,
        TextObject,
        PlainTextUtf8,
        XpSequenceFrame,
        Custom
    };

    enum class AnimatedTextAssetFrameResultCode
    {
        None,
        EmptyFrame,
        FrameIndexOutOfRange,
        PlainTextBuildFailed,
        XpSequenceUnavailable,
        XpFrameUnavailable,
        XpFrameBuildFailed,
        CustomFrameUnavailable
    };

    struct AnimatedTextAssetFrameTiming
    {
        double durationSeconds = 0.0;

        bool hasDuration() const;

        static AnimatedTextAssetFrameTiming fromSeconds(double seconds);
        static AnimatedTextAssetFrameTiming fromMilliseconds(int milliseconds);
    };

    struct AnimatedTextAssetFrameInfo
    {
        std::string name;
        std::string sourcePath;
        std::string sourceFormat;
        int sourceFrameIndex = -1;

        bool hasName() const;
        bool hasSourcePath() const;
    };

    struct AnimatedTextAssetFrameBuildResult
    {
        TextObject object;
        bool success = false;
        AnimatedTextAssetFrameResultCode code = AnimatedTextAssetFrameResultCode::None;
        std::string errorMessage;

        bool hasObject() const;
    };

    struct AnimatedTextAssetSequencePlaybackResult
    {
        AnimatedTextAssetFrameBuildResult buildResult;

        std::size_t frameIndex = 0;
        bool hasFrameIndex = false;
        bool usedExplicitFrameIndex = false;
        double elapsedSeconds = 0.0;

        bool success() const;
        bool hasObject() const;
    };

    class AnimatedTextAssetFrame
    {
    public:
        AnimatedTextAssetFrame() = default;

        static AnimatedTextAssetFrame fromTextObject(
            TextObject object,
            AnimatedTextAssetFrameTiming timing = {},
            AnimatedTextAssetFrameInfo info = {});

        static AnimatedTextAssetFrame fromPlainTextUtf8(
            std::string text,
            AnimatedTextAssetFrameTiming timing = {},
            AnimatedTextAssetFrameInfo info = {});

        static AnimatedTextAssetFrame fromXpSequenceFrame(
            std::shared_ptr<const XpArtLoader::XpSequence> sequence,
            int frameOrdinal,
            AnimatedTextAssetFrameTiming timing = {},
            AnimatedTextAssetFrameInfo info = {},
            XpArtLoader::XpFrameConversionOptions conversionOptions = {});

        static AnimatedTextAssetFrame custom(
            AnimatedTextAssetFrameTiming timing = {},
            AnimatedTextAssetFrameInfo info = {});

        AnimatedTextAssetFrameSourceKind sourceKind() const;

        const AnimatedTextAssetFrameTiming& timing() const;
        void setTiming(const AnimatedTextAssetFrameTiming& timing);

        const AnimatedTextAssetFrameInfo& info() const;
        void setInfo(const AnimatedTextAssetFrameInfo& info);

        bool hasName() const;
        const std::string& name() const;

        bool hasExplicitDuration() const;
        double durationSeconds(double fallbackDurationSeconds) const;

        bool canBuildTextObject() const;
        AnimatedTextAssetFrameBuildResult buildTextObject() const;

        const TextObject* textObject() const;
        const std::string& plainTextUtf8() const;

        const std::shared_ptr<const XpArtLoader::XpSequence>& xpSequence() const;
        int xpFrameOrdinal() const;
        const XpArtLoader::XpFrameConversionOptions& xpConversionOptions() const;

    private:
        AnimatedTextAssetFrameSourceKind m_sourceKind = AnimatedTextAssetFrameSourceKind::Empty;
        AnimatedTextAssetFrameTiming m_timing;
        AnimatedTextAssetFrameInfo m_info;

        std::optional<TextObject> m_textObject;
        std::string m_plainTextUtf8;

        std::shared_ptr<const XpArtLoader::XpSequence> m_xpSequence;
        int m_xpFrameOrdinal = -1;
        XpArtLoader::XpFrameConversionOptions m_xpConversionOptions;
    };

    class AnimatedTextAssetSequence
    {
    public:
        AnimatedTextAssetSequence() = default;
        explicit AnimatedTextAssetSequence(std::string name);

        static AnimatedTextAssetSequence fromTextObjects(
            std::vector<TextObject> frames,
            double defaultFrameDurationSeconds,
            std::string name = {});

        static AnimatedTextAssetSequence fromPlainTextFrames(
            std::vector<std::string> frames,
            double defaultFrameDurationSeconds,
            std::string name = {});

        static AnimatedTextAssetSequence fromXpSequence(
            std::shared_ptr<const XpArtLoader::XpSequence> sequence,
            double fallbackFrameDurationSeconds = 0.0,
            XpArtLoader::XpFrameConversionOptions conversionOptions = {},
            std::string name = {});

        bool isEmpty() const;
        bool isValid() const;
        void clear();

        const std::string& name() const;
        void setName(std::string name);

        void setDefaultFrameDurationSeconds(double seconds);
        double defaultFrameDurationSeconds() const;

        void setLooping(bool looping);
        bool isLooping() const;

        std::size_t frameCount() const;
        const std::vector<AnimatedTextAssetFrame>& frames() const;

        const AnimatedTextAssetFrame* frameAt(std::size_t index) const;
        AnimatedTextAssetFrame* frameAt(std::size_t index);

        bool addFrame(const AnimatedTextAssetFrame& frame);
        bool addFrame(AnimatedTextAssetFrame&& frame);

        int findFrameIndexByName(std::string_view name) const;

        double frameDurationSeconds(std::size_t index) const;
        double totalDurationSeconds() const;

        std::size_t frameIndexForElapsedSeconds(double elapsedSeconds) const;
        std::size_t frameIndexForProgress(double normalizedProgress) const;

        AnimatedTextAssetFrameBuildResult buildTextObjectForFrame(std::size_t index) const;
        AnimatedTextAssetFrameBuildResult buildTextObjectForElapsedSeconds(double elapsedSeconds) const;

    private:
        static double clampNonNegative(double value);
        static double sanitizeDefaultDuration(double seconds);

        std::string m_name;
        double m_defaultFrameDurationSeconds = 0.0;
        bool m_looping = false;
        std::vector<AnimatedTextAssetFrame> m_frames;
    };

    std::size_t frameIndexForAnimator(
        const AnimatedTextAssetSequence& sequence,
        const Animator& animator);

    AnimatedTextAssetSequencePlaybackResult buildTextObjectForAnimatorWithResult(
        const AnimatedTextAssetSequence& sequence,
        const Animator& animator);

    AnimatedTextAssetFrameBuildResult buildTextObjectForAnimator(
        const AnimatedTextAssetSequence& sequence,
        const Animator& animator);

    const char* toString(AnimatedTextAssetFrameSourceKind kind);
    const char* toString(AnimatedTextAssetFrameResultCode code);
}