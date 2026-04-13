#include "Rendering/Objects/AsciiBanner.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>

#include "Rendering/Objects/TextObjectBuilder.h"
#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"

namespace
{
    std::u32string decodeUtf8(std::string_view text)
    {
        std::u32string out;
        out.reserve(text.size());

        std::size_t i = 0;
        while (i < text.size())
        {
            const unsigned char c0 = static_cast<unsigned char>(text[i]);

            if (c0 < 0x80)
            {
                out.push_back(static_cast<char32_t>(c0));
                ++i;
                continue;
            }

            if ((c0 & 0xE0) == 0xC0)
            {
                if (i + 1 >= text.size())
                {
                    out.push_back(U'?');
                    break;
                }

                const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                if ((c1 & 0xC0) != 0x80)
                {
                    out.push_back(U'?');
                    ++i;
                    continue;
                }

                const char32_t cp =
                    ((static_cast<char32_t>(c0 & 0x1F) << 6) |
                        (static_cast<char32_t>(c1 & 0x3F)));

                out.push_back(cp);
                i += 2;
                continue;
            }

            if ((c0 & 0xF0) == 0xE0)
            {
                if (i + 2 >= text.size())
                {
                    out.push_back(U'?');
                    break;
                }

                const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                const unsigned char c2 = static_cast<unsigned char>(text[i + 2]);

                if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
                {
                    out.push_back(U'?');
                    ++i;
                    continue;
                }

                const char32_t cp =
                    ((static_cast<char32_t>(c0 & 0x0F) << 12) |
                        (static_cast<char32_t>(c1 & 0x3F) << 6) |
                        (static_cast<char32_t>(c2 & 0x3F)));

                out.push_back(cp);
                i += 3;
                continue;
            }

            if ((c0 & 0xF8) == 0xF0)
            {
                if (i + 3 >= text.size())
                {
                    out.push_back(U'?');
                    break;
                }

                const unsigned char c1 = static_cast<unsigned char>(text[i + 1]);
                const unsigned char c2 = static_cast<unsigned char>(text[i + 2]);
                const unsigned char c3 = static_cast<unsigned char>(text[i + 3]);

                if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
                {
                    out.push_back(U'?');
                    ++i;
                    continue;
                }

                const char32_t cp =
                    ((static_cast<char32_t>(c0 & 0x07) << 18) |
                        (static_cast<char32_t>(c1 & 0x3F) << 12) |
                        (static_cast<char32_t>(c2 & 0x3F) << 6) |
                        (static_cast<char32_t>(c3 & 0x3F)));

                out.push_back(cp);
                i += 4;
                continue;
            }

            out.push_back(U'?');
            ++i;
        }

        return out;
    }

    int cellWidthToInt(const CellWidth width)
    {
        switch (width)
        {
        case CellWidth::Zero:
            return 0;

        case CellWidth::Two:
            return 2;

        case CellWidth::One:
        default:
            return 1;
        }
    }
}

std::string AsciiBanner::trimRight(const std::string& s)
{
    std::string result = s;

    while (!result.empty() &&
        (result.back() == ' ' || result.back() == '\t' || result.back() == '\r'))
    {
        result.pop_back();
    }

    return result;
}

