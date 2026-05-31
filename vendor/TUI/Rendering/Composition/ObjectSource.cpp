#include "Rendering/Composition/ObjectSource.h"

#include <utility>

#include "Assets/AssetLibrary.h"
#include "Rendering/Objects/XpSequenceAnimationAdapter.h"

namespace
{
    XpArtLoader::XpFrameConversionOptions resolveAssetFrameOptions(
        const Composition::ObjectSource& source,
        const Assets::AssetLibrary* assetLibrary)
    {
        if (!source.xpFrameOptions.isEmpty())
        {
            return source.xpFrameOptions;
        }

        if (assetLibrary != nullptr)
        {
            return assetLibrary->getOptions().xpFrameConversionOptions;
        }

        return {};
    }

    AssetPaths::ResolutionResult resolveAssetReference(
        const Assets::AssetLibrary& assetLibrary,
        const std::string& assetName)
    {
        std::string requestedPath = assetLibrary.getRegisteredAssetPath(assetName);
        if (requestedPath.empty())
        {
            requestedPath = assetName;
        }

        AssetPaths::ResolutionOptions options;
        options.assetsRoot = assetLibrary.getAssetsRoot();
        options.searchRoots.push_back(assetLibrary.getAssetsRoot());
        return AssetPaths::resolveAssetPath(requestedPath, options);
    }

    Composition::ResolvedObjectSource makeFailure(
        std::string message,
        bool requiresAssetLibrary = false,
        bool usedAssetLibrary = false,
        AssetPaths::AssetType assetType = AssetPaths::AssetType::Unknown)
    {
        Composition::ResolvedObjectSource result;
        result.success = false;
        result.requiresAssetLibrary = requiresAssetLibrary;
        result.usedAssetLibrary = usedAssetLibrary;
        result.assetType = assetType;
        result.errorMessage = std::move(message);
        return result;
    }

    Composition::ResolvedObjectSource makeSuccess(
        TextObject object,
        bool usedXpConversion,
        bool usedAssetLibrary,
        AssetPaths::AssetType assetType,
        int resolvedFrameIndex)
    {
        Composition::ResolvedObjectSource result;
        result.object = std::move(object);
        result.success = result.object.isLoaded();
        result.usedXpConversion = usedXpConversion;
        result.usedAssetLibrary = usedAssetLibrary;
        result.assetType = assetType;
        result.resolvedFrameIndex = resolvedFrameIndex;
        if (!result.success)
        {
            result.errorMessage = "Resolved object source did not produce a loaded TextObject.";
        }
        return result;
    }
}

namespace Composition
{
    ObjectSource ObjectSource::fromTextObject(const TextObject& object)
    {
        ObjectSource source;
        source.kind = Kind::TextObject;
        source.textObject = &object;
        return source;
    }

    ObjectSource ObjectSource::fromRegisteredFrame(
        const std::vector<TextObject>& frames,
        int frameIndex)
    {
        ObjectSource source;
        source.kind = Kind::RegisteredFrame;
        source.registeredFrames = &frames;
        source.frameIndex = frameIndex;
        return source;
    }

    ObjectSource ObjectSource::fromAsset(std::string assetName)
    {
        ObjectSource source;
        source.kind = Kind::AssetReference;
        source.assetName = std::move(assetName);
        return source;
    }

    ObjectSource ObjectSource::fromAssetFrame(std::string assetName, int frameIndex)
    {
        ObjectSource source;
        source.kind = Kind::AssetReference;
        source.assetName = std::move(assetName);
        source.frameIndex = frameIndex;
        return source;
    }

    ObjectSource ObjectSource::fromXpDocument(
        const XpArtLoader::XpDocument& document,
        const XpArtLoader::LoadOptions& options)
    {
        ObjectSource source;
        source.kind = Kind::XpDocument;
        source.xpDocument = &document;
        source.xpDocumentOptions = options;
        return source;
    }

