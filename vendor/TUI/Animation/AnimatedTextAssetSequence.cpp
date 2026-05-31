#include "Animation/AnimatedTextAssetSequence.h"

#include <cmath>
#include <sstream>
#include <utility>

#include "Animation/Animator.h"
#include "Rendering/Objects/XpSequenceAnimationAdapter.h"

namespace
{
    constexpr double kMillisecondsPerSecond = 1000.0;

    Animation::AnimatedTextAssetFrameBuildResult makeFailure(
        const Animation::AnimatedTextAssetFrameResultCode code,
        std::string message)
    {
        Animation::AnimatedTextAssetFrameBuildResult result;
        result.success = false;
        result.code = code;
        result.errorMessage = std::move(message);
        return result;
    }

    Animation::AnimatedTextAssetFrameBuildResult makeSuccess(TextObject object)
    {
        Animation::AnimatedTextAssetFrameBuildResult result;
        result.object = std::move(object);
        result.success = true;
        result.code = Animation::AnimatedTextAssetFrameResultCode::None;
        return result;
    }

    double durationFromXpFrame(
        const XpArtLoader::XpSequence& sequence,
        const int frameOrdinal,
        const double fallbackDurationSeconds)
    {
        const XpSequenceAnimationAdapter::AnimationFrameTiming timing =
            XpSequenceAnimationAdapter::getFrameTiming(sequence, frameOrdinal);

        if (timing.durationMilliseconds.has_value() && *timing.durationMilliseconds > 0)
        {
            return static_cast<double>(*timing.durationMilliseconds) / kMillisecondsPerSecond;
        }

        if (timing.framesPerSecond.has_value() && *timing.framesPerSecond > 0)
        {
            return 1.0 / static_cast<double>(*timing.framesPerSecond);
        }

        return fallbackDurationSeconds;
    }

    std::size_t clampSequenceFrameIndex(
        const std::size_t frameIndex,
        const std::size_t frameCount)
    {
        if (frameCount == 0)
        {
            return 0;
        }

        return std::min(frameIndex, frameCount - 1);
    }
}

namespace Animation
{
    bool AnimatedTextAssetFrameTiming::hasDuration() const
    {
        return durationSeconds > 0.0;
    }

    AnimatedTextAssetFrameTiming AnimatedTextAssetFrameTiming::fromSeconds(const double seconds)
    {
        AnimatedTextAssetFrameTiming timing;
        timing.durationSeconds = seconds > 0.0 ? seconds : 0.0;
        return timing;
    }

    AnimatedTextAssetFrameTiming AnimatedTextAssetFrameTiming::fromMilliseconds(const int milliseconds)
    {
        if (milliseconds <= 0)
        {
            return {};
        }

        return fromSeconds(static_cast<double>(milliseconds) / kMillisecondsPerSecond);
    }

    bool AnimatedTextAssetFrameInfo::hasName() const
    {
        return !name.empty();
    }

    bool AnimatedTextAssetFrameInfo::hasSourcePath() const
    {
        return !sourcePath.empty();
    }

    bool AnimatedTextAssetFrameBuildResult::hasObject() const
    {
        return success && object.isLoaded();
    }

    bool AnimatedTextAssetSequencePlaybackResult::success() const
    {
        return buildResult.success;
    }

    bool AnimatedTextAssetSequencePlaybackResult::hasObject() const
    {
        return buildResult.hasObject();
    }

    AnimatedTextAssetFrame AnimatedTextAssetFrame::fromTextObject(
        TextObject object,
        const AnimatedTextAssetFrameTiming timing,
        AnimatedTextAssetFrameInfo info)
    {
        AnimatedTextAssetFrame frame;
        frame.m_sourceKind = AnimatedTextAssetFrameSourceKind::TextObject;
        frame.m_timing = timing;
        frame.m_info = std::move(info);
        frame.m_textObject = std::move(object);
        return frame;
    }

