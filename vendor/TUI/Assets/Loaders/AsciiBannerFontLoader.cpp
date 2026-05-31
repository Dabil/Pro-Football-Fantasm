#include "Assets/Loaders/AsciiBannerFontLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace
{
    bool startsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() &&
            value.compare(0, prefix.size(), prefix) == 0;
    }

    std::string trimRight(const std::string& input)
    {
        std::string result = input;

        while (!result.empty() &&
            (result.back() == ' ' || result.back() == '\t' || result.back() == '\r'))
        {
            result.pop_back();
        }

        return result;
    }

    std::string trimEndMarker(const std::string& input, const char endMarker)
    {
        std::string result = input;

        while (!result.empty() && (result.back() == endMarker || result.back() == '\r'))
        {
            result.pop_back();
        }

        return result;
    }

    bool parseCodeTag(const std::string& line, int& codePointOut)
    {
        std::istringstream stream(line);
        std::string token;
        if (!(stream >> token))
        {
            return false;
        }

        try
        {
            std::size_t pos = 0;
            const long long value = std::stoll(token, &pos, 0);
            if (pos != token.size())
            {
                return false;
            }

            codePointOut = static_cast<int>(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool readRawLine(std::ifstream& file, std::string& out)
    {
        out.clear();

        char ch = '\0';
        while (file.get(ch))
        {
            if (ch == '\n')
            {
                break;
            }

            out.push_back(ch);
        }

        if (!out.empty() && out.back() == '\r')
        {
            out.pop_back();
        }

        return !out.empty() || static_cast<bool>(file);
    }

    bool isValidUtf8(std::string_view text)
    {
        std::size_t i = 0;

        while (i < text.size())
        {
            const unsigned char c0 = static_cast<unsigned char>(text[i]);

            if (c0 < 0x80)
            {
                ++i;
                continue;
            }

            if ((c0 & 0xE0) == 0xC0)
            {
                if (i + 1 >= text.size())
                {
                    return false;
                }

                const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                if ((c1 & 0xC0) != 0x80)
                {
                    return false;
                }

                i += 2;
                continue;
            }

            if ((c0 & 0xF0) == 0xE0)
            {
                if (i + 2 >= text.size())
                {
                    return false;
                }

                const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                const unsigned char c2 = static_cast<unsigned char>(text[i + 2]);

                if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
                {
                    return false;
                }

                i += 3;
                continue;
            }

            if ((c0 & 0xF8) == 0xF0)
            {
                if (i + 3 >= text.size())
                {
                    return false;
                }

                const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                const unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
                const unsigned char c3 = static_cast<unsigned char>(text[i + 3]);

                if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                {
                    return false;
                }

                i += 4;
                continue;
            }

            return false;
        }

        return true;
    }

    void appendUtf8(std::string& out, const char32_t cp)
    {
        if (cp <= 0x7F)
        {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF)
        {
            out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0xFFFF)
        {
            out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    char32_t cp437ToCodePoint(const unsigned char b)
    {
        static const char32_t table[256] =
        {
            0x0000,0x263A,0x263B,0x2665,0x2666,0x2663,0x2660,0x2022,
            0x25D8,0x25CB,0x25D9,0x2642,0x2640,0x266A,0x266B,0x263C,
            0x25BA,0x25C4,0x2195,0x203C,0x00B6,0x00A7,0x25AC,0x21A8,
            0x2191,0x2193,0x2192,0x2190,0x221F,0x2194,0x25B2,0x25BC,
            0x0020,0x0021,0x0022,0x0023,0x0024,0x0025,0x0026,0x0027,
            0x0028,0x0029,0x002A,0x002B,0x002C,0x002D,0x002E,0x002F,
            0x0030,0x0031,0x0032,0x0033,0x0034,0x0035,0x0036,0x0037,
            0x0038,0x0039,0x003A,0x003B,0x003C,0x003D,0x003E,0x003F,
            0x0040,0x0041,0x0042,0x0043,0x0044,0x0045,0x0046,0x0047,
            0x0048,0x0049,0x004A,0x004B,0x004C,0x004D,0x004E,0x004F,
            0x0050,0x0051,0x0052,0x0053,0x0054,0x0055,0x0056,0x0057,
            0x0058,0x0059,0x005A,0x005B,0x005C,0x005D,0x005E,0x005F,
            0x0060,0x0061,0x0062,0x0063,0x0064,0x0065,0x0066,0x0067,
            0x0068,0x0069,0x006A,0x006B,0x006C,0x006D,0x006E,0x006F,
            0x0070,0x0071,0x0072,0x0073,0x0074,0x0075,0x0076,0x0077,
            0x0078,0x0079,0x007A,0x007B,0x007C,0x007D,0x007E,0x2302,
            0x00C7,0x00FC,0x00E9,0x00E2,0x00E4,0x00E0,0x00E5,0x00E7,
            0x00EA,0x00EB,0x00E8,0x00EF,0x00EE,0x00EC,0x00C4,0x00C5,
            0x00C9,0x00E6,0x00C6,0x00F4,0x00F6,0x00F2,0x00FB,0x00F9,
            0x00FF,0x00D6,0x00DC,0x00A2,0x00A3,0x00A5,0x20A7,0x0192,
            0x00E1,0x00ED,0x00F3,0x00FA,0x00F1,0x00D1,0x00AA,0x00BA,
            0x00BF,0x2310,0x00AC,0x00BD,0x00BC,0x00A1,0x00AB,0x00BB,
            0x2591,0x2592,0x2593,0x2502,0x2524,0x2561,0x2562,0x2556,
            0x2555,0x2563,0x2551,0x2557,0x255D,0x255C,0x255B,0x2510,
            0x2514,0x2534,0x252C,0x251C,0x2500,0x253C,0x255E,0x255F,
            0x255A,0x2554,0x2569,0x2566,0x2560,0x2550,0x256C,0x2567,
            0x2568,0x2564,0x2565,0x2559,0x2558,0x2552,0x2553,0x256B,
            0x256A,0x2518,0x250C,0x2588,0x2584,0x258C,0x2590,0x2580,
            0x03B1,0x00DF,0x0393,0x03C0,0x03A3,0x03C3,0x00B5,0x03C4,
            0x03A6,0x0398,0x03A9,0x03B4,0x221E,0x03C6,0x03B5,0x2229,
            0x2261,0x00B1,0x2265,0x2264,0x2320,0x2321,0x00F7,0x2248,
            0x00B0,0x2219,0x00B7,0x221A,0x207F,0x00B2,0x25A0,0x00A0
        };

        return table[b];
    }

    std::string decodeFontLineBytes(
        const std::string_view raw,
        const bool allowCp437Fallback,
        bool& usedFallbackOut)
    {
        if (isValidUtf8(raw))
        {
            return std::string(raw);
        }

        if (!allowCp437Fallback)
        {
            return std::string();
        }

        usedFallbackOut = true;

        std::string utf8;
        utf8.reserve(raw.size() * 3);

        for (const unsigned char b : raw)
        {
            appendUtf8(utf8, cp437ToCodePoint(b));
        }

        return utf8;
    }

    void addWarning(
        AsciiBannerFontLoader::LoadResult& result,
        const AsciiBannerFontLoader::LoadWarningCode code,
        const std::string& message)
    {
        AsciiBannerFontLoader::LoadWarning warning;
        warning.code = code;
        warning.message = message;
        result.warnings.push_back(std::move(warning));
    }
}

namespace AsciiBannerFontLoader
{
    LoadResult loadFromFile(const std::string& filePath)
    {
        return loadFromFile(filePath, {});
    }

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options)
    {
        LoadResult result;

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            result.errorMessage = "Could not open banner font file: " + filePath;
            return result;
        }

        AsciiBannerFont font;
        font.setSourcePath(filePath);
        font.setName(std::filesystem::path(filePath).stem().string());
        font.setKind(
            std::filesystem::path(filePath).extension() == ".tlf"
            ? AsciiBannerFont::Kind::Toilet
            : AsciiBannerFont::Kind::FIGlet);

        std::string header;
        if (!readRawLine(file, header))
        {
            result.errorMessage = "Banner font file is empty: " + filePath;
            return result;
        }

        header = trimRight(header);

        const bool isFigletHeader = startsWith(header, "flf2a");
        const bool isToiletHeader = startsWith(header, "tlf2a");
        if ((!isFigletHeader && !isToiletHeader) || header.size() < 6)
        {
            result.errorMessage = "Banner font header is not a valid FLF/TLF header.";
            return result;
        }

        font.setHardBlank(header[5]);

        std::istringstream headerStream(header.substr(6));

        int height = 0;
        int baseline = 0;
        int maxLength = 0;
        int oldLayout = 0;
        int commentLines = 0;
        int printDirection = 0;
        int fullLayout = 0;
        int codetagCount = 0;

        headerStream
            >> height
            >> baseline
            >> maxLength
            >> oldLayout
            >> commentLines
            >> printDirection
            >> fullLayout
            >> codetagCount;

        if (height <= 0)
        {
            result.errorMessage = "Banner font header reported an invalid height.";
            return result;
        }

        font.setHeight(height);
        font.setBaseline(baseline);
        font.setMaxLength(maxLength);
        font.setOldLayout(oldLayout);
        font.setCommentLines(commentLines);
        font.setPrintDirection(printDirection);
        font.setFullLayout(fullLayout);
        font.setCodetagCount(codetagCount);

        for (int i = 0; i < commentLines; ++i)
        {
            std::string ignored;
            if (!readRawLine(file, ignored))
            {
                result.errorMessage = "Banner font ended while skipping header comments.";
                return result;
            }
        }

        for (int codePoint = 32; codePoint <= 126; ++codePoint)
        {
            AsciiBannerFont::GlyphRows glyphRows;
            glyphRows.reserve(static_cast<std::size_t>(height));

            char endMarker = '\0';

            for (int row = 0; row < height; ++row)
            {
                std::string rawLine;
                if (!readRawLine(file, rawLine))
                {
                    result.errorMessage = "Banner font ended while reading required ASCII glyph rows.";
                    return result;
                }

                if (row == 0)
                {
                    endMarker = rawLine.empty() ? '@' : rawLine.back();
                }

                bool usedFallback = false;
                const std::string decoded = decodeFontLineBytes(
                    rawLine,
                    options.decodeNonUtf8GlyphRowsAsCp437,
                    usedFallback);

                if (!options.decodeNonUtf8GlyphRowsAsCp437 && decoded.empty() && !rawLine.empty())
                {
                    result.errorMessage = "Banner glyph row was not valid UTF-8 and CP437 fallback was disabled.";
                    return result;
                }

                if (usedFallback)
                {
                    result.decodedNonUtf8GlyphRows = true;
                }

                glyphRows.push_back(trimEndMarker(decoded, endMarker));
            }

            font.setGlyph(codePoint, std::move(glyphRows));
        }

        if (options.loadCodetagGlyphs)
        {
            while (file)
            {
                const std::streampos blockStart = file.tellg();

                std::string codeTagLine;
                if (!readRawLine(file, codeTagLine))
                {
                    break;
                }

                codeTagLine = trimRight(codeTagLine);
                if (codeTagLine.empty())
                {
                    continue;
                }

                int codePoint = 0;
                if (!parseCodeTag(codeTagLine, codePoint))
                {
                    if (options.preserveUnknownCodetagBlocks)
                    {
                        addWarning(
                            result,
                            LoadWarningCode::InvalidCodetagBlockIgnored,
                            "Ignored a non-codetag extension block while loading banner font.");
                    }
                    continue;
                }

                AsciiBannerFont::GlyphRows glyphRows;
                glyphRows.reserve(static_cast<std::size_t>(height));

                char endMarker = '\0';

                for (int row = 0; row < height; ++row)
                {
                    std::string rawLine;
                    if (!readRawLine(file, rawLine))
                    {
                        file.clear();
                        file.seekg(blockStart);
                        break;
                    }

                    if (row == 0)
                    {
                        endMarker = rawLine.empty() ? '@' : rawLine.back();
                    }

                    bool usedFallback = false;
                    const std::string decoded = decodeFontLineBytes(
                        rawLine,
                        options.decodeNonUtf8GlyphRowsAsCp437,
                        usedFallback);

                    if (!options.decodeNonUtf8GlyphRowsAsCp437 && decoded.empty() && !rawLine.empty())
                    {
                        result.errorMessage =
                            "Banner codetag glyph row was not valid UTF-8 and CP437 fallback was disabled.";
                        return result;
                    }

                    if (usedFallback)
                    {
                        result.decodedNonUtf8GlyphRows = true;
                    }

                    glyphRows.push_back(trimEndMarker(decoded, endMarker));
                }

                if (static_cast<int>(glyphRows.size()) == height)
                {
                    font.setGlyph(codePoint, std::move(glyphRows));
                }
                else
                {
                    addWarning(
                        result,
                        LoadWarningCode::InvalidCodetagBlockIgnored,
                        "Ignored an incomplete codetag glyph block while loading banner font.");
                    break;
                }
            }
        }

        if (result.decodedNonUtf8GlyphRows)
        {
            addWarning(
                result,
                LoadWarningCode::InvalidUtf8DecodedAsCp437,
                "One or more banner glyph rows were decoded through CP437 fallback.");
        }

        result.font = std::move(font);
        result.success = result.font.isLoaded();
        return result;
    }

    bool tryLoadFromFile(const std::string& filePath, AsciiBannerFont& outFont)
    {
        return tryLoadFromFile(filePath, outFont, {});
    }

    bool tryLoadFromFile(const std::string& filePath, AsciiBannerFont& outFont, const LoadOptions& options)
    {
        const LoadResult result = loadFromFile(filePath, options);
        if (!result.success)
        {
            return false;
        }

        outFont = result.font;
        return true;
    }

    std::string formatLoadError(const LoadResult& result)
    {
        return result.errorMessage;
    }

    const char* toString(const LoadWarningCode warningCode)
    {
        switch (warningCode)
        {
        case LoadWarningCode::InvalidCodetagBlockIgnored:
            return "InvalidCodetagBlockIgnored";

        case LoadWarningCode::InvalidUtf8DecodedAsCp437:
            return "InvalidUtf8DecodedAsCp437";

        case LoadWarningCode::None:
        default:
            return "None";
        }
    }
}
