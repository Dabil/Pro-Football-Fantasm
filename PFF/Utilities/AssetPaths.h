#pragma once

#include <string>
#include <vector>

namespace AssetPaths
{
    enum class AssetType
    {
        Unknown,
        PlainText,
        AnsiArt,
        BinaryArt,
        XpDocument,
        XpSequence,
        FontSource,
        BannerSource
    };

    struct ResolutionOptions
    {
        std::string assetsRoot;
        std::vector<std::string> searchRoots;
        bool allowAbsolutePaths = true;
        bool requireExistingPath = true;
    };

    struct ResolutionResult
    {
        bool success = false;
        AssetType assetType = AssetType::Unknown;
        std::string requestedPath;
        std::string resolvedPath;
        std::string normalizedPath;
        std::string assetsRoot;
        std::string errorMessage;
    };

    std::string normalizePath(std::string path);
    std::string normalizeExtension(std::string extension);
    std::string getExtension(const std::string& path);
    std::string getStem(const std::string& path);

    bool hasSupportedExtension(const std::string& path);
    bool isTextObjectAssetType(AssetType assetType);

    AssetType detectAssetTypeByExtension(const std::string& path);

    std::string resolveAssetsRoot();
    std::string resolveAssetsRoot(const std::string& startDirectory);

    ResolutionResult resolveAssetPath(
        const std::string& assetPath,
        const ResolutionOptions& options = {});

    const char* toString(AssetType assetType);
}
