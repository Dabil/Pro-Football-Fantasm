#include "Assets/AssetLibrary.h"

#include <utility>

namespace
{
    std::shared_ptr<TextObject> makeSharedTextObject(TextObject object)
    {
        return std::make_shared<TextObject>(std::move(object));
    }

    Data::Assets::LoadTextAssetResult makeFailure(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        const std::string& errorMessage)
    {
        Data::Assets::LoadTextAssetResult result;
        result.success = false;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        result.errorMessage = errorMessage;
        return result;
    }

    Data::Assets::LoadTextAssetResult makeSuccess(
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        std::shared_ptr<TextObject> object)
    {
        Data::Assets::LoadTextAssetResult result;
        result.success = true;
        result.asset.object = std::move(object);
        result.asset.assetType = resolution.assetType;
        result.asset.requestedPath = requestedPath;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }
}

namespace Data::Assets
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
        m_options.assetsRoot = assetsRoot.empty()
            ? AssetPaths::resolveAssetsRoot()
            : AssetPaths::normalizePath(assetsRoot);
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

    bool AssetLibrary::evictCachedTextAsset(const std::string& assetNameOrPath)
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return false;
        }

        return m_textAssetCache.erase(cacheKey) > 0;
    }

    void AssetLibrary::clear()
    {
        m_textAssetCache.clear();
    }

    std::size_t AssetLibrary::getCachedTextAssetCount() const
    {
        return m_textAssetCache.size();
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
            return makeFailure(assetNameOrPath, {}, "Asset name/path is empty.");
        }

        AssetPaths::ResolutionOptions resolutionOptions;
        resolutionOptions.assetsRoot = m_options.assetsRoot;
        resolutionOptions.searchRoots.push_back(m_options.assetsRoot);

        const AssetPaths::ResolutionResult resolution =
            AssetPaths::resolveAssetPath(requestedPath, resolutionOptions);

        if (!resolution.success)
        {
            return makeFailure(requestedPath, resolution, resolution.errorMessage);
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
            CacheEntry& entry = m_textAssetCache[cacheKey];
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
                return makeFailure(requestedPath, resolution, "Plain text asset load failed.");
            }

            return makeSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::AnsiArt:
        {
            const AnsiLoader::LoadResult loadResult =
                AnsiLoader::loadFromFile(resolution.normalizedPath, m_options.ansiOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeFailure(requestedPath, resolution, AnsiLoader::formatLoadError(loadResult));
            }

            return makeSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::BinaryArt:
        {
            const BinaryArtLoader::LoadResult loadResult =
                BinaryArtLoader::loadFromFile(resolution.normalizedPath, m_options.binaryArtOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeFailure(requestedPath, resolution, BinaryArtLoader::formatLoadError(loadResult));
            }

            return makeSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::XpDocument:
        {
            const XpArtLoader::LoadResult loadResult =
                XpArtLoader::loadFromFile(resolution.normalizedPath, m_options.xpLoadOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeFailure(requestedPath, resolution, XpArtLoader::formatLoadError(loadResult));
            }

            return makeSuccess(requestedPath, resolution, makeSharedTextObject(loadResult.object));
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
                return makeFailure(requestedPath, resolution, sequenceLoad.errorMessage);
            }

            const XpArtLoader::LoadResult buildResult =
                XpSequenceAccess::buildTextObjectFromDefaultFrame(
                    sequenceLoad.sequence,
                    {});

            if (!buildResult.success || !buildResult.object.isLoaded())
            {
                return makeFailure(
                    requestedPath,
                    resolution,
                    buildResult.errorMessage.empty()
                        ? "XP sequence default frame conversion failed."
                        : buildResult.errorMessage);
            }

            return makeSuccess(requestedPath, resolution, makeSharedTextObject(buildResult.object));
        }

        case AssetPaths::AssetType::FontSource:
            return makeFailure(
                requestedPath,
                resolution,
                "Font source was recognized by extension, but Phase 3.7 does not yet have a central font loader implementation.");

        case AssetPaths::AssetType::BannerSource:
            return makeFailure(
                requestedPath,
                resolution,
                "Banner source was recognized by extension, but Phase 3.7 does not yet have a central banner loader implementation.");

        case AssetPaths::AssetType::Unknown:
        default:
            return makeFailure(requestedPath, resolution, "Unsupported asset type.");
        }
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
