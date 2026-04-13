#include "Assets/AssetLibrary.h"

#include <utility>

namespace
{
    std::shared_ptr<TextObject> makeSharedTextObject(const TextObject& object)
    {
        return std::make_shared<TextObject>(object);
    }

    Assets::LoadTextAssetResult makeTextFailure(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        const std::string& errorMessage)
    {
        Assets::LoadTextAssetResult result;
        result.success = false;
        result.errorMessage = errorMessage;
        result.asset.assetType = resolution.assetType;
        result.asset.requestedPath = requestedPath;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }

    Assets::LoadTextAssetResult makeTextSuccess(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        std::shared_ptr<TextObject> object)
    {
        Assets::LoadTextAssetResult result;
        result.success = true;
        result.asset.object = std::move(object);
        result.asset.assetType = resolution.assetType;
        result.asset.requestedPath = requestedPath;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }

    Assets::LoadBannerFontResult makeBannerFailure(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        const std::string& errorMessage)
    {
        Assets::LoadBannerFontResult result;
        result.success = false;
        result.errorMessage = errorMessage;
        result.asset.assetType = AssetPaths::AssetType::BannerSource;
        result.asset.requestedPath = requestedPath;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }

    Assets::LoadBannerFontResult makeBannerSuccess(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        std::shared_ptr<const AsciiBannerFont> font)
    {
        Assets::LoadBannerFontResult result;
        result.success = true;
        result.asset.font = std::move(font);
        result.asset.assetType = AssetPaths::AssetType::BannerSource;
        result.asset.requestedPath = requestedPath;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }
}

namespace Assets
{
    AssetLibrary::AssetLibrary(AssetLibraryOptions options)
        : m_options(std::move(options))
    {
        if (m_options.assetsRoot.empty())
        {
            m_options.assetsRoot = AssetPaths::resolveAssetsRoot();
        }
    }

    const AssetLibraryOptions& AssetLibrary::getOptions() const
    {
        return m_options;
    }

    void AssetLibrary::setOptions(const AssetLibraryOptions& options)
    {
        m_options = options;
        if (m_options.assetsRoot.empty())
        {
            m_options.assetsRoot = AssetPaths::resolveAssetsRoot();
        }
    }

    void AssetLibrary::setAssetsRoot(const std::string& assetsRoot)
    {
        m_options.assetsRoot = assetsRoot;
    }

    const std::string& AssetLibrary::getAssetsRoot() const
    {
        return m_options.assetsRoot;
    }

    void AssetLibrary::registerAlias(const std::string& assetName, const std::string& assetPath)
    {
        if (assetName.empty() || assetPath.empty())
        {
            return;
        }

        m_aliases[assetName] = assetPath;
    }

    bool AssetLibrary::hasAlias(const std::string& assetName) const
    {
        return m_aliases.find(assetName) != m_aliases.end();
    }

    bool AssetLibrary::removeAlias(const std::string& assetName)
    {
        return m_aliases.erase(assetName) > 0;
    }

    void AssetLibrary::clearAliases()
    {
        m_aliases.clear();
    }

    LoadTextAssetResult AssetLibrary::loadTextAsset(const std::string& assetNameOrPath)
    {
        return loadTextAssetInternal(assetNameOrPath, false);
    }

    LoadTextAssetResult AssetLibrary::reloadTextAsset(const std::string& assetNameOrPath)
    {
        return loadTextAssetInternal(assetNameOrPath, true);
    }

    LoadBannerFontResult AssetLibrary::loadBannerFont(const std::string& assetNameOrPath)
    {
        return loadBannerFontInternal(assetNameOrPath, false);
    }

    LoadBannerFontResult AssetLibrary::reloadBannerFont(const std::string& assetNameOrPath)
    {
        return loadBannerFontInternal(assetNameOrPath, true);
    }