    AnimatedTextAssetFrame AnimatedTextAssetFrame::fromPlainTextUtf8(
        std::string text,
        const AnimatedTextAssetFrameTiming timing,
        AnimatedTextAssetFrameInfo info)
    {
        AnimatedTextAssetFrame frame;
        frame.m_sourceKind = AnimatedTextAssetFrameSourceKind::PlainTextUtf8;
        frame.m_timing = timing;
        frame.m_info = std::move(info);
        frame.m_plainTextUtf8 = std::move(text);
        return frame;
    }

    AnimatedTextAssetFrame AnimatedTextAssetFrame::fromXpSequenceFrame(
        std::shared_ptr<const XpArtLoader::XpSequence> sequence,
        const int frameOrdinal,
        const AnimatedTextAssetFrameTiming timing,
        AnimatedTextAssetFrameInfo info,
        XpArtLoader::XpFrameConversionOptions conversionOptions)
    {
        AnimatedTextAssetFrame frame;
        frame.m_sourceKind = AnimatedTextAssetFrameSourceKind::XpSequenceFrame;
        frame.m_timing = timing;
        frame.m_info = std::move(info);
        frame.m_xpSequence = std::move(sequence);
        frame.m_xpFrameOrdinal = frameOrdinal;
        frame.m_xpConversionOptions = std::move(conversionOptions);
        return frame;
    }

    AnimatedTextAssetFrame AnimatedTextAssetFrame::custom(
        const AnimatedTextAssetFrameTiming timing,
        AnimatedTextAssetFrameInfo info)
    {
        AnimatedTextAssetFrame frame;
        frame.m_sourceKind = AnimatedTextAssetFrameSourceKind::Custom;
        frame.m_timing = timing;
        frame.m_info = std::move(info);
        return frame;
    }

    AnimatedTextAssetFrameSourceKind AnimatedTextAssetFrame::sourceKind() const
    {
        return m_sourceKind;
    }

    const AnimatedTextAssetFrameTiming& AnimatedTextAssetFrame::timing() const
    {
        return m_timing;
    }

    void AnimatedTextAssetFrame::setTiming(const AnimatedTextAssetFrameTiming& timing)
    {
        m_timing = timing;
        if (m_timing.durationSeconds < 0.0)
        {
            m_timing.durationSeconds = 0.0;
        }
    }

    const AnimatedTextAssetFrameInfo& AnimatedTextAssetFrame::info() const
    {
        return m_info;
    }

    void AnimatedTextAssetFrame::setInfo(const AnimatedTextAssetFrameInfo& info)
    {
        m_info = info;
    }

    bool AnimatedTextAssetFrame::hasName() const
    {
        return m_info.hasName();
    }

    const std::string& AnimatedTextAssetFrame::name() const
    {
        return m_info.name;
    }

    bool AnimatedTextAssetFrame::hasExplicitDuration() const
    {
        return m_timing.hasDuration();
    }

    double AnimatedTextAssetFrame::durationSeconds(const double fallbackDurationSeconds) const
    {
        if (m_timing.hasDuration())
        {
            return m_timing.durationSeconds;
        }

        return fallbackDurationSeconds > 0.0 ? fallbackDurationSeconds : 0.0;
    }

    bool AnimatedTextAssetFrame::canBuildTextObject() const
    {
        switch (m_sourceKind)
        {
        case AnimatedTextAssetFrameSourceKind::TextObject:
            return m_textObject.has_value();
        case AnimatedTextAssetFrameSourceKind::PlainTextUtf8:
            return true;
        case AnimatedTextAssetFrameSourceKind::XpSequenceFrame:
            return m_xpSequence != nullptr && m_xpFrameOrdinal >= 0;
        case AnimatedTextAssetFrameSourceKind::Empty:
        case AnimatedTextAssetFrameSourceKind::Custom:
            return false;
        }

        return false;
    }

