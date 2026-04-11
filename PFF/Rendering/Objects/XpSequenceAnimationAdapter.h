#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Rendering/Objects/XpArtLoader.h"

namespace XpSequenceAnimationAdapter
{
    enum class FrameRequestCode
    {
        None,
        SequenceUnavailable,
        SequenceInvalid,
        NegativeFrameOrdinal,
        FrameOrdinalOutOfRange,
        FrameUnavailable,
        FrameDocumentUnavailable,
        TextObjectBuildFailed
    };

    struct AnimationFrameTiming
    {
        std::optional<int> durationMilliseconds;
        std::optional<int> framesPerSecond;
        bool durationUsesFrameOverride = false;
        bool durationUsesSequenceDefault = false;
        bool durationUsesFramesPerSecondFallback = false;
        bool loopEnabled = false;

        bool hasTiming() const;
    };

    struct AnimationFrameReference
    {
        int frameOrdinal = -1;
        int frameIndex = -1;
        std::string label;
        std::string sourcePath;
        const XpArtLoader::XpFrame* frame = nullptr;

        bool isValid() const;
    };

    struct FrameRequestResult
    {
        const XpArtLoader::XpSequence* sequence = nullptr;
        const XpArtLoader::XpFrame* frame = nullptr;
        AnimationFrameReference reference;
        AnimationFrameTiming timing;
        bool success = false;
        int requestedFrameOrdinal = -1;
        FrameRequestCode code = FrameRequestCode::None;
        std::string errorMessage;

        bool hasFrame() const;
    };

    struct TextObjectFrameResult
    {
        FrameRequestResult frameRequest;
        XpArtLoader::LoadResult buildResult;
        bool success = false;
        std::string errorMessage;
    };

    class Adapter
    {
    public:
        Adapter() = default;
        explicit Adapter(const XpArtLoader::XpSequence& sequence);

        void bind(const XpArtLoader::XpSequence& sequence);
        void clear();

        bool hasSequence() const;
        bool isValid() const;
        int getFrameCount() const;
        int getInitialFrameOrdinal() const;
        std::vector<int> getOrderedFrameIndices() const;

        const XpArtLoader::XpSequence* getSequence() const;

        FrameRequestResult getFrame(int frameOrdinal) const;
        AnimationFrameTiming getFrameTiming(int frameOrdinal) const;

        TextObjectFrameResult buildTextObjectForFrame(
            int frameOrdinal,
            const XpArtLoader::XpFrameConversionOptions& options = {}) const;

    private:
        const XpArtLoader::XpSequence* m_sequence = nullptr;
    };

    FrameRequestResult getFrame(
        const XpArtLoader::XpSequence& sequence,
        int frameOrdinal);

    AnimationFrameTiming getFrameTiming(
        const XpArtLoader::XpSequence& sequence,
        int frameOrdinal);

    TextObjectFrameResult buildTextObjectForFrame(
        const XpArtLoader::XpSequence& sequence,
        int frameOrdinal,
        const XpArtLoader::XpFrameConversionOptions& options = {});

    const char* toString(FrameRequestCode code);
}