    const std::shared_ptr<TextObject>* AssetLibrary::findCachedTextAsset(const std::string& assetNameOrPath) const
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return nullptr;
        }

        const auto it = m_textAssetCache.find(cacheKey);
        if (it == m_textAssetCache.end() || !it->second.loadSucceeded)
        {
            return nullptr;
        }

        return &it->second.object;
    }

    const std::shared_ptr<const AsciiBannerFont>* AssetLibrary::findCachedBannerFont(
        const std::string& assetNameOrPath) const
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return nullptr;
        }

        const auto it = m_bannerFontCache.find(cacheKey);
        if (it == m_bannerFontCache.end() || !it->second.loadSucceeded)
        {
            return nullptr;
        }

        return &it->second.font;
    }

    bool AssetLibrary::evictCachedTextAsset(const std::string& assetNameOrPath)
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return false;
        }

        return m_textAssetCache.erase(cacheKey) > 0;
    }

    bool AssetLibrary::evictCachedBannerFont(const std::string& assetNameOrPath)
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return false;
        }

        return m_bannerFontCache.erase(cacheKey) > 0;
    }

    void AssetLibrary::clear()
    {
        m_textAssetCache.clear();
        m_bannerFontCache.clear();
    }

    std::size_t AssetLibrary::getCachedTextAssetCount() const
    {
        return m_textAssetCache.size();
    }

    std::size_t AssetLibrary::getCachedBannerFontCount() const
    {
        return m_bannerFontCache.size();
    }

    std::size_t AssetLibrary::getAliasCount() const
    {
        return m_aliases.size();
    }

    LoadTextAssetResult AssetLibrary::loadTextAssetInternal(
        const std::string& assetNameOrPath,
        const bool forceReload)
    {
        const std::string requestedPath = resolveAssetReference(assetNameOrPath);
        if (requestedPath.empty())
        {
            return makeTextFailure(assetNameOrPath, {}, "Asset name/path is empty.");
        }

        AssetPaths::ResolutionOptions resolutionOptions;
        resolutionOptions.assetsRoot = m_options.assetsRoot;
        resolutionOptions.searchRoots.push_back(m_options.assetsRoot);

        const AssetPaths::ResolutionResult resolution =
            AssetPaths::resolveAssetPath(requestedPath, resolutionOptions);

        if (!resolution.success)
        {
            return makeTextFailure(requestedPath, resolution, resolution.errorMessage);
        }

        const std::string cacheKey = resolution.normalizedPath;
        if (!forceReload)
        {
            const auto cacheIt = m_textAssetCache.find(cacheKey);
            if (cacheIt != m_textAssetCache.end() && cacheIt->second.loadSucceeded)
            {
                LoadTextAssetResult result;
                result.success = true;
                result.fromCache = true;
                result.asset.object = cacheIt->second.object;
                result.asset.assetType = cacheIt->second.assetType;
                result.asset.requestedPath = cacheIt->second.requestedPath;
                result.asset.resolvedPath = cacheIt->second.resolvedPath;
                result.asset.normalizedPath = cacheIt->second.normalizedPath;
                result.asset.cacheKey = cacheKey;
                return result;
            }
        }

        LoadTextAssetResult result = dispatchLoad(requestedPath, resolution);
        result.asset.cacheKey = cacheKey;

        if (result.success || m_options.cacheFailures)
        {
            TextCacheEntry& entry = m_textAssetCache[cacheKey];
            entry.object = result.asset.object;
            entry.assetType = result.asset.assetType;
            entry.requestedPath = result.asset.requestedPath;
            entry.resolvedPath = result.asset.resolvedPath;
            entry.normalizedPath = result.asset.normalizedPath;
            entry.loadSucceeded = result.success;
            entry.errorMessage = result.errorMessage;
        }

        return result;
    }

    LoadBannerFontResult AssetLibrary::loadBannerFontInternal(
        const std::string& assetNameOrPath,
        const bool forceReload)
    {
        const std::string requestedPath = resolveAssetReference(assetNameOrPath);
        if (requestedPath.empty())
        {
            return makeBannerFailure(assetNameOrPath, {}, "Asset name/path is empty.");
        }

        AssetPaths::ResolutionOptions resolutionOptions;
        resolutionOptions.assetsRoot = m_options.assetsRoot;
        resolutionOptions.searchRoots.push_back(m_options.assetsRoot);

        const AssetPaths::ResolutionResult resolution =
            AssetPaths::resolveAssetPath(requestedPath, resolutionOptions);

        if (!resolution.success)
        {
            return makeBannerFailure(requestedPath, resolution, resolution.errorMessage);
        }

        if (resolution.assetType != AssetPaths::AssetType::BannerSource)
        {
            return makeBannerFailure(
                requestedPath,
                resolution,
                "Requested asset does not resolve to a banner font source.");
        }

        const std::string cacheKey = resolution.normalizedPath;
        if (!forceReload)
        {
            const auto cacheIt = m_bannerFontCache.find(cacheKey);
            if (cacheIt != m_bannerFontCache.end() && cacheIt->second.loadSucceeded)
            {
                LoadBannerFontResult result;
                result.success = true;
                result.fromCache = true;
                result.asset.font = cacheIt->second.font;
                result.asset.assetType = cacheIt->second.assetType;
                result.asset.requestedPath = cacheIt->second.requestedPath;
                result.asset.resolvedPath = cacheIt->second.resolvedPath;
                result.asset.normalizedPath = cacheIt->second.normalizedPath;
                result.asset.cacheKey = cacheKey;
                return result;
            }
        }

        LoadBannerFontResult result = dispatchLoadBannerFont(requestedPath, resolution);
        result.asset.cacheKey = cacheKey;

        if (result.success || m_options.cacheFailures)
        {
            BannerFontCacheEntry& entry = m_bannerFontCache[cacheKey];
            entry.font = result.asset.font;
            entry.assetType = result.asset.assetType;
            entry.requestedPath = result.asset.requestedPath;
            entry.resolvedPath = result.asset.resolvedPath;
            entry.normalizedPath = result.asset.normalizedPath;
            entry.loadSucceeded = result.success;
            entry.errorMessage = result.errorMessage;
        }

        return result;
    }

    LoadTextAssetResult AssetLibrary::dispatchLoad(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution)
    {
        switch (resolution.assetType)
        {
        case AssetPaths::AssetType::PlainText:
        {
            const PlainTextLoader::LoadResult loadResult =
                PlainTextLoader::loadFromFile(resolution.normalizedPath, m_options.plainTextOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(requestedPath, resolution, "Plain text asset load failed.");
            }

            return makeTextSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::AnsiArt:
        {
            const AnsiLoader::LoadResult loadResult =
                AnsiLoader::loadFromFile(resolution.normalizedPath, m_options.ansiOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(requestedPath, resolution, AnsiLoader::formatLoadError(loadResult));
            }

            return makeTextSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::BinaryArt:
        {
            const BinaryArtLoader::LoadResult loadResult =
                BinaryArtLoader::loadFromFile(resolution.normalizedPath, m_options.binaryArtOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(requestedPath, resolution, BinaryArtLoader::formatLoadError(loadResult));
            }

            return makeTextSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::XpDocument:
        {
            const XpArtLoader::LoadResult loadResult =
                XpArtLoader::loadFromFile(resolution.normalizedPath, m_options.xpLoadOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(requestedPath, resolution, XpArtLoader::formatLoadError(loadResult));
            }

            return makeTextSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::XpSequence:
        {
            XpSequenceAccess::LocalSequenceCache sequenceCache;
            const XpSequenceAccess::SequenceLoadResult sequenceLoad =
                XpSequenceAccess::loadSequenceFromFile(
                    resolution.normalizedPath,
                    sequenceCache,
                    m_options.xpSequenceOptions);

            if (!sequenceLoad.success || !sequenceLoad.hasSequence())
            {
                return makeTextFailure(requestedPath, resolution, sequenceLoad.errorMessage);
            }

            const XpArtLoader::LoadResult buildResult =
                XpSequenceAccess::buildTextObjectFromDefaultFrame(
                    sequenceLoad.sequence,
                    {});

            if (!buildResult.success || !buildResult.object.isLoaded())
            {
                return makeTextFailure(
                    requestedPath,
                    resolution,
                    buildResult.errorMessage.empty()
                    ? "XP sequence default frame conversion failed."
                    : buildResult.errorMessage);
            }

            return makeTextSuccess(requestedPath, resolution, makeSharedTextObject(buildResult.object));
        }

        case AssetPaths::AssetType::FontSource:
            return makeTextFailure(
                requestedPath,
                resolution,
                "Font source was recognized by extension, but this AssetLibrary path only loads TextObject-compatible assets.");

        case AssetPaths::AssetType::BannerSource:
            return makeTextFailure(
                requestedPath,
                resolution,
                "Banner font assets must be loaded through loadBannerFont(...).");

        case AssetPaths::AssetType::Unknown:
        default:
            return makeTextFailure(requestedPath, resolution, "Unsupported asset type.");
        }
    }

    LoadBannerFontResult AssetLibrary::dispatchLoadBannerFont(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution)
    {
        const AsciiBannerFontLoader::LoadResult loadResult =
            AsciiBannerFontLoader::loadFromFile(
                resolution.normalizedPath,
                m_options.bannerFontOptions);

        if (!loadResult.success || !loadResult.hasFont())
        {
            return makeBannerFailure(
                requestedPath,
                resolution,
                loadResult.errorMessage.empty()
                ? "Banner font load failed."
                : loadResult.errorMessage);
        }

        return makeBannerSuccess(
            requestedPath,
            resolution,
            std::make_shared<const AsciiBannerFont>(loadResult.font));
    }

    std::string AssetLibrary::resolveAssetReference(const std::string& assetNameOrPath) const
    {
        const auto aliasIt = m_aliases.find(assetNameOrPath);
        if (aliasIt != m_aliases.end())
        {
            return aliasIt->second;
        }

        return assetNameOrPath;
    }

    std::string AssetLibrary::makeCacheKey(const std::string& assetNameOrPath) const
    {
        const std::string requestedPath = resolveAssetReference(assetNameOrPath);
        if (requestedPath.empty())
        {
            return std::string();
        }

        AssetPaths::ResolutionOptions resolutionOptions;
        resolutionOptions.assetsRoot = m_options.assetsRoot;
        resolutionOptions.searchRoots.push_back(m_options.assetsRoot);

        const AssetPaths::ResolutionResult resolution =
            AssetPaths::resolveAssetPath(requestedPath, resolutionOptions);
        if (!resolution.success)
        {
            return std::string();
        }

        return resolution.normalizedPath;
    }
}