std::vector<std::string> AsciiBanner::splitLines(std::string_view text)
{
    std::vector<std::string> lines;

    std::stringstream ss{ std::string(text) };
    std::string line;

    while (std::getline(ss, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(line);
    }

    if (lines.empty())
    {
        lines.push_back("");
    }

    return lines;
}

const AsciiBannerFont::GlyphRows* AsciiBanner::findGlyph(
    const AsciiBannerFont& font,
    const int codePoint)
{
    return font.findGlyph(codePoint);
}

const AsciiBannerFont::GlyphRows& AsciiBanner::getFallbackGlyph(const AsciiBannerFont& font)
{
    if (const auto* questionGlyph = findGlyph(font, '?'))
    {
        return *questionGlyph;
    }

    static const AsciiBannerFont::GlyphRows empty;
    return empty;
}

int AsciiBanner::getMinimumLeadingSpaces(const AsciiBannerFont::GlyphRows& glyph)
{
    int minSpaces = std::numeric_limits<int>::max();

    for (const auto& line : glyph)
    {
        const std::u32string decoded = decodeUtf8(line);

        int count = 0;
        while (count < static_cast<int>(decoded.size()) &&
            decoded[static_cast<std::size_t>(count)] == U' ')
        {
            ++count;
        }

        if (count < static_cast<int>(decoded.size()))
        {
            minSpaces = (std::min)(minSpaces, count);
        }
    }

    return (minSpaces == std::numeric_limits<int>::max()) ? 0 : minSpaces;
}

int AsciiBanner::getMinimumTrailingSpaces(const std::vector<std::string>& lines)
{
    int minSpaces = std::numeric_limits<int>::max();

    for (const auto& line : lines)
    {
        const std::u32string decoded = decodeUtf8(line);

        int count = 0;
        for (int i = static_cast<int>(decoded.size()) - 1;
            i >= 0 && decoded[static_cast<std::size_t>(i)] == U' ';
            --i)
        {
            ++count;
        }

        if (!decoded.empty())
        {
            minSpaces = (std::min)(minSpaces, count);
        }
    }

    return (minSpaces == std::numeric_limits<int>::max()) ? 0 : minSpaces;
}

void AsciiBanner::appendGlyphFullWidth(
    std::vector<std::string>& outputLines,
    const AsciiBannerFont::GlyphRows& glyph)
{
    for (std::size_t row = 0; row < outputLines.size(); ++row)
    {
        outputLines[row] += glyph[row];
    }
}

void AsciiBanner::appendGlyphKern(
    std::vector<std::string>& outputLines,
    const AsciiBannerFont::GlyphRows& glyph)
{
    bool allEmpty = true;

    for (const auto& line : outputLines)
    {
        if (!line.empty())
        {
            allEmpty = false;
            break;
        }
    }

    if (allEmpty)
    {
        appendGlyphFullWidth(outputLines, glyph);
        return;
    }

    const int trailing = getMinimumTrailingSpaces(outputLines);
    const int leading = getMinimumLeadingSpaces(glyph);
    const int overlap = (std::min)(trailing, leading);

    for (std::size_t row = 0; row < outputLines.size(); ++row)
    {
        int removed = 0;

        while (removed < overlap)
        {
            const std::u32string decoded = decodeUtf8(outputLines[row]);
            if (decoded.empty() || decoded.back() != U' ')
            {
                break;
            }

            std::string trimmed = outputLines[row];
            while (!trimmed.empty())
            {
                const unsigned char c = static_cast<unsigned char>(trimmed.back());
                trimmed.pop_back();

                if ((c & 0xC0) != 0x80)
                {
                    break;
                }
            }

            outputLines[row] = trimmed;
            ++removed;
        }

        outputLines[row] += glyph[row];
    }
}

std::vector<std::string> AsciiBanner::renderSingleLine(
    const AsciiBannerFont& font,
    std::string_view text,
    const RenderOptions& options)
{
    if (!font.isLoaded())
    {
        throw std::runtime_error("AsciiBanner::renderSingleLine(): font asset is not loaded.");
    }

    std::vector<std::string> outputLines(static_cast<std::size_t>(font.getHeight()), "");

    const std::u32string codePoints = decodeUtf8(text);

    for (const char32_t cp : codePoints)
    {
        const AsciiBannerFont::GlyphRows* glyph = findGlyph(font, static_cast<int>(cp));

        if (!glyph)
        {
            glyph = options.useFallbackGlyphForUnknownChars ? &getFallbackGlyph(font) : nullptr;
        }

        if (!glyph || glyph->empty())
        {
            continue;
        }

        AsciiBannerFont::GlyphRows processedGlyph = *glyph;

        if (options.replaceHardBlankWithSpace)
        {
            for (auto& row : processedGlyph)
            {
                std::replace(row.begin(), row.end(), font.getHardBlank(), ' ');
            }
        }

        if (options.composeMode == ComposeMode::FullWidth)
        {
            appendGlyphFullWidth(outputLines, processedGlyph);
        }
        else
        {
            appendGlyphKern(outputLines, processedGlyph);
        }
    }

    return outputLines;
}

std::vector<std::string> AsciiBanner::alignLines(
    const std::vector<std::string>& lines,
    const Alignment alignment,
    const std::size_t targetWidth)
{
    if (alignment == Alignment::Left || targetWidth == 0)
    {
        return lines;
    }

    std::vector<std::string> out = lines;

    for (auto& line : out)
    {
        const std::u32string decoded = UnicodeConversion::utf8ToU32(line);

        std::size_t width = 0;
        for (const char32_t cp : decoded)
        {
            width += static_cast<std::size_t>(
                cellWidthToInt(UnicodeWidth::measureCodePointWidth(
                    UnicodeConversion::sanitizeCodePoint(cp))));
        }

        if (width >= targetWidth)
        {
            continue;
        }

        const std::size_t pad = targetWidth - width;
        std::size_t leftPad = 0;

        if (alignment == Alignment::Center)
        {
            leftPad = pad / 2;
        }
        else if (alignment == Alignment::Right)
        {
            leftPad = pad;
        }

        line = std::string(leftPad, ' ') + line;
    }

    return out;
}

std::string AsciiBanner::joinLines(
    const std::vector<std::string>& lines,
    const bool trimTrailingSpaces)
{
    std::ostringstream out;

    for (std::size_t i = 0; i < lines.size(); ++i)
    {
        out << (trimTrailingSpaces ? trimRight(lines[i]) : lines[i]);

        if (i + 1 < lines.size())
        {
            out << '\n';
        }
    }

    return out.str();
}

std::string AsciiBanner::render(
    const AsciiBannerFont& font,
    std::string_view text)
{
    RenderOptions options;
    return render(font, text, options);
}

std::string AsciiBanner::render(
    const AsciiBannerFont& font,
    std::string_view text,
    const RenderOptions& options)
{
    const std::vector<std::string> lines = renderLines(font, text, options);
    return joinLines(lines, false);
}

std::vector<std::string> AsciiBanner::renderLines(
    const AsciiBannerFont& font,
    std::string_view text)
{
    RenderOptions options;
    return renderLines(font, text, options);
}

std::vector<std::string> AsciiBanner::renderLines(
    const AsciiBannerFont& font,
    std::string_view text,
    const RenderOptions& options)
{
    if (!font.isLoaded())
    {
        throw std::runtime_error("AsciiBanner::renderLines(): font asset is not loaded.");
    }

    std::vector<std::string> result;
    const std::vector<std::string> inputLines = splitLines(text);

    for (std::size_t i = 0; i < inputLines.size(); ++i)
    {
        std::vector<std::string> rendered = renderSingleLine(font, inputLines[i], options);
        rendered = alignLines(rendered, options.alignment, options.targetWidth);

        if (options.trimTrailingSpaces)
        {
            for (auto& line : rendered)
            {
                line = trimRight(line);
            }
        }

        result.insert(result.end(), rendered.begin(), rendered.end());

        if (i + 1 < inputLines.size())
        {
            result.emplace_back("");
        }
    }

    return result;
}

TextObject AsciiBanner::generateTextObject(
    const AsciiBannerFont& font,
    std::string_view text)
{
    RenderOptions options;
    return generateTextObject(font, text, options);
}

TextObject AsciiBanner::generateTextObject(
    const AsciiBannerFont& font,
    std::string_view text,
    const RenderOptions& options)
{
    return generateTextObject(font, text, Style{}, options);
}

TextObject AsciiBanner::generateTextObject(
    const AsciiBannerFont& font,
    std::string_view text,
    const Style& style)
{
    RenderOptions options;
    return generateTextObject(font, text, style, options);
}

TextObject AsciiBanner::generateTextObject(
    const AsciiBannerFont& font,
    std::string_view text,
    const Style& style,
    const RenderOptions& options)
{
    const std::vector<std::string> lines = renderLines(font, text, options);

    int width = 0;
    for (const std::string& line : lines)
    {
        width = (std::max)(width, measureRenderedLineWidth(line));
    }

    const int height = static_cast<int>(lines.size());

    if (width <= 0 || height <= 0)
    {
        return TextObject{};
    }

    TextObjectBuilder builder(width, height);

    for (int y = 0; y < height; ++y)
    {
        appendRenderedLineToBuilder(
            builder,
            y,
            lines[static_cast<std::size_t>(y)],
            options.transparentSpaces,
            std::optional<Style>(style));
    }

    return builder.build();
}

int AsciiBanner::measureRenderedLineWidth(std::string_view line)
{
    const std::u32string decoded = UnicodeConversion::utf8ToU32(line);

    int width = 0;
    for (char32_t cp : decoded)
    {
        cp = UnicodeConversion::sanitizeCodePoint(cp);
        width += cellWidthToInt(UnicodeWidth::measureCodePointWidth(cp));
    }

    return width;
}

void AsciiBanner::appendRenderedLineToBuilder(
    TextObjectBuilder& builder,
    const int y,
    std::string_view line,
    const bool transparentSpaces,
    const std::optional<Style>& style)
{
    const std::u32string decoded = UnicodeConversion::utf8ToU32(line);

    int x = 0;
    for (char32_t cp : decoded)
    {
        cp = UnicodeConversion::sanitizeCodePoint(cp);

        if (cp == U'\r' || cp == U'\n')
        {
            continue;
        }

        const CellWidth measuredWidth = UnicodeWidth::measureCodePointWidth(cp);

        if (measuredWidth == CellWidth::Zero)
        {
            continue;
        }

        if (cp == U' ' && transparentSpaces)
        {
            if (measuredWidth == CellWidth::Two)
            {
                builder.setEmpty(x, y);
                builder.setEmpty(x + 1, y);
                x += 2;
            }
            else
            {
                builder.setEmpty(x, y);
                x += 1;
            }

            continue;
        }

        if (measuredWidth == CellWidth::Two)
        {
            builder.setWideGlyph(x, y, cp, style);
            x += 2;
            continue;
        }

        builder.setGlyph(x, y, cp, style);
        x += 1;
    }
}
