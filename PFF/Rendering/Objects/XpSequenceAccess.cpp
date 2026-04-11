#include "Rendering/Objects/XpSequenceAccess.h"

#include <filesystem>
#include <utility>

namespace
{
    std::string tryCanonicalizePath(const std::string& filePath)
    {
        if (filePath.empty())
        {
            return std::string();
        }

        std::error_code error;
        const std::filesystem::path weaklyCanonical =
            std::filesystem::weakly_canonical(std::filesystem::path(filePath), error);

        if (error)
        {
            return std::filesystem::path(filePath).lexically_normal().string();
        }

        return weaklyCanonical.string();
    }

    XpSequenceAccess::SequenceLoadResult makeSequenceLoadFailure(
        const std::string& canonicalPath,
        XpSequenceLoader::LoadResult loadResult,
        const std::string& errorMessage)
    {
        XpSequenceAccess::SequenceLoadResult result;
        result.loadResult = std::move(loadResult);
        result.success = false;
        result.canonicalPath = canonicalPath;
        result.errorMessage = errorMessage;
        return result;
    }

    XpSequenceAccess::FrameLookupResult makeFrameLookupFailure(
        const XpArtLoader::XpSequence* sequence,
        const int frameIndex,
        const XpSequenceAccess::FrameLookupCode code,
        const std::string& errorMessage)
    {
        XpSequenceAccess::FrameLookupResult result;
        result.sequence = sequence;
        result.requestedFrameIndex = frameIndex;
        result.code = code;
        result.errorMessage = errorMessage;
        result.success = false;
        return result;
    }

    XpArtLoader::LoadResult makeTextObjectFailure(const std::string& errorMessage)
    {
        XpArtLoader::LoadResult result;
        result.success = false;
        result.errorMessage = errorMessage;
        return result;
    }

    XpArtLoader::LoadResult buildTextObjectFromFrameLookup(
        const XpSequenceAccess::FrameLookupResult& lookup,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        if (!lookup.success || lookup.sequence == nullptr || lookup.frame == nullptr)
        {
            return makeTextObjectFailure(lookup.errorMessage);
        }

        return XpArtLoader::buildTextObjectFromXpFrame(
            *lookup.frame,
            lookup.sequence->metadata,
            options);
    }
}

