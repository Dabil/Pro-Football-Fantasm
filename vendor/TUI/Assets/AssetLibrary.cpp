#include "Assets/AssetLibrary.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace
{
    std::shared_ptr<TextObject> makeSharedTextObject(TextObject object)
    {
        return std::make_shared<TextObject>(std::move(object));
    }

    std::shared_ptr<AsciiBannerFont> makeSharedBannerFont(AsciiBannerFont font)
    {
        return std::make_shared<AsciiBannerFont>(std::move(font));
    }

    std::shared_ptr<PseudoFont::FontDefinition> makeSharedPseudoFont(
        PseudoFont::FontDefinition fontDefinition)
    {
        return std::make_shared<PseudoFont::FontDefinition>(std::move(fontDefinition));
    }

    XpArtLoader::XpFrameConversionOptions resolveXpFrameConversionOptions(
        const Assets::AssetLibraryOptions& options)
    {
        XpArtLoader::XpFrameConversionOptions resolved = options.xpFrameConversionOptions;
        resolved.loadOptions = options.xpLoadOptions;
        return resolved;
    }

    Assets::LoadTextAssetResult makeTextFailure(
        const std::string& requestedReference,
        const std::string& assetKey,
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        const std::string& errorMessage)
    {
        Assets::LoadTextAssetResult result;
        result.success = false;
        result.asset.requestedReference = requestedReference;
        result.asset.assetKey = assetKey;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        result.errorMessage = errorMessage;
        return result;
    }

    Assets::LoadTextAssetResult makeTextSuccess(
        const std::string& requestedReference,
        const std::string& assetKey,
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        std::shared_ptr<TextObject> object)
    {
        Assets::LoadTextAssetResult result;
        result.success = true;
        result.asset.object = std::move(object);
        result.asset.requestedReference = requestedReference;
        result.asset.assetKey = assetKey;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }

    Assets::LoadBannerFontResult makeBannerFontFailure(
        const std::string& requestedReference,
        const std::string& assetKey,
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        const std::string& errorMessage)
    {
        Assets::LoadBannerFontResult result;
        result.success = false;
        result.asset.requestedReference = requestedReference;
        result.asset.assetKey = assetKey;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        result.errorMessage = errorMessage;
        return result;
    }

    Assets::LoadBannerFontResult makeBannerFontSuccess(
        const std::string& requestedReference,
        const std::string& assetKey,
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        std::shared_ptr<AsciiBannerFont> font)
    {
        Assets::LoadBannerFontResult result;
        result.success = true;
        result.asset.font = std::move(font);
        result.asset.requestedReference = requestedReference;
        result.asset.assetKey = assetKey;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        return result;
    }

    Assets::LoadPseudoFontAssetResult makePseudoFontFailure(
        const std::string& requestedReference,
        const std::string& assetKey,
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        const std::string& errorMessage)
    {
        Assets::LoadPseudoFontAssetResult result;
        result.success = false;
        result.asset.requestedReference = requestedReference;
        result.asset.assetKey = assetKey;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
        result.asset.resolvedPath = resolution.resolvedPath;
        result.asset.normalizedPath = resolution.normalizedPath;
        result.errorMessage = errorMessage;
        return result;
    }

    Assets::LoadPseudoFontAssetResult makePseudoFontSuccess(
        const std::string& requestedReference,
        const std::string& assetKey,
        const std::string& requestedPath,
        const AssetPaths::ResolutionResult& resolution,
        std::shared_ptr<PseudoFont::FontDefinition> font)
    {
        Assets::LoadPseudoFontAssetResult result;
        result.success = true;
        result.asset.font = std::move(font);
        result.asset.requestedReference = requestedReference;
        result.asset.assetKey = assetKey;
        result.asset.requestedPath = requestedPath;
        result.asset.assetType = resolution.assetType;
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
        m_options.assetsRoot = assetsRoot.empty()
            ? AssetPaths::resolveAssetsRoot()
            : AssetPaths::normalizePath(assetsRoot);
    }

    const std::string& AssetLibrary::getAssetsRoot() const
    {
        return m_options.assetsRoot;
    }

    void AssetLibrary::registerAsset(const std::string& assetKey, const std::string& assetPath)
    {
        const std::string normalizedKey = normalizeAssetKey(assetKey);
        const std::string normalizedPath = AssetPaths::normalizePath(assetPath);

        if (normalizedKey.empty() || normalizedPath.empty())
        {
            return;
        }

        m_aliases[normalizedKey] = normalizedPath;
        m_preferredAssetKeysByPath[normalizedPath] = assetKey;
    }

    void AssetLibrary::registerAlias(const std::string& assetName, const std::string& assetPathOrAssetKey)
    {
        const std::string normalizedAlias = normalizeAssetKey(assetName);
        if (normalizedAlias.empty() || assetPathOrAssetKey.empty())
        {
            return;
        }

        const std::string resolvedPath = resolveAssetPathFromReference(assetPathOrAssetKey);
        if (resolvedPath.empty())
        {
            return;
        }

        m_aliases[normalizedAlias] = resolvedPath;
    }

    void AssetLibrary::registerAliases(
        const std::string& AssetKey,
        const std::vector<std::string>& aliases)
    {
        for (const std::string& alias : aliases)
        {
            registerAlias(alias, AssetKey);
        }
    }

    bool AssetLibrary::hasAlias(const std::string& assetName) const
    {
        const std::string normalizedAlias = normalizeAssetKey(assetName);
        return m_aliases.find(normalizedAlias) != m_aliases.end();
    }

    bool AssetLibrary::removeAlias(const std::string& assetName)
    {
        const std::string normalizedAlias = normalizeAssetKey(assetName);
        if (normalizedAlias.empty())
        {
            return false;
        }

        return m_aliases.erase(normalizedAlias) > 0;
    }

    void AssetLibrary::clearAliases()
    {
        m_aliases.clear();
        m_preferredAssetKeysByPath.clear();
    }

    std::string AssetLibrary::getRegisteredAssetPath(const std::string& assetNameOrPath) const
    {
        return resolveAssetPathFromReference(assetNameOrPath);
    }

    std::string AssetLibrary::getPreferredAssetKeyForPath(const std::string& assetPath) const
    {
        const std::string normalizedPath = AssetPaths::normalizePath(assetPath);
        const auto it = m_preferredAssetKeysByPath.find(normalizedPath);
        if (it == m_preferredAssetKeysByPath.end())
        {
            return std::string();
        }

        return it->second;
    }

    LoadAssetResult AssetLibrary::loadAsset(const std::string& assetNameOrPath) const
    {
        return loadAssetInternal(assetNameOrPath, false);
    }

    LoadAssetResult AssetLibrary::reloadAsset(const std::string& assetNameOrPath) const
    {
        return loadAssetInternal(assetNameOrPath, true);
    }

    LoadTextAssetResult AssetLibrary::loadTextAsset(const std::string& assetNameOrPath) const
    {
        return loadTextAssetInternal(assetNameOrPath, false);
    }

    LoadTextAssetResult AssetLibrary::reloadTextAsset(const std::string& assetNameOrPath) const
    {
        return loadTextAssetInternal(assetNameOrPath, true);
    }

    LoadBannerFontResult AssetLibrary::loadBannerFontAsset(const std::string& assetNameOrPath) const
    {
        return loadBannerFontAssetInternal(assetNameOrPath, false);
    }

    LoadBannerFontResult AssetLibrary::reloadBannerFontAsset(const std::string& assetNameOrPath) const
    {
        return loadBannerFontAssetInternal(assetNameOrPath, true);
    }

    LoadPseudoFontAssetResult AssetLibrary::loadPseudoFontAsset(const std::string& assetNameOrPath) const
    {
        return loadPseudoFontAssetInternal(assetNameOrPath, false);
    }

    LoadPseudoFontAssetResult AssetLibrary::reloadPseudoFontAsset(const std::string& assetNameOrPath) const
    {
        return loadPseudoFontAssetInternal(assetNameOrPath, true);
    }

    const std::shared_ptr<TextObject>* AssetLibrary::findCachedTextAsset(
        const std::string& assetNameOrPath) const
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

    const std::shared_ptr<AsciiBannerFont>* AssetLibrary::findCachedBannerFontAsset(
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

    const std::shared_ptr<PseudoFont::FontDefinition>* AssetLibrary::findCachedPseudoFontAsset(
        const std::string& assetNameOrPath) const
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return nullptr;
        }

        const auto it = m_pseudoFontCache.find(cacheKey);
        if (it == m_pseudoFontCache.end() || !it->second.loadSucceeded)
        {
            return nullptr;
        }

        return &it->second.font;
    }

    bool AssetLibrary::evictCachedAsset(const std::string& assetNameOrPath)
    {
        const std::string requestedPath = resolveAssetPathFromReference(assetNameOrPath);
        if (requestedPath.empty())
        {
            return false;
        }

        const AssetPaths::ResolutionResult resolution = resolveAssetPath(requestedPath);
        if (!resolution.success)
        {
            return false;
        }

        switch (resolution.assetType)
        {
        case AssetPaths::AssetType::PlainText:
        case AssetPaths::AssetType::AnsiArt:
        case AssetPaths::AssetType::BinaryArt:
        case AssetPaths::AssetType::XpDocument:
        case AssetPaths::AssetType::XpSequence:
            return evictCachedTextAsset(assetNameOrPath);

        case AssetPaths::AssetType::BannerSource:
            return evictCachedBannerFontAsset(assetNameOrPath);

        case AssetPaths::AssetType::FontSource:
            return evictCachedPseudoFontAsset(assetNameOrPath);

        case AssetPaths::AssetType::Unknown:
        default:
            return false;
        }
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

    bool AssetLibrary::evictCachedBannerFontAsset(const std::string& assetNameOrPath)
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return false;
        }

        return m_bannerFontCache.erase(cacheKey) > 0;
    }

    bool AssetLibrary::evictCachedPseudoFontAsset(const std::string& assetNameOrPath)
    {
        const std::string cacheKey = makeCacheKey(assetNameOrPath);
        if (cacheKey.empty())
        {
            return false;
        }

        return m_pseudoFontCache.erase(cacheKey) > 0;
    }

    void AssetLibrary::clear()
    {
        m_textAssetCache.clear();
        m_bannerFontCache.clear();
        m_pseudoFontCache.clear();
    }

    std::size_t AssetLibrary::getCachedTextAssetCount() const
    {
        return m_textAssetCache.size();
    }

    std::size_t AssetLibrary::getCachedBannerFontAssetCount() const
    {
        return m_bannerFontCache.size();
    }

    std::size_t AssetLibrary::getCachedPseudoFontAssetCount() const
    {
        return m_pseudoFontCache.size();
    }

    std::size_t AssetLibrary::getAliasCount() const
    {
        return m_aliases.size();
    }

    LoadAssetResult AssetLibrary::loadAssetInternal(
        const std::string& assetNameOrPath,
        const bool forceReload) const
    {
        const ResolvedAssetReference reference = resolveAssetReference(assetNameOrPath);
        if (reference.requestedPath.empty())
        {
            LoadAssetResult result;
            result.success = false;
            result.errorMessage = "Asset name/path is empty.";
            result.asset.requestedReference = reference.requestedReference;
            result.asset.assetKey = reference.assetKey;
            result.asset.requestedPath = reference.requestedPath;
            return result;
        }

        const AssetPaths::ResolutionResult resolution = resolveAssetPath(reference.requestedPath);
        if (!resolution.success)
        {
            LoadAssetResult result;
            result.success = false;
            result.errorMessage = resolution.errorMessage;
            result.asset.requestedReference = reference.requestedReference;
            result.asset.assetKey = reference.assetKey;
            result.asset.requestedPath = reference.requestedPath;
            result.asset.assetType = resolution.assetType;
            result.asset.resolvedPath = resolution.resolvedPath;
            result.asset.normalizedPath = resolution.normalizedPath;
            return result;
        }

        switch (resolution.assetType)
        {
        case AssetPaths::AssetType::PlainText:
        case AssetPaths::AssetType::AnsiArt:
        case AssetPaths::AssetType::BinaryArt:
        case AssetPaths::AssetType::XpDocument:
        case AssetPaths::AssetType::XpSequence:
            return makeUnifiedResult(loadTextAssetInternal(assetNameOrPath, forceReload));

        case AssetPaths::AssetType::BannerSource:
            return makeUnifiedResult(loadBannerFontAssetInternal(assetNameOrPath, forceReload));

        case AssetPaths::AssetType::FontSource:
            return makeUnifiedResult(loadPseudoFontAssetInternal(assetNameOrPath, forceReload));

        case AssetPaths::AssetType::Unknown:
        default:
        {
            LoadAssetResult result;
            result.success = false;
            result.errorMessage = "Unsupported asset type.";
            result.asset.requestedReference = reference.requestedReference;
            result.asset.assetKey = reference.assetKey;
            result.asset.requestedPath = reference.requestedPath;
            result.asset.assetType = resolution.assetType;
            result.asset.resolvedPath = resolution.resolvedPath;
            result.asset.normalizedPath = resolution.normalizedPath;
            return result;
        }
        }
    }

    LoadTextAssetResult AssetLibrary::loadTextAssetInternal(
        const std::string& assetNameOrPath,
        const bool forceReload) const
    {
        const ResolvedAssetReference reference = resolveAssetReference(assetNameOrPath);
        if (reference.requestedPath.empty())
        {
            return makeTextFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                {},
                "Asset name/path is empty.");
        }

        const AssetPaths::ResolutionResult resolution = resolveAssetPath(reference.requestedPath);
        if (!resolution.success)
        {
            return makeTextFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                resolution.errorMessage);
        }

        const std::string cacheKey = resolution.normalizedPath;
        if (!forceReload)
        {
            const auto cacheIt = m_textAssetCache.find(cacheKey);
            if (cacheIt != m_textAssetCache.end())
            {
                LoadTextAssetResult result;
                result.success = cacheIt->second.loadSucceeded;
                result.fromCache = true;
                result.asset.object = cacheIt->second.object;
                result.asset.assetType = cacheIt->second.assetType;
                result.asset.requestedReference = cacheIt->second.requestedReference;
                result.asset.assetKey = cacheIt->second.assetKey;
                result.asset.requestedPath = cacheIt->second.requestedPath;
                result.asset.resolvedPath = cacheIt->second.resolvedPath;
                result.asset.normalizedPath = cacheIt->second.normalizedPath;
                result.asset.cacheKey = cacheKey;
                result.errorMessage = cacheIt->second.errorMessage;
                return result;
            }
        }

        LoadTextAssetResult result = dispatchTextLoad(reference, resolution);
        result.asset.cacheKey = cacheKey;

        if (result.success || m_options.cacheFailures)
        {
            TextCacheEntry& entry = m_textAssetCache[cacheKey];
            entry.object = result.asset.object;
            entry.assetType = result.asset.assetType;
            entry.requestedReference = result.asset.requestedReference;
            entry.assetKey = result.asset.assetKey;
            entry.requestedPath = result.asset.requestedPath;
            entry.resolvedPath = result.asset.resolvedPath;
            entry.normalizedPath = result.asset.normalizedPath;
            entry.loadSucceeded = result.success;
            entry.errorMessage = result.errorMessage;
        }

        return result;
    }

    LoadBannerFontResult AssetLibrary::loadBannerFontAssetInternal(
        const std::string& assetNameOrPath,
        const bool forceReload) const
    {
        const ResolvedAssetReference reference = resolveAssetReference(assetNameOrPath);
        if (reference.requestedPath.empty())
        {
            return makeBannerFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                {},
                "Asset name/path is empty.");
        }

        const AssetPaths::ResolutionResult resolution = resolveAssetPath(reference.requestedPath);
        if (!resolution.success)
        {
            return makeBannerFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                resolution.errorMessage);
        }

        const std::string cacheKey = resolution.normalizedPath;
        if (!forceReload)
        {
            const auto cacheIt = m_bannerFontCache.find(cacheKey);
            if (cacheIt != m_bannerFontCache.end())
            {
                LoadBannerFontResult result;
                result.success = cacheIt->second.loadSucceeded;
                result.fromCache = true;
                result.asset.font = cacheIt->second.font;
                result.asset.assetType = cacheIt->second.assetType;
                result.asset.requestedReference = cacheIt->second.requestedReference;
                result.asset.assetKey = cacheIt->second.assetKey;
                result.asset.requestedPath = cacheIt->second.requestedPath;
                result.asset.resolvedPath = cacheIt->second.resolvedPath;
                result.asset.normalizedPath = cacheIt->second.normalizedPath;
                result.asset.cacheKey = cacheKey;
                result.errorMessage = cacheIt->second.errorMessage;
                return result;
            }
        }

        LoadBannerFontResult result = dispatchBannerFontLoad(reference, resolution);
        result.asset.cacheKey = cacheKey;

        if (result.success || m_options.cacheFailures)
        {
            BannerFontCacheEntry& entry = m_bannerFontCache[cacheKey];
            entry.font = result.asset.font;
            entry.assetType = result.asset.assetType;
            entry.requestedReference = result.asset.requestedReference;
            entry.assetKey = result.asset.assetKey;
            entry.requestedPath = result.asset.requestedPath;
            entry.resolvedPath = result.asset.resolvedPath;
            entry.normalizedPath = result.asset.normalizedPath;
            entry.loadSucceeded = result.success;
            entry.errorMessage = result.errorMessage;
        }

        return result;
    }

    LoadPseudoFontAssetResult AssetLibrary::loadPseudoFontAssetInternal(
        const std::string& assetNameOrPath,
        const bool forceReload) const
    {
        const ResolvedAssetReference reference = resolveAssetReference(assetNameOrPath);
        if (reference.requestedPath.empty())
        {
            return makePseudoFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                {},
                "Asset name/path is empty.");
        }

        const AssetPaths::ResolutionResult resolution = resolveAssetPath(reference.requestedPath);
        if (!resolution.success)
        {
            return makePseudoFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                resolution.errorMessage);
        }

        const std::string cacheKey = resolution.normalizedPath;
        if (!forceReload)
        {
            const auto cacheIt = m_pseudoFontCache.find(cacheKey);
            if (cacheIt != m_pseudoFontCache.end())
            {
                LoadPseudoFontAssetResult result;
                result.success = cacheIt->second.loadSucceeded;
                result.fromCache = true;
                result.asset.font = cacheIt->second.font;
                result.asset.assetType = cacheIt->second.assetType;
                result.asset.requestedReference = cacheIt->second.requestedReference;
                result.asset.assetKey = cacheIt->second.assetKey;
                result.asset.requestedPath = cacheIt->second.requestedPath;
                result.asset.resolvedPath = cacheIt->second.resolvedPath;
                result.asset.normalizedPath = cacheIt->second.normalizedPath;
                result.asset.cacheKey = cacheKey;
                result.errorMessage = cacheIt->second.errorMessage;
                return result;
            }
        }

        LoadPseudoFontAssetResult result = dispatchPseudoFontLoad(reference, resolution);
        result.asset.cacheKey = cacheKey;

        if (result.success || m_options.cacheFailures)
        {
            PseudoFontCacheEntry& entry = m_pseudoFontCache[cacheKey];
            entry.font = result.asset.font;
            entry.assetType = result.asset.assetType;
            entry.requestedReference = result.asset.requestedReference;
            entry.assetKey = result.asset.assetKey;
            entry.requestedPath = result.asset.requestedPath;
            entry.resolvedPath = result.asset.resolvedPath;
            entry.normalizedPath = result.asset.normalizedPath;
            entry.loadSucceeded = result.success;
            entry.errorMessage = result.errorMessage;
        }

        return result;
    }

    LoadTextAssetResult AssetLibrary::dispatchTextLoad(
        const ResolvedAssetReference& reference,
        const AssetPaths::ResolutionResult& resolution) const
    {
        switch (resolution.assetType)
        {
        case AssetPaths::AssetType::PlainText:
        {
            const PlainTextLoader::LoadResult loadResult =
                PlainTextLoader::loadFromFile(resolution.normalizedPath, m_options.plainTextOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    "Plain text load failed.");
            }

            return makeTextSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::AnsiArt:
        {
            const AnsiLoader::LoadResult loadResult =
                AnsiLoader::loadFromFile(resolution.normalizedPath, m_options.ansiOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    AnsiLoader::formatLoadError(loadResult));
            }

            return makeTextSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::BinaryArt:
        {
            const BinaryArtLoader::LoadResult loadResult =
                BinaryArtLoader::loadFromFile(resolution.normalizedPath, m_options.binaryArtOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    BinaryArtLoader::formatLoadError(loadResult));
            }

            return makeTextSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedTextObject(loadResult.object));
        }

        case AssetPaths::AssetType::XpDocument:
        {
            const XpArtLoader::LoadResult loadResult =
                XpArtLoader::loadFromFile(resolution.normalizedPath, m_options.xpLoadOptions);

            if (!loadResult.success || !loadResult.object.isLoaded())
            {
                return makeTextFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    XpArtLoader::formatLoadError(loadResult));
            }

            return makeTextSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedTextObject(loadResult.object));
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
                return makeTextFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    sequenceLoad.errorMessage);
            }

            const XpArtLoader::LoadResult buildResult =
                XpSequenceAccess::buildTextObjectFromDefaultFrame(
                    sequenceLoad.sequence,
                    resolveXpFrameConversionOptions(m_options));

            if (!buildResult.success || !buildResult.object.isLoaded())
            {
                return makeTextFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    buildResult.errorMessage.empty()
                    ? "XP sequence default frame conversion failed."
                    : buildResult.errorMessage);
            }

            return makeTextSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedTextObject(buildResult.object));
        }

        case AssetPaths::AssetType::BannerSource:
            return makeTextFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Requested asset is a banner font asset. Use loadAsset(...) or loadBannerFontAsset(...).");

        case AssetPaths::AssetType::FontSource:
            return makeTextFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Requested asset is a pFont asset. Use loadAsset(...) or loadPseudoFontAsset(...).");

        case AssetPaths::AssetType::Unknown:
        default:
            return makeTextFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Unsupported asset type.");
        }
    }

    LoadBannerFontResult AssetLibrary::dispatchBannerFontLoad(
        const ResolvedAssetReference& reference,
        const AssetPaths::ResolutionResult& resolution) const
    {
        switch (resolution.assetType)
        {
        case AssetPaths::AssetType::BannerSource:
        {
            const AsciiBannerFontLoader::LoadResult loadResult =
                AsciiBannerFontLoader::loadFromFile(
                    resolution.normalizedPath,
                    m_options.bannerFontLoadOptions);

            if (!loadResult.success)
            {
                return makeBannerFontFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    loadResult.errorMessage.empty()
                    ? "Banner font load failed."
                    : loadResult.errorMessage);
            }

            return makeBannerFontSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedBannerFont(std::move(loadResult.font)));
        }

        case AssetPaths::AssetType::PlainText:
        case AssetPaths::AssetType::AnsiArt:
        case AssetPaths::AssetType::BinaryArt:
        case AssetPaths::AssetType::XpDocument:
        case AssetPaths::AssetType::XpSequence:
            return makeBannerFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Requested asset is not a banner font asset. Use loadAsset(...) or loadTextAsset(...) for this asset type.");

        case AssetPaths::AssetType::FontSource:
            return makeBannerFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Requested asset is a pFont asset. Use loadAsset(...) or loadPseudoFontAsset(...).");

        case AssetPaths::AssetType::Unknown:
        default:
            return makeBannerFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Unsupported asset type.");
        }
    }

    LoadPseudoFontAssetResult AssetLibrary::dispatchPseudoFontLoad(
        const ResolvedAssetReference& reference,
        const AssetPaths::ResolutionResult& resolution) const
    {
        switch (resolution.assetType)
        {
        case AssetPaths::AssetType::FontSource:
        {
            const PseudoFont::LoadResult loadResult =
                PseudoFont::loadFromFile(resolution.normalizedPath, m_options.pFontLoadOptions);

            if (!loadResult.success)
            {
                return makePseudoFontFailure(
                    reference.requestedReference,
                    reference.assetKey,
                    reference.requestedPath,
                    resolution,
                    loadResult.errorMessage.empty()
                    ? "pFont load failed."
                    : loadResult.errorMessage);
            }

            return makePseudoFontSuccess(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                makeSharedPseudoFont(std::move(loadResult.font)));
        }

        case AssetPaths::AssetType::PlainText:
        case AssetPaths::AssetType::AnsiArt:
        case AssetPaths::AssetType::BinaryArt:
        case AssetPaths::AssetType::XpDocument:
        case AssetPaths::AssetType::XpSequence:
            return makePseudoFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Requested asset is not a pFont asset. Use loadAsset(...) or loadTextAsset(...) for this asset type.");

        case AssetPaths::AssetType::BannerSource:
            return makePseudoFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Requested asset is a banner font asset. Use loadAsset(...) or loadBannerFontAsset(...).");

        case AssetPaths::AssetType::Unknown:
        default:
            return makePseudoFontFailure(
                reference.requestedReference,
                reference.assetKey,
                reference.requestedPath,
                resolution,
                "Unsupported asset type.");
        }
    }

    LoadAssetResult AssetLibrary::makeUnifiedResult(const LoadTextAssetResult& result) const
    {
        LoadAssetResult unified;
        unified.success = result.success;
        unified.fromCache = result.fromCache;
        unified.errorMessage = result.errorMessage;
        unified.asset.kind = AssetLoadKind::TextObject;
        unified.asset.assetType = result.asset.assetType;
        unified.asset.requestedReference = result.asset.requestedReference;
        unified.asset.assetKey = result.asset.assetKey;
        unified.asset.requestedPath = result.asset.requestedPath;
        unified.asset.resolvedPath = result.asset.resolvedPath;
        unified.asset.normalizedPath = result.asset.normalizedPath;
        unified.asset.cacheKey = result.asset.cacheKey;
        unified.asset.textObject = result.asset.object;
        return unified;
    }

    LoadAssetResult AssetLibrary::makeUnifiedResult(const LoadBannerFontResult& result) const
    {
        LoadAssetResult unified;
        unified.success = result.success;
        unified.fromCache = result.fromCache;
        unified.errorMessage = result.errorMessage;
        unified.asset.kind = AssetLoadKind::BannerFont;
        unified.asset.assetType = result.asset.assetType;
        unified.asset.requestedReference = result.asset.requestedReference;
        unified.asset.assetKey = result.asset.assetKey;
        unified.asset.requestedPath = result.asset.requestedPath;
        unified.asset.resolvedPath = result.asset.resolvedPath;
        unified.asset.normalizedPath = result.asset.normalizedPath;
        unified.asset.cacheKey = result.asset.cacheKey;
        unified.asset.bannerFont = result.asset.font;
        return unified;
    }

    LoadAssetResult AssetLibrary::makeUnifiedResult(const LoadPseudoFontAssetResult& result) const
    {
        LoadAssetResult unified;
        unified.success = result.success;
        unified.fromCache = result.fromCache;
        unified.errorMessage = result.errorMessage;
        unified.asset.kind = AssetLoadKind::PseudoFont;
        unified.asset.assetType = result.asset.assetType;
        unified.asset.requestedReference = result.asset.requestedReference;
        unified.asset.assetKey = result.asset.assetKey;
        unified.asset.requestedPath = result.asset.requestedPath;
        unified.asset.resolvedPath = result.asset.resolvedPath;
        unified.asset.normalizedPath = result.asset.normalizedPath;
        unified.asset.cacheKey = result.asset.cacheKey;
        unified.asset.pseudoFont = result.asset.font;
        return unified;
    }

    AssetPaths::ResolutionResult AssetLibrary::resolveAssetPath(const std::string& requestedPath) const
    {
        AssetPaths::ResolutionOptions resolutionOptions;
        resolutionOptions.assetsRoot = m_options.assetsRoot;
        resolutionOptions.searchRoots.push_back(m_options.assetsRoot);
        return AssetPaths::resolveAssetPath(requestedPath, resolutionOptions);
    }

    AssetLibrary::ResolvedAssetReference AssetLibrary::resolveAssetReference(
        const std::string& assetNameOrPath) const
    {
        ResolvedAssetReference resolved;
        resolved.requestedReference = assetNameOrPath;
        resolved.requestedPath = resolveAssetPathFromReference(assetNameOrPath);

        const std::string normalizedKey = normalizeAssetKey(assetNameOrPath);
        const auto aliasIt = m_aliases.find(normalizedKey);
        if (aliasIt != m_aliases.end())
        {
            const auto preferredIt = m_preferredAssetKeysByPath.find(aliasIt->second);
            if (preferredIt != m_preferredAssetKeysByPath.end())
            {
                resolved.assetKey = preferredIt->second;
            }
            else
            {
                resolved.assetKey = assetNameOrPath;
            }

            return resolved;
        }

        const std::string normalizedPath = AssetPaths::normalizePath(assetNameOrPath);
        const auto preferredIt = m_preferredAssetKeysByPath.find(normalizedPath);
        if (preferredIt != m_preferredAssetKeysByPath.end())
        {
            resolved.assetKey = preferredIt->second;
        }

        return resolved;
    }

    std::string AssetLibrary::resolveAssetPathFromReference(const std::string& assetNameOrPath) const
    {
        if (assetNameOrPath.empty())
        {
            return std::string();
        }

        const std::string normalizedKey = normalizeAssetKey(assetNameOrPath);
        const auto aliasIt = m_aliases.find(normalizedKey);
        if (aliasIt != m_aliases.end())
        {
            return aliasIt->second;
        }

        return AssetPaths::normalizePath(assetNameOrPath);
    }

    std::string AssetLibrary::makeCacheKey(const std::string& assetNameOrPath) const
    {
        const std::string requestedPath = resolveAssetPathFromReference(assetNameOrPath);
        if (requestedPath.empty())
        {
            return std::string();
        }

        const AssetPaths::ResolutionResult resolution = resolveAssetPath(requestedPath);
        if (!resolution.success)
        {
            return std::string();
        }

        return resolution.normalizedPath;
    }

    std::string AssetLibrary::normalizeAssetKey(const std::string& assetKey)
    {
        std::string normalized = trimString(assetKey);
        std::transform(
            normalized.begin(),
            normalized.end(),
            normalized.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return normalized;
    }

    std::string AssetLibrary::trimString(const std::string& value)
    {
        std::size_t begin = 0;
        while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])))
        {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])))
        {
            --end;
        }

        return value.substr(begin, end - begin);
    }
}