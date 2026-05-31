#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Rendering/ScreenCell.h"
#include "UI/Content/TileGrid.h"

namespace TileGridLoader
{
    struct LoadOptions
    {
        char delimiter = ',';
        char commentMarker = '#';

        bool trimWhitespace = true;
        bool allowQuotedFields = true;
        bool allowComments = true;
        bool skipBlankLines = true;
        bool requireConsistentWidth = true;

        ScreenCell defaultCell;
        ScreenCell fallbackTokenCell;

        bool useFallbackForUnknownTokens = false;

        std::unordered_map<std::string, ScreenCell> tokenPalette;
    };

    struct LoadError
    {
        int line = 0;
        int column = 0;
        std::string message;
    };

    struct LoadResult
    {
        TileGrid grid;
        bool success = false;
        std::vector<LoadError> errors;

        bool hasGrid() const
        {
            return success && !grid.empty();
        }

        std::string errorMessage() const;
    };

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options = {});
    LoadResult loadFromString(const std::string& text, const LoadOptions& options = {});

    bool tryLoadFromFile(const std::string& filePath, TileGrid& outGrid, const LoadOptions& options = {});
    bool tryLoadFromString(const std::string& text, TileGrid& outGrid, const LoadOptions& options = {});
}