    AnimatedTextAssetFrameBuildResult AnimatedTextAssetFrame::buildTextObject() const
    {
        switch (m_sourceKind)
        {
        case AnimatedTextAssetFrameSourceKind::TextObject:
            if (!m_textObject.has_value())
            {
                return makeFailure(
                    AnimatedTextAssetFrameResultCode::EmptyFrame,
                    "The animated text frame does not contain a TextObject.");
            }

            return makeSuccess(*m_textObject);

        case AnimatedTextAssetFrameSourceKind::PlainTextUtf8:
            return makeSuccess(TextObject::fromUtf8(m_plainTextUtf8));

        case AnimatedTextAssetFrameSourceKind::XpSequenceFrame:
        {
            if (m_xpSequence == nullptr)
            {
                return makeFailure(
                    AnimatedTextAssetFrameResultCode::XpSequenceUnavailable,
                    "The animated text frame is not bound to an XP sequence.");
            }

            XpSequenceAnimationAdapter::TextObjectFrameResult frameResult =
                XpSequenceAnimationAdapter::buildTextObjectForFrame(
                    *m_xpSequence,
                    m_xpFrameOrdinal,
                    m_xpConversionOptions);

            if (!frameResult.success)
            {
                return makeFailure(
                    frameResult.frameRequest.hasFrame()
                    ? AnimatedTextAssetFrameResultCode::XpFrameBuildFailed
                    : AnimatedTextAssetFrameResultCode::XpFrameUnavailable,
                    frameResult.errorMessage.empty()
                    ? "Failed to build a TextObject from the XP sequence frame."
                    : frameResult.errorMessage);
            }

            return makeSuccess(std::move(frameResult.buildResult.object));
        }

        case AnimatedTextAssetFrameSourceKind::Custom:
            return makeFailure(
                AnimatedTextAssetFrameResultCode::CustomFrameUnavailable,
                "Custom animated text frames require an asset-specific conversion path before rendering.");

        case AnimatedTextAssetFrameSourceKind::Empty:
            return makeFailure(
                AnimatedTextAssetFrameResultCode::EmptyFrame,
                "The animated text frame is empty.");
        }

        return makeFailure(
            AnimatedTextAssetFrameResultCode::EmptyFrame,
            "The animated text frame source kind is unknown.");
    }

    const TextObject* AnimatedTextAssetFrame::textObject() const
    {
        return m_textObject.has_value() ? &(*m_textObject) : nullptr;
    }

    const std::string& AnimatedTextAssetFrame::plainTextUtf8() const
    {
        return m_plainTextUtf8;
    }

    const std::shared_ptr<const XpArtLoader::XpSequence>& AnimatedTextAssetFrame::xpSequence() const
    {
        return m_xpSequence;
    }

    int AnimatedTextAssetFrame::xpFrameOrdinal() const
    {
        return m_xpFrameOrdinal;
    }

    const XpArtLoader::XpFrameConversionOptions& AnimatedTextAssetFrame::xpConversionOptions() const
    {
        return m_xpConversionOptions;
    }

    AnimatedTextAssetSequence::AnimatedTextAssetSequence(std::string name)
        : m_name(std::move(name))
    {
    }

    AnimatedTextAssetSequence AnimatedTextAssetSequence::fromTextObjects(
        std::vector<TextObject> frames,
        const double defaultFrameDurationSeconds,
        std::string name)
    {
        AnimatedTextAssetSequence sequence(std::move(name));
        sequence.setDefaultFrameDurationSeconds(defaultFrameDurationSeconds);

        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            AnimatedTextAssetFrameInfo info;
            info.sourceFormat = "TextObject";
            info.sourceFrameIndex = static_cast<int>(index);

            sequence.addFrame(AnimatedTextAssetFrame::fromTextObject(
                std::move(frames[index]),
                {},
                std::move(info)));
        }

