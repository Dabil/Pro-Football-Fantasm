#include "Rendering/Objects/XpSequenceAnimationAdapter.h"

#include <sstream>

namespace
{
    constexpr int kMillisecondsPerSecond = 1000;

    XpSequenceAnimationAdapter::FrameRequestResult makeFailure(
        const XpArtLoader::XpSequence* sequence,
        const int frameOrdinal,
        const XpSequenceAnimationAdapter::FrameRequestCode code,
        const std::string& errorMessage)
    {
        XpSequenceAnimationAdapter::FrameRequestResult result;
        result.sequence = sequence;
        result.requestedFrameOrdinal = frameOrdinal;
        result.reference.frameOrdinal = frameOrdinal;
        result.code = code;
        result.errorMessage = errorMessage;
        result.success = false;
        return result;
    }

    XpSequenceAnimationAdapter::AnimationFrameTiming resolveTiming(
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpSequenceMetadata& metadata)
    {
        XpSequenceAnimationAdapter::AnimationFrameTiming timing;
        timing.loopEnabled = metadata.loop.value_or(false);

        if (frame.overrides.durationMilliseconds.has_value())
        {
            timing.durationMilliseconds = frame.overrides.durationMilliseconds;
            timing.durationUsesFrameOverride = true;
        }
        else if (metadata.defaultFrameDurationMilliseconds.has_value())
        {
            timing.durationMilliseconds = metadata.defaultFrameDurationMilliseconds;
            timing.durationUsesSequenceDefault = true;
        }
        else if (metadata.defaultFramesPerSecond.has_value() && *metadata.defaultFramesPerSecond > 0)
        {
            timing.framesPerSecond = metadata.defaultFramesPerSecond;
            timing.durationMilliseconds =
                kMillisecondsPerSecond / *metadata.defaultFramesPerSecond;
            timing.durationUsesFramesPerSecondFallback = true;
        }
        else if (metadata.defaultFramesPerSecond.has_value())
        {
            timing.framesPerSecond = metadata.defaultFramesPerSecond;
        }

        if (!timing.framesPerSecond.has_value() &&
            metadata.defaultFramesPerSecond.has_value() &&
            *metadata.defaultFramesPerSecond > 0)
        {
            timing.framesPerSecond = metadata.defaultFramesPerSecond;
        }

        return timing;
    }

    XpSequenceAnimationAdapter::AnimationFrameReference buildReference(
        const XpArtLoader::XpFrame& frame,
        const int frameOrdinal)
    {
        XpSequenceAnimationAdapter::AnimationFrameReference reference;
        reference.frameOrdinal = frameOrdinal;
        reference.frameIndex = frame.frameIndex;
        reference.label = frame.label;
        reference.sourcePath = frame.sourcePath;
        reference.frame = &frame;
        return reference;
    }

    XpSequenceAnimationAdapter::TextObjectFrameResult makeTextObjectFailure(
        XpSequenceAnimationAdapter::FrameRequestResult frameRequest,
        const std::string& errorMessage,
        const XpSequenceAnimationAdapter::FrameRequestCode code)
    {
        XpSequenceAnimationAdapter::TextObjectFrameResult result;
        frameRequest.code = code;
        frameRequest.success = false;
        frameRequest.errorMessage = errorMessage;
        result.frameRequest = std::move(frameRequest);
        result.buildResult.success = false;
        result.buildResult.errorMessage = errorMessage;
        result.success = false;
        result.errorMessage = errorMessage;
        return result;
    }
}

namespace XpSequenceAnimationAdapter
{
    bool AnimationFrameTiming::hasTiming() const
    {
        return durationMilliseconds.has_value() || framesPerSecond.has_value();
    }

    bool AnimationFrameReference::isValid() const
    {
        return frame != nullptr && frameOrdinal >= 0 && frameIndex >= 0;
    }

    bool FrameRequestResult::hasFrame() const
    {
        return frame != nullptr;
    }

    Adapter::Adapter(const XpArtLoader::XpSequence& sequence)
        : m_sequence(&sequence)
    {
    }

    void Adapter::bind(const XpArtLoader::XpSequence& sequence)
    {
        m_sequence = &sequence;
    }

    void Adapter::clear()
    {
        m_sequence = nullptr;
    }

    bool Adapter::hasSequence() const
    {
        return m_sequence != nullptr;
    }

    bool Adapter::isValid() const
    {
        return m_sequence != nullptr && m_sequence->isValid();
    }

    int Adapter::getFrameCount() const
    {
        return m_sequence != nullptr ? m_sequence->getFrameCount() : 0;
    }

    int Adapter::getInitialFrameOrdinal() const
    {
        return getFrameCount() > 0 ? 0 : -1;
    }

    std::vector<int> Adapter::getOrderedFrameIndices() const
    {
        std::vector<int> indices;
        if (m_sequence == nullptr)
        {
            return indices;
        }

        indices.reserve(m_sequence->frames.size());
        for (const XpArtLoader::XpFrame& frame : m_sequence->frames)
        {
            indices.push_back(frame.frameIndex);
        }

        return indices;
    }

    const XpArtLoader::XpSequence* Adapter::getSequence() const
    {
        return m_sequence;
    }

