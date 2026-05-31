#pragma once

#include <string>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/XpArtLoader.h"
#include "Utilities/AssetPaths.h"

namespace Assets
{
    class AssetLibrary;
}

namespace Composition
{
    struct ObjectSource
    {
        enum class Kind
        {
            None,
            TextObject,
            RegisteredFrame,
            AssetReference,
            XpDocument,
            XpFrame,
            XpSequence
        };

        Kind kind = Kind::None;

        const TextObject* textObject = nullptr;
        const std::vector<TextObject>* registeredFrames = nullptr;

        const XpArtLoader::XpDocument* xpDocument = nullptr;
        const XpArtLoader::XpFrame* xpFrame = nullptr;
        const XpArtLoader::XpSequence* xpSequence = nullptr;

        std::string assetName;
        int frameIndex = -1;

        XpArtLoader::LoadOptions xpDocumentOptions;
        XpArtLoader::XpFrameConversionOptions xpFrameOptions;

        static ObjectSource fromTextObject(const TextObject& object);
        static ObjectSource fromRegisteredFrame(
            const std::vector<TextObject>& frames,
            int frameIndex);
        static ObjectSource fromAsset(std::string assetName);
        static ObjectSource fromAssetFrame(std::string assetName, int frameIndex);
        static ObjectSource fromXpDocument(
            const XpArtLoader::XpDocument& document,
            const XpArtLoader::LoadOptions& options = {});
        static ObjectSource fromXpFrame(
            const XpArtLoader::XpFrame& frame,
            const XpArtLoader::XpFrameConversionOptions& options = {});
        static ObjectSource fromXpSequence(
            const XpArtLoader::XpSequence& sequence,
            int frameIndex = -1,
            const XpArtLoader::XpFrameConversionOptions& options = {});
    };

    struct ResolvedObjectSource
    {
        TextObject object;
        bool success = false;
        bool usedXpConversion = false;
        bool usedAssetLibrary = false;
        bool requiresAssetLibrary = false;
        AssetPaths::AssetType assetType = AssetPaths::AssetType::Unknown;
        int resolvedFrameIndex = -1;
        std::string errorMessage;

        bool hasObject() const
        {
            return success && object.isLoaded();
        }
    };

    ResolvedObjectSource resolveObjectSource(
        const ObjectSource& source,
        const Assets::AssetLibrary* assetLibrary = nullptr);
}

