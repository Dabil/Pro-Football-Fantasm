#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "Assets/Loaders/AsciiBannerFontLoader.h"
#include "Assets/Models/AsciiBannerFont.h"
#include "Rendering/Objects/AnsiLoader.h"
#include "Rendering/Objects/BinaryArtLoader.h"
#include "Rendering/Objects/PlainTextLoader.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/XpArtLoader.h"
#include "Rendering/Objects/XpSequenceAccess.h"
#include "Utilities/AssetPaths.h"

namespace Assets
{
    struct AssetLibraryOptions
    {
        std::string assetsRoot;
        bool cacheFailures = false;

        PlainTextLoader::LoadOptions plainTextOptions;
        AnsiLoader::LoadOptions ansiOptions;
        BinaryArtLoader::LoadOptions binaryArtOptions;
        XpArtLoader::LoadOptions xpLoadOptions;
        XpSequenceAccess::SequenceLoadOptions xpSequenceOptions;
        AsciiBannerFontLoader::LoadOptions bannerFontOptions;
    };

    struct LoadedTextAsset
    {
        std::shared_ptr<TextObject> object;
        AssetPaths::AssetType assetType = AssetPaths::AssetType::Unknown;
        std::string requestedPath;
        std::string resolvedPath;
        std::string normalizedPath;
        std::string cacheKey;
    };

    struct LoadTextAssetResult
    {
        LoadedTextAsset asset;
        bool success = false;
        bool fromCache = false;
        std::string errorMessage;

        bool hasObject() const
        {
            return asset.object != nullptr;
        }
    };

    struct LoadedBannerFontAsset
    {
        std::shared_ptr<const AsciiBannerFont> font;
        AssetPaths::AssetType assetType = AssetPaths::AssetType::BannerSource;
        std::string requestedPath;
        std::string resolvedPath;
        std::string normalizedPath;
        std::string cacheKey;
    };

    struct LoadBannerFontResult
    {
        LoadedBannerFontAsset asset;
        bool success = false;
        bool fromCache = false;
        std::string errorMessage;

        bool hasFont() const
        {
            return asset.font != nullptr;
        }
    };

    class AssetLibrary
    {
    public:
        explicit AssetLibrary(AssetLibraryOptions options = {});

        const AssetLibraryOptions& getOptions() const;
        void setOptions(const AssetLibraryOptions& options);

        void setAssetsRoot(const std::string& assetsRoot);
        const std::string& getAssetsRoot() const;

        void registerAlias(const std::string& assetName, const std::string& assetPath);
        bool hasAlias(const std::string& assetName) const;
        bool removeAlias(const std::string& assetName);
        void clearAliases();

        LoadTextAssetResult loadTextAsset(const std::string& assetNameOrPath);
        LoadTextAssetResult reloadTextAsset(const std::string& assetNameOrPath);

        LoadBannerFontResult loadBannerFont(const std::string& assetNameOrPath);
        LoadBannerFontResult reloadBannerFont(const std::string& assetNameOrPath);

        const std::shared_ptr<TextObject>* findCachedTextAsset(const std::string& assetNameOrPath) const;
        const std::shared_ptr<const AsciiBannerFont>* findCachedBannerFont(const std::string& assetNameOrPath) const;

        bool evictCachedTextAsset(const std::string& assetNameOrPath);
        bool evictCachedBannerFont(const std::string& assetNameOrPath);

        void clear();

        std::size_t getCachedTextAssetCount() const;
        std::size_t getCachedBannerFontCount() const;
        std::size_t getAliasCount() const;

    private:
        struct TextCacheEntry
        {
            std::shared_ptr<TextObject> object;
            AssetPaths::AssetType assetType = AssetPaths::AssetType::Unknown;
            std::string requestedPath;
            std::string resolvedPath;
            std::string normalizedPath;
            bool loadSucceeded = false;
            std::string errorMessage;
        };

        struct BannerFontCacheEntry
        {
            std::shared_ptr<const AsciiBannerFont> font;
            AssetPaths::AssetType assetType = AssetPaths::AssetType::BannerSource;
            std::string requestedPath;
            std::string resolvedPath;
            std::string normalizedPath;
            bool loadSucceeded = false;
            std::string errorMessage;
        };

        LoadTextAssetResult loadTextAssetInternal(const std::string& assetNameOrPath, bool forceReload);
        LoadBannerFontResult loadBannerFontInternal(const std::string& assetNameOrPath, bool forceReload);

        LoadTextAssetResult dispatchLoad(
            const std::string& requestedPath,
            const AssetPaths::ResolutionResult& resolution);

        LoadBannerFontResult dispatchLoadBannerFont(
            const std::string& requestedPath,
            const AssetPaths::ResolutionResult& resolution);

        std::string resolveAssetReference(const std::string& assetNameOrPath) const;
        std::string makeCacheKey(const std::string& assetNameOrPath) const;

    private:
        AssetLibraryOptions m_options;
        std::unordered_map<std::string, std::string> m_aliases;
        std::unordered_map<std::string, TextCacheEntry> m_textAssetCache;
        std::unordered_map<std::string, BannerFontCacheEntry> m_bannerFontCache;
    };
}