    FrameRequestResult Adapter::getFrame(const int frameOrdinal) const
    {
        if (m_sequence == nullptr)
        {
            return makeFailure(
                nullptr,
                frameOrdinal,
                FrameRequestCode::SequenceUnavailable,
                "XP sequence animation adapter is not bound to a sequence.");
        }

        return XpSequenceAnimationAdapter::getFrame(*m_sequence, frameOrdinal);
    }

    AnimationFrameTiming Adapter::getFrameTiming(const int frameOrdinal) const
    {
        if (m_sequence == nullptr)
        {
            return {};
        }

        return XpSequenceAnimationAdapter::getFrameTiming(*m_sequence, frameOrdinal);
    }

    TextObjectFrameResult Adapter::buildTextObjectForFrame(
        const int frameOrdinal,
        const XpArtLoader::XpFrameConversionOptions& options) const
    {
        if (m_sequence == nullptr)
        {
            FrameRequestResult frameRequest = makeFailure(
                nullptr,
                frameOrdinal,
                FrameRequestCode::SequenceUnavailable,
                "XP sequence animation adapter is not bound to a sequence.");

            return makeTextObjectFailure(
                std::move(frameRequest),
                "Cannot build a TextObject because no XP sequence is bound.",
                FrameRequestCode::SequenceUnavailable);
        }

        return XpSequenceAnimationAdapter::buildTextObjectForFrame(*m_sequence, frameOrdinal, options);
    }

    FrameRequestResult getFrame(
        const XpArtLoader::XpSequence& sequence,
        const int frameOrdinal)
    {
        if (!sequence.isValid())
        {
            return makeFailure(
                &sequence,
                frameOrdinal,
                FrameRequestCode::SequenceInvalid,
                "Cannot resolve an animation frame from an invalid XP sequence.");
        }

        if (frameOrdinal < 0)
        {
            return makeFailure(
                &sequence,
                frameOrdinal,
                FrameRequestCode::NegativeFrameOrdinal,
                "Animation frame ordinal must be zero or greater.");
        }

        const XpArtLoader::XpFrame* frame = sequence.tryGetFrame(frameOrdinal);
        if (frame == nullptr)
        {
            std::ostringstream stream;
            stream << "Animation frame ordinal " << frameOrdinal
                << " is outside the retained XP sequence frame range.";

            return makeFailure(
                &sequence,
                frameOrdinal,
                FrameRequestCode::FrameOrdinalOutOfRange,
                stream.str());
        }

        FrameRequestResult result;
        result.sequence = &sequence;
        result.frame = frame;
        result.reference = buildReference(*frame, frameOrdinal);
        result.timing = resolveTiming(*frame, sequence.metadata);
        result.requestedFrameOrdinal = frameOrdinal;
        result.success = true;
        return result;
    }

    AnimationFrameTiming getFrameTiming(
        const XpArtLoader::XpSequence& sequence,
        const int frameOrdinal)
    {
        const FrameRequestResult frameRequest = getFrame(sequence, frameOrdinal);
        if (!frameRequest.success)
        {
            return {};
        }

        return frameRequest.timing;
    }

    TextObjectFrameResult buildTextObjectForFrame(
        const XpArtLoader::XpSequence& sequence,
        const int frameOrdinal,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        FrameRequestResult frameRequest = getFrame(sequence, frameOrdinal);
        if (!frameRequest.success || frameRequest.frame == nullptr)
        {
            return makeTextObjectFailure(
                std::move(frameRequest),
                "Cannot build a TextObject because the requested animation frame could not be resolved.",
                frameRequest.code);
        }

        if (!frameRequest.frame->hasDocument())
        {
            std::ostringstream stream;
            stream << "Animation frame ordinal " << frameOrdinal
                << " does not retain an XP document, so it cannot be converted to a TextObject.";

            return makeTextObjectFailure(
                std::move(frameRequest),
                stream.str(),
                FrameRequestCode::FrameDocumentUnavailable);
        }

        TextObjectFrameResult result;
        result.frameRequest = frameRequest;
        result.buildResult = XpArtLoader::buildTextObjectFromXpFrame(
            *frameRequest.frame,
            sequence.metadata,
            options);

        result.success = result.buildResult.success;
        if (!result.success)
        {
            result.frameRequest.code = FrameRequestCode::TextObjectBuildFailed;
            result.frameRequest.success = false;
            result.frameRequest.errorMessage = result.buildResult.errorMessage;
            result.errorMessage = result.buildResult.errorMessage;
        }

        return result;
    }

    const char* toString(const FrameRequestCode code)
    {
        switch (code)
        {
        case FrameRequestCode::None:
            return "None";
        case FrameRequestCode::SequenceUnavailable:
            return "SequenceUnavailable";
        case FrameRequestCode::SequenceInvalid:
            return "SequenceInvalid";
        case FrameRequestCode::NegativeFrameOrdinal:
            return "NegativeFrameOrdinal";
        case FrameRequestCode::FrameOrdinalOutOfRange:
            return "FrameOrdinalOutOfRange";
        case FrameRequestCode::FrameUnavailable:
            return "FrameUnavailable";
        case FrameRequestCode::FrameDocumentUnavailable:
            return "FrameDocumentUnavailable";
        case FrameRequestCode::TextObjectBuildFailed:
            return "TextObjectBuildFailed";
        }

        return "Unknown";
    }
}