    ObjectSource ObjectSource::fromXpFrame(
        const XpArtLoader::XpFrame& frame,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        ObjectSource source;
        source.kind = Kind::XpFrame;
        source.xpFrame = &frame;
        source.xpFrameOptions = options;
        return source;
    }

    ObjectSource ObjectSource::fromXpSequence(
        const XpArtLoader::XpSequence& sequence,
        int frameIndex,
        const XpArtLoader::XpFrameConversionOptions& options)
    {
        ObjectSource source;
        source.kind = Kind::XpSequence;
        source.xpSequence = &sequence;
        source.frameIndex = frameIndex;
        source.xpFrameOptions = options;
        return source;
    }

    ResolvedObjectSource resolveObjectSource(
        const ObjectSource& source,
        const Assets::AssetLibrary* assetLibrary)
    {
        switch (source.kind)
        {
        case ObjectSource::Kind::TextObject:
            if (source.textObject == nullptr || !source.textObject->isLoaded())
            {
                return makeFailure("TextObject source is unavailable or not loaded.");
            }

            return makeSuccess(
                *source.textObject,
                false,
                false,
                AssetPaths::AssetType::Unknown,
                -1);

        case ObjectSource::Kind::RegisteredFrame:
            if (source.registeredFrames == nullptr)
            {
                return makeFailure("Registered frame source is unavailable.");
            }

            if (source.frameIndex < 0 ||
                static_cast<std::size_t>(source.frameIndex) >= source.registeredFrames->size())
            {
                return makeFailure("Requested registered frame index is out of range.");
            }

            if (!(*source.registeredFrames)[static_cast<std::size_t>(source.frameIndex)].isLoaded())
            {
                return makeFailure("Requested registered frame is not loaded.");
            }

            return makeSuccess(
                (*source.registeredFrames)[static_cast<std::size_t>(source.frameIndex)],
                false,
                false,
                AssetPaths::AssetType::Unknown,
                source.frameIndex);

        case ObjectSource::Kind::XpDocument:
            if (source.xpDocument == nullptr || !source.xpDocument->isValid())
            {
                return makeFailure("XP document source is unavailable or invalid.");
            }
            else
            {
                const XpArtLoader::LoadResult buildResult =
                    XpArtLoader::buildTextObjectFromXpDocument(
                        *source.xpDocument,
                        source.xpDocumentOptions);

                if (!buildResult.success || !buildResult.object.isLoaded())
                {
                    return makeFailure(
                        buildResult.errorMessage.empty()
                        ? "XP document conversion failed."
                        : buildResult.errorMessage);
                }

                return makeSuccess(
                    buildResult.object,
                    true,
                    false,
                    AssetPaths::AssetType::XpDocument,
                    -1);
            }

        case ObjectSource::Kind::XpFrame:
            if (source.xpFrame == nullptr || !source.xpFrame->isValid())
            {
                return makeFailure("XP frame source is unavailable or invalid.");
            }
            else
            {
                const XpArtLoader::LoadResult buildResult =
                    XpArtLoader::buildTextObjectFromXpFrame(
                        *source.xpFrame,
                        source.xpFrameOptions);

                if (!buildResult.success || !buildResult.object.isLoaded())
                {
                    return makeFailure(
                        buildResult.errorMessage.empty()
                        ? "XP frame conversion failed."
                        : buildResult.errorMessage);
                }

                return makeSuccess(
                    buildResult.object,
                    true,
                    false,
                    AssetPaths::AssetType::XpSequence,
                    source.xpFrame->frameIndex);
            }

        case ObjectSource::Kind::XpSequence:
            if (source.xpSequence == nullptr || !source.xpSequence->isValid())
            {
                return makeFailure("XP sequence source is unavailable or invalid.");
            }
            else
            {
                const XpSequenceAnimationAdapter::TextObjectFrameResult frameResult =
                    source.frameIndex >= 0
                    ? XpSequenceAnimationAdapter::buildTextObjectForFrame(
                        *source.xpSequence,
                        source.frameIndex,
                        source.xpFrameOptions)
                    : XpSequenceAnimationAdapter::buildTextObjectForFrame(
                        *source.xpSequence,
                        0,
                        source.xpFrameOptions);

                if (!frameResult.success || !frameResult.buildResult.object.isLoaded())
                {
                    return makeFailure(
                        frameResult.errorMessage.empty()
                        ? "XP sequence frame conversion failed."
                        : frameResult.errorMessage);
                }

                return makeSuccess(
                    frameResult.buildResult.object,
                    true,
                    false,
                    AssetPaths::AssetType::XpSequence,
                    frameResult.frameRequest.reference.frameIndex);
            }

        case ObjectSource::Kind::AssetReference:
            if (source.assetName.empty())
            {
                return makeFailure("Asset source name is empty.", true);
            }

            if (assetLibrary == nullptr)
            {
                return makeFailure(
                    "Asset source resolution requires an AssetLibrary.",
                    true,
                    false);
            }

            if (source.frameIndex < 0)
            {
                const Assets::LoadTextAssetResult loadResult =
                    assetLibrary->loadTextAsset(source.assetName);

                if (!loadResult.success || !loadResult.hasObject())
                {
                    return makeFailure(
                        loadResult.errorMessage.empty()
                        ? "AssetLibrary text asset resolution failed."
                        : loadResult.errorMessage,
                        true,
                        true,
                        loadResult.asset.assetType);
                }

                return makeSuccess(
                    *loadResult.asset.object,
                    loadResult.asset.assetType == AssetPaths::AssetType::XpDocument ||
                    loadResult.asset.assetType == AssetPaths::AssetType::XpSequence,
                    true,
                    loadResult.asset.assetType,
                    -1);
            }
            else
            {
                const AssetPaths::ResolutionResult resolution =
                    resolveAssetReference(*assetLibrary, source.assetName);

                if (!resolution.success)
                {
                    return makeFailure(
                        resolution.errorMessage.empty()
                        ? "Unable to resolve asset reference for frame selection."
                        : resolution.errorMessage,
                        true,
                        true,
                        resolution.assetType);
                }

                if (resolution.assetType != AssetPaths::AssetType::XpSequence)
                {
                    return makeFailure(
                        "Static frame selection by index is only supported for XP sequence assets.",
                        true,
                        true,
                        resolution.assetType);
                }

                XpSequenceAccess::LocalSequenceCache cache;
                const XpSequenceAccess::SequenceLoadResult sequenceLoad =
                    XpSequenceAccess::loadSequenceFromFile(
                        resolution.normalizedPath,
                        cache,
                        assetLibrary->getOptions().xpSequenceOptions);

                if (!sequenceLoad.success || !sequenceLoad.hasSequence())
                {
                    return makeFailure(
                        sequenceLoad.errorMessage.empty()
                        ? "XP sequence asset load failed."
                        : sequenceLoad.errorMessage,
                        true,
                        true,
                        AssetPaths::AssetType::XpSequence);
                }

                const XpArtLoader::XpFrameConversionOptions frameOptions =
                    resolveAssetFrameOptions(source, assetLibrary);

                const XpArtLoader::LoadResult buildResult =
                    XpSequenceAccess::buildTextObjectFromFrameByIndex(
                        sequenceLoad.sequence,
                        source.frameIndex,
                        frameOptions);

                if (!buildResult.success || !buildResult.object.isLoaded())
                {
                    return makeFailure(
                        buildResult.errorMessage.empty()
                        ? "XP sequence asset frame conversion failed."
                        : buildResult.errorMessage,
                        true,
                        true,
                        AssetPaths::AssetType::XpSequence);
                }

                return makeSuccess(
                    buildResult.object,
                    true,
                    true,
                    AssetPaths::AssetType::XpSequence,
                    source.frameIndex);
            }

        case ObjectSource::Kind::None:
        default:
            return makeFailure("Object source is not initialized.");
        }
    }
}