namespace XpSequenceAccess
{
    const std::shared_ptr<XpArtLoader::XpSequence>* LocalSequenceCache::find(
        const std::string& canonicalPath) const
    {
        const auto it = m_sequences.find(canonicalPath);
        if (it == m_sequences.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    std::shared_ptr<XpArtLoader::XpSequence> LocalSequenceCache::store(
        const std::string& canonicalPath,
        std::shared_ptr<XpArtLoader::XpSequence> sequence)
    {
        if (canonicalPath.empty() || sequence == nullptr)
        {
            return sequence;
        }

        m_sequences[canonicalPath] = std::move(sequence);
        return m_sequences[canonicalPath];
    }

    void LocalSequenceCache::clear()
    {
        m_sequences.clear();
    }

    std::size_t LocalSequenceCache::size() const
    {
        return m_sequences.size();
    }

    SequenceLoadResult loadSequenceFromFile(
        const std::string& filePath,
        const SequenceLoadOptions& options)
    {
        const std::string canonicalPath = tryCanonicalizePath(filePath);
        XpSequenceLoader::LoadResult loadResult =
            XpSequenceLoader::loadFromFile(filePath, options.sequenceLoadOptions);

        if (!loadResult.success)
        {
            const std::string errorMessage = XpSequenceLoader::formatLoadError(loadResult);
            return makeSequenceLoadFailure(
                canonicalPath,
                std::move(loadResult),
                errorMessage);
        }

        if (!loadResult.sequence.isValid())
        {
            return makeSequenceLoadFailure(
                canonicalPath,
                std::move(loadResult),
                "XP sequence load succeeded, but the retained sequence is invalid.");
        }

        SequenceLoadResult result;
        result.sequence = std::make_shared<XpArtLoader::XpSequence>(loadResult.sequence);
        result.loadResult = std::move(loadResult);
        result.success = true;
        result.canonicalPath = canonicalPath;
        return result;
    }

    SequenceLoadResult loadSequenceFromFile(
        const std::string& filePath,
        LocalSequenceCache& cache,
        const SequenceLoadOptions& options)
    {
        const std::string canonicalPath = tryCanonicalizePath(filePath);
        if (!canonicalPath.empty())
        {
            const std::shared_ptr<XpArtLoader::XpSequence>* cachedSequence = cache.find(canonicalPath);
            if (cachedSequence != nullptr && *cachedSequence != nullptr)
            {
                SequenceLoadResult result;
                result.sequence = *cachedSequence;
                result.success = true;
                result.canonicalPath = canonicalPath;
                result.loadResult.sequence = **cachedSequence;
                result.loadResult.success = true;
                result.loadResult.manifestPath = canonicalPath;
                result.loadResult.resolvedVersion = 1;
                result.loadResult.resolvedFrameCount = (*cachedSequence)->getFrameCount();
                return result;
            }
        }

        SequenceLoadResult result = loadSequenceFromFile(filePath, options);
        if (result.success && result.sequence != nullptr && !result.canonicalPath.empty())
        {
            result.sequence = cache.store(result.canonicalPath, result.sequence);
        }

        return result;
    }

    FrameLookupResult getDefaultFrame(const XpArtLoader::XpSequence& sequence)
    {
        if (!sequence.isValid())
        {
            return makeFrameLookupFailure(
                &sequence,
                -1,
                FrameLookupCode::SequenceInvalid,
                "Cannot retrieve a default frame from an invalid XP sequence.");
        }

        const XpArtLoader::XpFrame* frame = sequence.getDefaultFrame();
        if (frame == nullptr)
        {
            return makeFrameLookupFailure(
                &sequence,
                -1,
                FrameLookupCode::DefaultFrameUnavailable,
                "XP sequence does not contain a default frame.");
        }

        FrameLookupResult result;
        result.sequence = &sequence;
        result.frame = frame;
        result.success = true;
        result.requestedFrameIndex = frame->frameIndex;
        return result;
    }

    FrameLookupResult getDefaultFrame(const std::shared_ptr<XpArtLoader::XpSequence>& sequence)
    {
        if (sequence == nullptr)
        {
            return makeFrameLookupFailure(
                nullptr,
                -1,
                FrameLookupCode::SequenceUnavailable,
                "XP sequence is unavailable.");
        }

        return getDefaultFrame(*sequence);
    }

    FrameLookupResult getFrameByIndex(
        const XpArtLoader::XpSequence& sequence,
        const int frameIndex)
    {
        if (!sequence.isValid())
        {
            return makeFrameLookupFailure(
                &sequence,
                frameIndex,
                FrameLookupCode::SequenceInvalid,
                "Cannot retrieve a frame from an invalid XP sequence.");
        }

        if (frameIndex < 0)
        {
            return makeFrameLookupFailure(
                &sequence,
                frameIndex,
                FrameLookupCode::NegativeFrameIndex,
                "Frame index must be zero or greater.");
        }

        const XpArtLoader::XpFrame* frame = sequence.findFrameByIndex(frameIndex);
        if (frame == nullptr)
        {
            return makeFrameLookupFailure(
                &sequence,
                frameIndex,
                FrameLookupCode::FrameIndexNotFound,
                "XP sequence does not contain frame index " + std::to_string(frameIndex) + ".");
        }

        FrameLookupResult result;
        result.sequence = &sequence;
        result.frame = frame;
        result.success = true;
        result.requestedFrameIndex = frameIndex;
        return result;
    }

    FrameLookupResult getFrameByIndex(
        const std::shared_ptr<XpArtLoader::XpSequence>& sequence,
        const int frameIndex)
    {
        if (sequence == nullptr)
        {
            return makeFrameLookupFailure(
                nullptr,
                frameIndex,
                FrameLookupCode::SequenceUnavailable,
                "XP sequence is unavailable.");
        }

        return getFrameByIndex(*sequence, frameIndex);
    }

    XpArtLoader::LoadResult buildTextObjectFromDefaultFrame(
        const XpArtLoader::XpSequence& sequence,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        return buildTextObjectFromFrameLookup(getDefaultFrame(sequence), options);
    }

    XpArtLoader::LoadResult buildTextObjectFromDefaultFrame(
        const std::shared_ptr<XpArtLoader::XpSequence>& sequence,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        return buildTextObjectFromFrameLookup(getDefaultFrame(sequence), options);
    }

    XpArtLoader::LoadResult buildTextObjectFromFrameByIndex(
        const XpArtLoader::XpSequence& sequence,
        const int frameIndex,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        return buildTextObjectFromFrameLookup(getFrameByIndex(sequence, frameIndex), options);
    }

    XpArtLoader::LoadResult buildTextObjectFromFrameByIndex(
        const std::shared_ptr<XpArtLoader::XpSequence>& sequence,
        const int frameIndex,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        return buildTextObjectFromFrameLookup(getFrameByIndex(sequence, frameIndex), options);
    }

    const char* toString(const FrameLookupCode code)
    {
        switch (code)
        {
        case FrameLookupCode::None:
            return "None";
        case FrameLookupCode::SequenceUnavailable:
            return "SequenceUnavailable";
        case FrameLookupCode::SequenceInvalid:
            return "SequenceInvalid";
        case FrameLookupCode::NegativeFrameIndex:
            return "NegativeFrameIndex";
        case FrameLookupCode::FrameIndexNotFound:
            return "FrameIndexNotFound";
        case FrameLookupCode::DefaultFrameUnavailable:
            return "DefaultFrameUnavailable";
        }

        return "Unknown";
    }
}
