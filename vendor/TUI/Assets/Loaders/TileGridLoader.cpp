#include "Assets/Loaders/TileGridLoader.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <sstream>
#include <utility>

namespace
{
    std::string trimCopy(const std::string& value)
    {
        std::size_t first = 0;
        while (first < value.size() &&
            std::isspace(static_cast<unsigned char>(value[first])))
        {
            ++first;
        }

        std::size_t last = value.size();
        while (last > first &&
            std::isspace(static_cast<unsigned char>(value[last - 1])))
        {
            --last;
        }

        return value.substr(first, last - first);
    }

    bool isBlankLine(const std::string& line)
    {
        for (char ch : line)
        {
            if (!std::isspace(static_cast<unsigned char>(ch)))
            {
                return false;
            }
        }

        return true;
    }

    bool isCommentLine(const std::string& line, const TileGridLoader::LoadOptions& options)
    {
        if (!options.allowComments)
        {
            return false;
        }

        const std::string trimmed = options.trimWhitespace ? trimCopy(line) : line;
        return !trimmed.empty() && trimmed.front() == options.commentMarker;
    }

    void addError(
        TileGridLoader::LoadResult& result,
        int line,
        int column,
        const std::string& message)
    {
        TileGridLoader::LoadError error;
        error.line = line;
        error.column = column;
        error.message = message;
        result.errors.push_back(error);
    }

    bool parseCsvLine(
        const std::string& line,
        int lineNumber,
        const TileGridLoader::LoadOptions& options,
        std::vector<std::string>& outFields,
        TileGridLoader::LoadResult& result)
    {
        outFields.clear();

        std::string field;
        bool inQuotes = false;

        for (std::size_t i = 0; i < line.size(); ++i)
        {
            const char ch = line[i];

            if (options.allowQuotedFields && ch == '"')
            {
                if (inQuotes && i + 1 < line.size() && line[i + 1] == '"')
                {
                    field.push_back('"');
                    ++i;
                    continue;
                }

                inQuotes = !inQuotes;
                continue;
            }

            if (!inQuotes && ch == options.delimiter)
            {
                outFields.push_back(options.trimWhitespace ? trimCopy(field) : field);
                field.clear();
                continue;
            }

            field.push_back(ch);
        }

        if (inQuotes)
        {
            addError(result, lineNumber, static_cast<int>(line.size()), "Unclosed quoted CSV field.");
            return false;
        }

        outFields.push_back(options.trimWhitespace ? trimCopy(field) : field);
        return true;
    }

    ScreenCell makeGlyphCell(char32_t glyph, const ScreenCell& templateCell)
    {
        ScreenCell cell = templateCell;
        cell.glyph = glyph;
        cell.cluster = std::u32string(1, glyph);
        cell.kind = CellKind::Glyph;
        cell.width = CellWidth::One;
        return cell;
    }

    ScreenCell tokenToCell(const std::string& token, const TileGridLoader::LoadOptions& options)
    {
        if (token.empty())
        {
            return options.defaultCell;
        }

        const auto paletteIt = options.tokenPalette.find(token);
        if (paletteIt != options.tokenPalette.end())
        {
            return paletteIt->second;
        }

        if (options.useFallbackForUnknownTokens)
        {
            return options.fallbackTokenCell;
        }

        return makeGlyphCell(
            static_cast<char32_t>(static_cast<unsigned char>(token.front())),
            options.defaultCell);
    }

    std::string stripUtf8Bom(const std::string& text)
    {
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            return text.substr(3);
        }

        return text;
    }
}

namespace TileGridLoader
{
    std::string LoadResult::errorMessage() const
    {
        if (errors.empty())
        {
            return {};
        }

        std::ostringstream stream;

        for (std::size_t i = 0; i < errors.size(); ++i)
        {
            const LoadError& error = errors[i];

            if (i > 0)
            {
                stream << "\n";
            }

            stream << "Line " << error.line;
            if (error.column > 0)
            {
                stream << ", column " << error.column;
            }

            stream << ": " << error.message;
        }

        return stream.str();
    }

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options)
    {
        LoadResult result;

        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            addError(result, 0, 0, "Unable to open grid file: " + filePath);
            return result;
        }

        const std::string bytes{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>() };

        return loadFromString(bytes, options);
    }

    LoadResult loadFromString(const std::string& text, const LoadOptions& options)
    {
        LoadResult result;
        const std::string normalizedText = stripUtf8Bom(text);

        std::istringstream stream(normalizedText);
        std::string line;
        std::vector<std::vector<std::string>> rows;

        int lineNumber = 0;
        int expectedWidth = -1;

        while (std::getline(stream, line))
        {
            ++lineNumber;

            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            if ((options.skipBlankLines && isBlankLine(line)) || isCommentLine(line, options))
            {
                continue;
            }

            std::vector<std::string> fields;
            if (!parseCsvLine(line, lineNumber, options, fields, result))
            {
                continue;
            }

            const int rowWidth = static_cast<int>(fields.size());
            if (expectedWidth < 0)
            {
                expectedWidth = rowWidth;
            }
            else if (options.requireConsistentWidth && rowWidth != expectedWidth)
            {
                addError(
                    result,
                    lineNumber,
                    0,
                    "Inconsistent row width. Expected " +
                    std::to_string(expectedWidth) +
                    " fields but found " +
                    std::to_string(rowWidth) + ".");
            }

            rows.push_back(std::move(fields));
        }

        if (rows.empty())
        {
            addError(result, 0, 0, "Grid file is empty or contains no loadable rows.");
        }

        if (!result.errors.empty())
        {
            return result;
        }

        const int height = static_cast<int>(rows.size());
        const int width = expectedWidth < 0 ? 0 : expectedWidth;

        result.grid.resize(width, height);
        result.grid.fill(options.defaultCell);

        for (int y = 0; y < height; ++y)
        {
            const std::vector<std::string>& row = rows[static_cast<std::size_t>(y)];

            for (int x = 0; x < static_cast<int>(row.size()); ++x)
            {
                result.grid.set(
                    x,
                    y,
                    tokenToCell(row[static_cast<std::size_t>(x)], options));
            }
        }

        result.success = true;
        return result;
    }

    bool tryLoadFromFile(const std::string& filePath, TileGrid& outGrid, const LoadOptions& options)
    {
        LoadResult result = loadFromFile(filePath, options);
        if (!result.success)
        {
            return false;
        }

        outGrid = std::move(result.grid);
        return true;
    }

    bool tryLoadFromString(const std::string& text, TileGrid& outGrid, const LoadOptions& options)
    {
        LoadResult result = loadFromString(text, options);
        if (!result.success)
        {
            return false;
        }

        outGrid = std::move(result.grid);
        return true;
    }
}