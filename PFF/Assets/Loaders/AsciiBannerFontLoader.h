#pragma once

#include <optional>
#include <string>

#include "Assets/Models/AsciiBannerFont.h"

namespace AsciiBannerFontLoader
{
    enum class LoadWarningCode
    {
        None,
        InvalidCodetagBlockIgnored,
        InvalidUtf8DecodedAsCp437
    };

    struct LoadWarning
    {
        LoadWarningCode code = LoadWarningCode::None;
        std::string message;

        bool isValid() const
        {
            return code != LoadWarningCode::None;
        }
    };

    struct LoadOptions
    {
        bool decodeNonUtf8GlyphRowsAsCp437 = true;
        bool loadCodetagGlyphs = true;
        bool preserveUnknownCodetagBlocks = false;
    };

    struct LoadResult
    {
        AsciiBannerFont font;
        bool success = false;
        bool decodedNonUtf8GlyphRows = false;
        std::vector<LoadWarning> warnings;
        std::string errorMessage;

        bool hasFont() const
        {
            return font.isLoaded();
        }
    };

    LoadResult loadFromFile(const std::string& filePath);
    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options);

    bool tryLoadFromFile(const std::string& filePath, AsciiBannerFont& outFont);
    bool tryLoadFromFile(const std::string& filePath, AsciiBannerFont& outFont, const LoadOptions& options);

    std::string formatLoadError(const LoadResult& result);

    const char* toString(LoadWarningCode warningCode);
}