        return sequence;
    }

    AnimatedTextAssetSequence AnimatedTextAssetSequence::fromPlainTextFrames(
        std::vector<std::string> frames,
        const double defaultFrameDurationSeconds,
        std::string name)
    {
        AnimatedTextAssetSequence sequence(std::move(name));
        sequence.setDefaultFrameDurationSeconds(defaultFrameDurationSeconds);

        for (std::size_t index = 0; index < frames.size(); ++index)
        {
            AnimatedTextAssetFrameInfo info;
            info.sourceFormat = "txt";
            info.sourceFrameIndex = static_cast<int>(index);

            sequence.addFrame(AnimatedTextAssetFrame::fromPlainTextUtf8(
                std::move(frames[index]),
                {},
                std::move(info)));
        }

        return sequence;
    }

    AnimatedTextAssetSequence AnimatedTextAssetSequence::fromXpSequence(
        std::shared_ptr<const XpArtLoader::XpSequence> xpSequence,
        const double fallbackFrameDurationSeconds,
        XpArtLoader::XpFrameConversionOptions conversionOptions,
        std::string name)
    {
        AnimatedTextAssetSequence sequence(std::move(name));
        sequence.setDefaultFrameDurationSeconds(fallbackFrameDurationSeconds);

        if (xpSequence == nullptr)
        {
            return sequence;
        }

        if (sequence.m_name.empty())
        {
            sequence.m_name = xpSequence->metadata.name.empty()
                ? xpSequence->metadata.sequenceLabel
                : xpSequence->metadata.name;
        }

        sequence.setLooping(xpSequence->metadata.loop.value_or(false));

        for (std::size_t ordinal = 0; ordinal < xpSequence->frames.size(); ++ordinal)
        {
            const XpArtLoader::XpFrame& xpFrame = xpSequence->frames[ordinal];

            AnimatedTextAssetFrameInfo info;
            info.name = xpFrame.label;
            info.sourcePath = xpFrame.sourcePath;
            info.sourceFormat = "xp";
            info.sourceFrameIndex = xpFrame.frameIndex;

            AnimatedTextAssetFrameTiming timing;
            timing.durationSeconds = durationFromXpFrame(
                *xpSequence,
                static_cast<int>(ordinal),
                sequence.m_defaultFrameDurationSeconds);

            sequence.addFrame(AnimatedTextAssetFrame::fromXpSequenceFrame(
                xpSequence,
                static_cast<int>(ordinal),
                timing,
                std::move(info),
                conversionOptions));
        }

        return sequence;
    }

    bool AnimatedTextAssetSequence::isEmpty() const
    {
        return m_frames.empty();
    }

    bool AnimatedTextAssetSequence::isValid() const
    {
        return !m_frames.empty();
    }

    void AnimatedTextAssetSequence::clear()
    {
        m_name.clear();
        m_defaultFrameDurationSeconds = 0.0;
        m_looping = false;
        m_frames.clear();
    }

    const std::string& AnimatedTextAssetSequence::name() const
    {
        return m_name;
    }

    void AnimatedTextAssetSequence::setName(std::string name)
    {
        m_name = std::move(name);
    }

    void AnimatedTextAssetSequence::setDefaultFrameDurationSeconds(const double seconds)
    {
        m_defaultFrameDurationSeconds = sanitizeDefaultDuration(seconds);
    }

    double AnimatedTextAssetSequence::defaultFrameDurationSeconds() const
    {
        return m_defaultFrameDurationSeconds;
    }

    void AnimatedTextAssetSequence::setLooping(const bool looping)
    {
        m_looping = looping;
    }

    bool AnimatedTextAssetSequence::isLooping() const
    {
        return m_looping;
    }

    std::size_t AnimatedTextAssetSequence::frameCount() const
    {
        return m_frames.size();
    }

    const std::vector<AnimatedTextAssetFrame>& AnimatedTextAssetSequence::frames() const
    {
        return m_frames;
    }

    const AnimatedTextAssetFrame* AnimatedTextAssetSequence::frameAt(const std::size_t index) const
    {
        return index < m_frames.size() ? &m_frames[index] : nullptr;
    }

    AnimatedTextAssetFrame* AnimatedTextAssetSequence::frameAt(const std::size_t index)
    {
        return index < m_frames.size() ? &m_frames[index] : nullptr;
    }

    bool AnimatedTextAssetSequence::addFrame(const AnimatedTextAssetFrame& frame)
    {
        m_frames.push_back(frame);
        return true;
    }

    bool AnimatedTextAssetSequence::addFrame(AnimatedTextAssetFrame&& frame)
    {
        m_frames.push_back(std::move(frame));
        return true;
    }

    int AnimatedTextAssetSequence::findFrameIndexByName(const std::string_view name) const
    {
        if (name.empty())
        {
            return -1;
        }

        for (std::size_t index = 0; index < m_frames.size(); ++index)
        {
            if (m_frames[index].name() == name)
            {
                return static_cast<int>(index);
            }
        }

        return -1;
    }

    double AnimatedTextAssetSequence::frameDurationSeconds(const std::size_t index) const
    {
        const AnimatedTextAssetFrame* frame = frameAt(index);
        if (frame == nullptr)
        {
            return 0.0;
        }

        return frame->durationSeconds(m_defaultFrameDurationSeconds);
    }

    double AnimatedTextAssetSequence::totalDurationSeconds() const
    {
        double total = 0.0;
        for (std::size_t index = 0; index < m_frames.size(); ++index)
        {
            total += frameDurationSeconds(index);
        }

        return total;
    }

    std::size_t AnimatedTextAssetSequence::frameIndexForElapsedSeconds(double elapsedSeconds) const
    {
        if (m_frames.empty())
        {
            return 0;
        }

        elapsedSeconds = clampNonNegative(elapsedSeconds);

        const double totalDuration = totalDurationSeconds();
        if (totalDuration <= 0.0)
        {
            return 0;
        }

        if (m_looping)
        {
            elapsedSeconds = std::fmod(elapsedSeconds, totalDuration);
        }
        else if (elapsedSeconds >= totalDuration)
        {
            return m_frames.size() - 1;
        }

        double cursor = 0.0;
        for (std::size_t index = 0; index < m_frames.size(); ++index)
        {
            const double duration = frameDurationSeconds(index);
            cursor += duration;

            if (elapsedSeconds < cursor)
            {
                return index;
            }
        }

        return m_frames.size() - 1;
    }

    std::size_t AnimatedTextAssetSequence::frameIndexForProgress(double normalizedProgress) const
    {
        if (m_frames.empty())
        {
            return 0;
        }

        if (normalizedProgress <= 0.0)
        {
            return 0;
        }

        if (normalizedProgress >= 1.0)
        {
            return m_frames.size() - 1;
        }

        const double duration = totalDurationSeconds();
        if (duration <= 0.0)
        {
            const std::size_t index = static_cast<std::size_t>(
                normalizedProgress * static_cast<double>(m_frames.size()));
            return index < m_frames.size() ? index : m_frames.size() - 1;
        }

        return frameIndexForElapsedSeconds(normalizedProgress * duration);
    }

    AnimatedTextAssetFrameBuildResult AnimatedTextAssetSequence::buildTextObjectForFrame(
        const std::size_t index) const
    {
        const AnimatedTextAssetFrame* frame = frameAt(index);
        if (frame == nullptr)
        {
            std::ostringstream stream;
            stream << "Animated text asset frame index " << index
                << " is outside the sequence frame range.";

            return makeFailure(
                AnimatedTextAssetFrameResultCode::FrameIndexOutOfRange,
                stream.str());
        }

        return frame->buildTextObject();
    }

    AnimatedTextAssetFrameBuildResult AnimatedTextAssetSequence::buildTextObjectForElapsedSeconds(
        const double elapsedSeconds) const
    {
        if (m_frames.empty())
        {
            return makeFailure(
                AnimatedTextAssetFrameResultCode::FrameIndexOutOfRange,
                "Cannot build a TextObject from an empty animated text asset sequence.");
        }

        return buildTextObjectForFrame(frameIndexForElapsedSeconds(elapsedSeconds));
    }

    double AnimatedTextAssetSequence::clampNonNegative(const double value)
    {
        return value > 0.0 ? value : 0.0;
    }

    double AnimatedTextAssetSequence::sanitizeDefaultDuration(const double seconds)
    {
        return seconds > 0.0 ? seconds : 0.0;
    }

    std::size_t frameIndexForAnimator(
        const AnimatedTextAssetSequence& sequence,
        const Animator& animator)
    {
        if (sequence.isEmpty())
        {
            return 0;
        }

        if (animator.hasExplicitFrameIndex())
        {
            return clampSequenceFrameIndex(
                animator.explicitFrameIndex().value(),
                sequence.frameCount());
        }

        return sequence.frameIndexForElapsedSeconds(animator.elapsedSeconds());
    }

    AnimatedTextAssetSequencePlaybackResult buildTextObjectForAnimatorWithResult(
        const AnimatedTextAssetSequence& sequence,
        const Animator& animator)
    {
        AnimatedTextAssetSequencePlaybackResult result;
        result.elapsedSeconds = animator.elapsedSeconds();
        result.usedExplicitFrameIndex = animator.hasExplicitFrameIndex();

        if (sequence.isEmpty())
        {
            result.buildResult = makeFailure(
                AnimatedTextAssetFrameResultCode::FrameIndexOutOfRange,
                "Cannot build a TextObject from an empty animated text asset sequence.");

            return result;
        }

        result.frameIndex = frameIndexForAnimator(sequence, animator);
        result.hasFrameIndex = true;
        result.buildResult = sequence.buildTextObjectForFrame(result.frameIndex);

        return result;
    }

    AnimatedTextAssetFrameBuildResult buildTextObjectForAnimator(
        const AnimatedTextAssetSequence& sequence,
        const Animator& animator)
    {
        return buildTextObjectForAnimatorWithResult(sequence, animator).buildResult;
    }

    const char* toString(const AnimatedTextAssetFrameSourceKind kind)
    {
        switch (kind)
        {
        case AnimatedTextAssetFrameSourceKind::Empty:
            return "Empty";
        case AnimatedTextAssetFrameSourceKind::TextObject:
            return "TextObject";
        case AnimatedTextAssetFrameSourceKind::PlainTextUtf8:
            return "PlainTextUtf8";
        case AnimatedTextAssetFrameSourceKind::XpSequenceFrame:
            return "XpSequenceFrame";
        case AnimatedTextAssetFrameSourceKind::Custom:
            return "Custom";
        }

        return "Unknown";
    }

    const char* toString(const AnimatedTextAssetFrameResultCode code)
    {
        switch (code)
        {
        case AnimatedTextAssetFrameResultCode::None:
            return "None";
        case AnimatedTextAssetFrameResultCode::EmptyFrame:
            return "EmptyFrame";
        case AnimatedTextAssetFrameResultCode::FrameIndexOutOfRange:
            return "FrameIndexOutOfRange";
        case AnimatedTextAssetFrameResultCode::PlainTextBuildFailed:
            return "PlainTextBuildFailed";
        case AnimatedTextAssetFrameResultCode::XpSequenceUnavailable:
            return "XpSequenceUnavailable";
        case AnimatedTextAssetFrameResultCode::XpFrameUnavailable:
            return "XpFrameUnavailable";
        case AnimatedTextAssetFrameResultCode::XpFrameBuildFailed:
            return "XpFrameBuildFailed";
        case AnimatedTextAssetFrameResultCode::CustomFrameUnavailable:
            return "CustomFrameUnavailable";
        }

        return "Unknown";
    }
}