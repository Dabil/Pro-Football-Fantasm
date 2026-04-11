#include "Utilities/AssetPaths.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

namespace
{
    std::string toLower(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });
        return value;
    }

    std::string weaklyCanonicalString(const std::filesystem::path& path)
    {
        std::error_code error;
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
        if (!error)
        {
            return canonical.generic_string();
        }

        return path.lexically_normal().generic_string();
    }

    std::vector<std::filesystem::path> buildCandidatePaths(
        const std::filesystem::path& input,
        const AssetPaths::ResolutionOptions& options)
    {
        std::vector<std::filesystem::path> candidates;
        candidates.reserve(8);

        if (input.is_absolute())
        {
            if (options.allowAbsolutePaths)
            {
                candidates.push_back(input);
            }

            return candidates;
        }

        if (!options.assetsRoot.empty())
        {
            candidates.push_back(std::filesystem::path(options.assetsRoot) / input);
        }

        for (const std::string& searchRoot : options.searchRoots)
        {
            if (!searchRoot.empty())
            {
                candidates.push_back(std::filesystem::path(searchRoot) / input);
            }
        }

        candidates.push_back(input);
        return candidates;
    }
}

namespace AssetPaths
{
    std::string normalizePath(std::string path)
    {
        if (path.empty())
        {
            return path;
        }

        std::replace(path.begin(), path.end(), '\\', '/');

        std::error_code error;
        const std::filesystem::path filesystemPath(path);
        const std::filesystem::path normalized = filesystemPath.lexically_normal();
        if (!error)
        {
            return normalized.generic_string();
        }

        return filesystemPath.generic_string();
    }

    std::string normalizeExtension(std::string extension)
    {
        extension = normalizePath(std::move(extension));
        if (!extension.empty() && extension.front() == '.')
        {
            extension.erase(extension.begin());
        }

        return toLower(std::move(extension));
    }

    std::string getExtension(const std::string& path)
    {
        return normalizeExtension(std::filesystem::path(path).extension().string());
    }

    std::string getStem(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    bool hasSupportedExtension(const std::string& path)
    {
        return detectAssetTypeByExtension(path) != AssetType::Unknown;
    }

    bool isTextObjectAssetType(AssetType assetType)
    {
        switch (assetType)
        {
        case AssetType::PlainText:
        case AssetType::AnsiArt:
        case AssetType::BinaryArt:
        case AssetType::XpDocument:
        case AssetType::XpSequence:
            return true;

        case AssetType::Unknown:
        case AssetType::FontSource:
        case AssetType::BannerSource:
        default:
            return false;
        }
    }

    AssetType detectAssetTypeByExtension(const std::string& path)
    {
        static const std::unordered_set<std::string> plainTextExtensions =
        {
            "txt", "asc", "nfo", "diz"
        };

        static const std::unordered_set<std::string> ansiExtensions =
        {
            "ans"
        };

        static const std::unordered_set<std::string> binaryExtensions =
        {
            "bin", "xbin", "adf"
        };

        static const std::unordered_set<std::string> fontExtensions =
        {
            "fon", "fnt"
        };

        static const std::unordered_set<std::string> bannerExtensions =
        {
            "flf", "tlf"
        };

        const std::string extension = getExtension(path);
        if (extension.empty())
        {
            return AssetType::Unknown;
        }

        if (plainTextExtensions.count(extension) > 0)
        {
            return AssetType::PlainText;
        }

        if (ansiExtensions.count(extension) > 0)
        {
            return AssetType::AnsiArt;
        }

        if (binaryExtensions.count(extension) > 0)
        {
            return AssetType::BinaryArt;
        }

        if (extension == "xp")
        {
            return AssetType::XpDocument;
        }

        if (extension == "xpseq")
        {
            return AssetType::XpSequence;
        }

        if (fontExtensions.count(extension) > 0)
        {
            return AssetType::FontSource;
        }

        if (bannerExtensions.count(extension) > 0)
        {
            return AssetType::BannerSource;
        }

        return AssetType::Unknown;
    }

    std::string resolveAssetsRoot()
    {
        return resolveAssetsRoot(std::filesystem::current_path().string());
    }

    std::string resolveAssetsRoot(const std::string& startDirectory)
    {
        std::filesystem::path current =
            startDirectory.empty()
            ? std::filesystem::current_path()
            : std::filesystem::path(startDirectory);

        std::error_code error;
        if (!std::filesystem::exists(current, error))
        {
            current = std::filesystem::current_path();
        }

        current = current.lexically_normal();

        while (!current.empty())
        {
            const std::filesystem::path candidate = current / "Assets";
            if (std::filesystem::exists(candidate, error) && std::filesystem::is_directory(candidate, error))
            {
                return weaklyCanonicalString(candidate);
            }

            const std::filesystem::path parent = current.parent_path();
            if (parent == current)
            {
                break;
            }

            current = parent;
        }

        return weaklyCanonicalString(std::filesystem::current_path() / "Assets");
    }

    ResolutionResult resolveAssetPath(
        const std::string& assetPath,
        const ResolutionOptions& options)
    {
        ResolutionResult result;
        result.requestedPath = assetPath;
        result.assetsRoot = options.assetsRoot.empty()
            ? resolveAssetsRoot()
            : normalizePath(options.assetsRoot);

        if (assetPath.empty())
        {
            result.errorMessage = "Asset path is empty.";
            return result;
        }

        result.assetType = detectAssetTypeByExtension(assetPath);
        if (result.assetType == AssetType::Unknown)
        {
            result.errorMessage = "Asset type could not be determined from extension.";
            return result;
        }

        const std::filesystem::path inputPath(normalizePath(assetPath));
        const std::vector<std::filesystem::path> candidates = buildCandidatePaths(inputPath, options);

        for (const std::filesystem::path& candidate : candidates)
        {
            if (candidate.empty())
            {
                continue;
            }

            if (!options.requireExistingPath)
            {
                result.success = true;
                result.resolvedPath = candidate.lexically_normal().generic_string();
                result.normalizedPath = weaklyCanonicalString(candidate);
                return result;
            }

            std::error_code error;
            if (std::filesystem::exists(candidate, error) && !error)
            {
                result.success = true;
                result.resolvedPath = candidate.lexically_normal().generic_string();
                result.normalizedPath = weaklyCanonicalString(candidate);
                return result;
            }
        }

        result.errorMessage = "Asset path could not be resolved from the configured asset roots.";
        return result;
    }

    const char* toString(AssetType assetType)
    {
        switch (assetType)
        {
        case AssetType::PlainText:
            return "PlainText";

        case AssetType::AnsiArt:
            return "AnsiArt";

        case AssetType::BinaryArt:
            return "BinaryArt";

        case AssetType::XpDocument:
            return "XpDocument";

        case AssetType::XpSequence:
            return "XpSequence";

        case AssetType::FontSource:
            return "FontSource";

        case AssetType::BannerSource:
            return "BannerSource";

        case AssetType::Unknown:
        default:
            return "Unknown";
        }
    }
}
