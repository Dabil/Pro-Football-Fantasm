#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "Rendering/Objects/XpArtLoader.h"
#include "Rendering/Objects/XpSequenceLoader.h"

namespace XpSequenceAccess
{
    enum class FrameLookupCode
    {
        None,
        SequenceUnavailable,
        SequenceInvalid,
        NegativeFrameIndex,
        FrameIndexNotFound,
        DefaultFrameUnavailable
    };

    struct SequenceLoadOptions
    {
        XpSequenceLoader::LoadOptions sequenceLoadOptions;
    };

    struct SequenceLoadResult
    {
        std::shared_ptr<XpArtLoader::XpSequence> sequence;
        XpSequenceLoader::LoadResult loadResult;
        bool success = false;
        std::string canonicalPath;
        std::string errorMessage;

        bool hasSequence() const
        {
            return sequence != nullptr;
        }
    };

    struct FrameLookupResult
    {
        const XpArtLoader::XpSequence* sequence = nullptr;
        const XpArtLoader::XpFrame* frame = nullptr;
        bool success = false;
        int requestedFrameIndex = -1;
        FrameLookupCode code = FrameLookupCode::None;
        std::string errorMessage;

        bool hasFrame() const
        {
            return frame != nullptr;
        }
    };

    class LocalSequenceCache
    {
    public:
        const std::shared_ptr<XpArtLoader::XpSequence>* find(const std::string& canonicalPath) const;

        std::shared_ptr<XpArtLoader::XpSequence> store(
            const std::string& canonicalPath,
            std::shared_ptr<XpArtLoader::XpSequence> sequence);

        void clear();
        std::size_t size() const;

    private:
        std::unordered_map<std::string, std::shared_ptr<XpArtLoader::XpSequence>> m_sequences;
    };

    SequenceLoadResult loadSequenceFromFile(
        const std::string& filePath,
        const SequenceLoadOptions& options = {});

    SequenceLoadResult loadSequenceFromFile(
        const std::string& filePath,
        LocalSequenceCache& cache,
        const SequenceLoadOptions& options = {});

    FrameLookupResult getDefaultFrame(const XpArtLoader::XpSequence& sequence);
    FrameLookupResult getDefaultFrame(const std::shared_ptr<XpArtLoader::XpSequence>& sequence);

    FrameLookupResult getFrameByIndex(
        const XpArtLoader::XpSequence& sequence,
        int frameIndex);

    FrameLookupResult getFrameByIndex(
        const std::shared_ptr<XpArtLoader::XpSequence>& sequence,
        int frameIndex);

    XpArtLoader::LoadResult buildTextObjectFromDefaultFrame(
        const XpArtLoader::XpSequence& sequence,
        const XpArtLoader::XpFrameConversionOptions& options = {});

    XpArtLoader::LoadResult buildTextObjectFromDefaultFrame(
        const std::shared_ptr<XpArtLoader::XpSequence>& sequence,
        const XpArtLoader::XpFrameConversionOptions& options = {});

    XpArtLoader::LoadResult buildTextObjectFromFrameByIndex(
        const XpArtLoader::XpSequence& sequence,
        int frameIndex,
        const XpArtLoader::XpFrameConversionOptions& options = {});

    XpArtLoader::LoadResult buildTextObjectFromFrameByIndex(
        const std::shared_ptr<XpArtLoader::XpSequence>& sequence,
        int frameIndex,
        const XpArtLoader::XpFrameConversionOptions& options = {});

    const char* toString(FrameLookupCode code);
}
