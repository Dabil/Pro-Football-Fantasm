#include "Rendering/Objects/pFontLoader.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/StyleMerge.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "Rendering/Objects/TextObjectBuilder.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    using namespace PseudoFont;

    std::string trim(const std::string& value)
    {
        std::size_t begin = 0;
        while (begin < value.size() &&
            std::isspace(static_cast<unsigned char>(value[begin])))
        {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin &&
            std::isspace(static_cast<unsigned char>(value[end - 1])))
        {
            --end;
        }

        return value.substr(begin, end - begin);
    }

    bool startsWith(const std::string& value, const std::string& prefix)
    {
        return value.size() >= prefix.size() &&
            value.compare(0, prefix.size(), prefix) == 0;
    }

    bool equalsIgnoreCase(const std::string& a, const std::string& b)
    {
        if (a.size() != b.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
            {
                return false;
            }
        }

        return true;
    }

    bool parseInt(const std::string& value, int& outValue)
    {
        try
        {
            std::size_t consumed = 0;
            const int parsed = std::stoi(value, &consumed, 10);
            if (consumed != value.size())
            {
                return false;
            }

            outValue = parsed;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool parseBool(const std::string& value, bool& outValue)
    {
        if (equalsIgnoreCase(value, "true") ||
            equalsIgnoreCase(value, "yes") ||
            value == "1")
        {
            outValue = true;
            return true;
        }

        if (equalsIgnoreCase(value, "false") ||
            equalsIgnoreCase(value, "no") ||
            value == "0")
        {
            outValue = false;
            return true;
        }

        return false;
    }

    std::optional<Color> parseBasicColor(const std::string& value)
    {
        const struct Entry
        {
            const char* name;
            Color::Basic basic;
        } table[] =
        {
            { "black", Color::Basic::Black },
            { "red", Color::Basic::Red },
            { "green", Color::Basic::Green },
            { "yellow", Color::Basic::Yellow },
            { "blue", Color::Basic::Blue },
            { "magenta", Color::Basic::Magenta },
            { "cyan", Color::Basic::Cyan },
            { "white", Color::Basic::White },
            { "bright_black", Color::Basic::BrightBlack },
            { "bright_red", Color::Basic::BrightRed },
            { "bright_green", Color::Basic::BrightGreen },
            { "bright_yellow", Color::Basic::BrightYellow },
            { "bright_blue", Color::Basic::BrightBlue },
            { "bright_magenta", Color::Basic::BrightMagenta },
            { "bright_cyan", Color::Basic::BrightCyan },
            { "bright_white", Color::Basic::BrightWhite }
        };

        for (const Entry& entry : table)
        {
            if (equalsIgnoreCase(value, entry.name))
            {
                return Color::FromBasic(entry.basic);
            }
        }

        return std::nullopt;
    }

    bool parseHexNibble(char ch, std::uint8_t& outValue)
    {
        if (ch >= '0' && ch <= '9')
        {
            outValue = static_cast<std::uint8_t>(ch - '0');
            return true;
        }

        if (ch >= 'a' && ch <= 'f')
        {
            outValue = static_cast<std::uint8_t>(10 + (ch - 'a'));
            return true;
        }

        if (ch >= 'A' && ch <= 'F')
        {
            outValue = static_cast<std::uint8_t>(10 + (ch - 'A'));
            return true;
        }

        return false;
    }

    std::optional<Color> parseRgbColor(const std::string& value)
    {
        if (value.size() != 7 || value[0] != '#')
        {
            return std::nullopt;
        }

        std::uint8_t n0 = 0;
        std::uint8_t n1 = 0;
        std::uint8_t n2 = 0;
        std::uint8_t n3 = 0;
        std::uint8_t n4 = 0;
        std::uint8_t n5 = 0;

        if (!parseHexNibble(value[1], n0) ||
            !parseHexNibble(value[2], n1) ||
            !parseHexNibble(value[3], n2) ||
            !parseHexNibble(value[4], n3) ||
            !parseHexNibble(value[5], n4) ||
            !parseHexNibble(value[6], n5))
        {
            return std::nullopt;
        }

        return Color::FromRgb(
            static_cast<std::uint8_t>((n0 << 4) | n1),
            static_cast<std::uint8_t>((n2 << 4) | n3),
            static_cast<std::uint8_t>((n4 << 4) | n5));
    }

    bool applyStyleProperty(Style& style, const std::string& key, const std::string& value)
    {
        if (equalsIgnoreCase(key, "foreground"))
        {
            if (equalsIgnoreCase(value, "none") || equalsIgnoreCase(value, "default"))
            {
                style = style.withoutForeground();
                return true;
            }

            if (const std::optional<Color> basic = parseBasicColor(value))
            {
                style = style.withForeground(*basic);
                return true;
            }

            if (const std::optional<Color> rgb = parseRgbColor(value))
            {
                style = style.withForeground(*rgb);
                return true;
            }

            return false;
        }

        if (equalsIgnoreCase(key, "background"))
        {
            if (equalsIgnoreCase(value, "none") || equalsIgnoreCase(value, "default"))
            {
                style = style.withoutBackground();
                return true;
            }

            if (const std::optional<Color> basic = parseBasicColor(value))
            {
                style = style.withBackground(*basic);
                return true;
            }

            if (const std::optional<Color> rgb = parseRgbColor(value))
            {
                style = style.withBackground(*rgb);
                return true;
            }

            return false;
        }

        bool boolValue = false;
        if (!parseBool(value, boolValue))
        {
            return false;
        }

        if (equalsIgnoreCase(key, "bold"))
        {
            style = style.withBold(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "dim"))
        {
            style = style.withDim(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "underline"))
        {
            style = style.withUnderline(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "slowblink"))
        {
            style = style.withSlowBlink(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "fastblink"))
        {
            style = style.withFastBlink(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "reverse"))
        {
            style = style.withReverse(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "invisible"))
        {
            style = style.withInvisible(boolValue);
            return true;
        }

        if (equalsIgnoreCase(key, "strike"))
        {
            style = style.withStrike(boolValue);
            return true;
        }

        return false;
    }

    bool parseCodePointToken(const std::string& token, char32_t& outCodePoint)
    {
        if (equalsIgnoreCase(token, "space"))
        {
            outCodePoint = U' ';
            return true;
        }

        if (equalsIgnoreCase(token, "tab"))
        {
            outCodePoint = U'\t';
            return true;
        }

        if (equalsIgnoreCase(token, "question"))
        {
            outCodePoint = U'?';
            return true;
        }

        if (startsWith(token, "U+") || startsWith(token, "u+"))
        {
            try
            {
                const unsigned long value = std::stoul(token.substr(2), nullptr, 16);
                outCodePoint = static_cast<char32_t>(value);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        const std::u32string decoded = UnicodeConversion::utf8ToU32(token);
        if (decoded.size() != 1)
        {
            return false;
        }

        outCodePoint = decoded.front();
        return true;
    }

    void addWarning(
        LoadResult& result,
        const LoadWarningCode code,
        const std::string& message)
    {
        LoadWarning warning;
        warning.code = code;
        warning.message = message;
        result.warnings.push_back(std::move(warning));
    }

    std::vector<std::string> splitLogicalLines(std::string_view text)
    {
        std::vector<std::string> lines;
        std::string current;

        for (char ch : text)
        {
            if (ch == '\r')
            {
                continue;
            }

            if (ch == '\n')
            {
                lines.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(ch);
        }

        lines.push_back(current);
        return lines;
    }

    std::vector<std::u32string> splitTextLinesUtf32(std::string_view utf8Text)
    {
        const std::u32string decoded = UnicodeConversion::utf8ToU32(utf8Text);

        std::vector<std::u32string> lines;
        std::u32string current;

        for (char32_t cp : decoded)
        {
            if (cp == U'\r')
            {
                continue;
            }

            if (cp == U'\n')
            {
                lines.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(cp);
        }

        lines.push_back(current);
        return lines;
    }

    bool layerNameEnabled(
        const std::string& layerName,
        const RenderOptions& options)
    {
        switch (options.layerSelectionMode)
        {
        case LayerSelectionMode::VisibleByDefaultOnly:
            return true;

        case LayerSelectionMode::AllLayers:
            return true;

        case LayerSelectionMode::NamedLayerSet:
            return std::find(
                options.enabledLayerNames.begin(),
                options.enabledLayerNames.end(),
                layerName) != options.enabledLayerNames.end();
        }

        return false;
    }

    bool includeLayer(
        const GlyphLayer& layer,
        const RenderOptions& options)
    {
        switch (options.layerSelectionMode)
        {
        case LayerSelectionMode::VisibleByDefaultOnly:
            return layer.visibleByDefault;

        case LayerSelectionMode::AllLayers:
            return true;

        case LayerSelectionMode::NamedLayerSet:
            return layerNameEnabled(layer.name, options);
        }

        return false;
    }

    std::optional<Style> resolveCellStyle(
        const FontDefinition& font,
        const LayeredGlyph& glyph,
        const GlyphLayer& layer,
        const GlyphCell& cell,
        const StyleMode styleMode,
        const std::optional<Style>& overrideStyle)
    {
        auto overlayStyle = [](Style base, const Style& overlay) -> Style
            {
                if (overlay.hasForeground())
                {
                    base = base.withForeground(*overlay.foreground());
                }

                if (overlay.hasBackground())
                {
                    base = base.withBackground(*overlay.background());
                }

                if (overlay.hasBold())
                {
                    base = base.withBold(overlay.bold());
                }

                if (overlay.hasDim())
                {
                    base = base.withDim(overlay.dim());
                }

                if (overlay.hasUnderline())
                {
                    base = base.withUnderline(overlay.underline());
                }

                if (overlay.hasSlowBlink())
                {
                    base = base.withSlowBlink(overlay.slowBlink());
                }

                if (overlay.hasFastBlink())
                {
                    base = base.withFastBlink(overlay.fastBlink());
                }

                if (overlay.hasReverse())
                {
                    base = base.withReverse(overlay.reverse());
                }

                if (overlay.hasInvisible())
                {
                    base = base.withInvisible(overlay.invisible());
                }

                if (overlay.hasStrike())
                {
                    base = base.withStrike(overlay.strike());
                }

                return base;
            };

        auto composeAuthoredStyle = [&]() -> std::optional<Style>
            {
                Style composed;
                bool hasAny = false;

                auto applyIfPresent = [&](const std::optional<Style>& candidate)
                    {
                        if (!candidate.has_value())
                        {
                            return;
                        }

                        if (!hasAny)
                        {
                            composed = *candidate;
                            hasAny = true;
                            return;
                        }

                        composed = overlayStyle(composed, *candidate);
                    };

                applyIfPresent(font.defaultStyle);
                applyIfPresent(glyph.defaultStyle);
                applyIfPresent(layer.defaultStyle);
                applyIfPresent(cell.style);

                if (!hasAny)
                {
                    return std::nullopt;
                }

                return composed;
            };

        switch (styleMode)
        {
        case StyleMode::ForceOverride:
            return overrideStyle;

        case StyleMode::PreserveAuthored:
            return composeAuthoredStyle();

        case StyleMode::ApplyOverrideWhereMissing:
        {
            const std::optional<Style> authored = composeAuthoredStyle();

            if (!authored.has_value())
            {
                return overrideStyle;
            }

            if (!overrideStyle.has_value())
            {
                return authored;
            }

            return overlayStyle(*authored, *overrideStyle);
        }
        }

        return std::nullopt;
    }

    int spacingForCodePoint(const FontDefinition& font, char32_t codePoint)
    {
        return (codePoint == U' ') ? font.spacing.wordSpacing : font.spacing.letterSpacing;
    }

    int measureLineWidth(
        const FontDefinition& font,
        const std::u32string& line,
        const bool useFallbackGlyphForUnknownChars)
    {
        int width = 0;
        bool first = true;

        for (char32_t cp : line)
        {
            if (cp == U'\t')
            {
                cp = U' ';
            }

            const LayeredGlyph* glyph = font.findGlyph(cp);
            if (glyph == nullptr && useFallbackGlyphForUnknownChars)
            {
                glyph = font.findFallbackGlyph();
            }

            if (glyph == nullptr)
            {
                continue;
            }

            if (!first)
            {
                width += spacingForCodePoint(font, cp);
            }

            width += glyph->width;
            first = false;
        }

        return width;
    }

    int computeOutputWidth(
        const FontDefinition& font,
        const std::vector<std::u32string>& lines,
        const Alignment alignment,
        const std::size_t targetWidth,
        const bool useFallbackGlyphForUnknownChars)
    {
        (void)alignment;

        int measuredWidth = 0;
        for (const std::u32string& line : lines)
        {
            measuredWidth = std::max(
                measuredWidth,
                measureLineWidth(font, line, useFallbackGlyphForUnknownChars));
        }

        if (targetWidth > 0)
        {
            measuredWidth = std::max(measuredWidth, static_cast<int>(targetWidth));
        }

        return measuredWidth;
    }

    int computeOutputHeight(
        const FontDefinition& font,
        const std::size_t lineCount)
    {
        if (lineCount == 0)
        {
            return 0;
        }

        return static_cast<int>(lineCount) * font.glyphHeight +
            static_cast<int>(lineCount > 0 ? lineCount - 1 : 0) * font.spacing.lineSpacing;
    }

    int computeAlignedLineStartX(
        const FontDefinition& font,
        const std::u32string& line,
        const int outputWidth,
        const Alignment alignment,
        const bool useFallbackGlyphForUnknownChars)
    {
        const int lineWidth = measureLineWidth(font, line, useFallbackGlyphForUnknownChars);

        switch (alignment)
        {
        case Alignment::Center:
            return std::max(0, (outputWidth - lineWidth) / 2);

        case Alignment::Right:
            return std::max(0, outputWidth - lineWidth);

        case Alignment::Left:
        default:
            return 0;
        }
    }

    void stampGlyphLayerToBuilder(
        TextObjectBuilder& builder,
        const FontDefinition& font,
        const LayeredGlyph& glyph,
        const GlyphLayer& layer,
        const int destX,
        const int destY,
        const StyleMode styleMode,
        const std::optional<Style>& overrideStyle,
        const bool transparentSpaces,
        const bool applyLayerOffset)
    {
        for (int y = 0; y < layer.height; ++y)
        {
            for (int x = 0; x < layer.width; ++x)
            {
                const GlyphCell* cell = layer.tryGetCell(x, y);
                if (cell == nullptr)
                {
                    continue;
                }

                const int outX = destX + (applyLayerOffset ? layer.offsetX : 0) + x;
                const int outY = destY + (applyLayerOffset ? layer.offsetY : 0) + y;

                if (!builder.inBounds(outX, outY))
                {
                    continue;
                }

                const std::optional<Style> styleToApply = resolveCellStyle(
                    font,
                    glyph,
                    layer,
                    *cell,
                    styleMode,
                    overrideStyle);

                if (cell->transparent)
                {
                    if (!transparentSpaces)
                    {
                        builder.setTransparent(outX, outY, styleToApply);
                    }
                    continue;
                }

                builder.setGlyph(outX, outY, cell->glyph, styleToApply);
            }
        }
    }

    Rendering::TextObjectLayer* findOutputLayer(
        Rendering::LayeredTextObject& layered,
        std::string_view layerName)
    {
        return layered.findLayer(layerName);
    }

    bool ensureOutputLayer(
        Rendering::LayeredTextObject& layered,
        const GlyphLayer& sourceLayer,
        const LayeredRenderOptions& options)
    {
        if (findOutputLayer(layered, sourceLayer.name) != nullptr)
        {
            return true;
        }

        Rendering::TextObjectLayer outputLayer;
        outputLayer.name = sourceLayer.name;
        outputLayer.zIndex = sourceLayer.zIndex;
        outputLayer.offsetX = sourceLayer.offsetX;
        outputLayer.offsetY = sourceLayer.offsetY;

        switch (options.initialVisibilityMode)
        {
        case LayeredRenderOptions::InitialVisibilityMode::UseFontDefaults:
            outputLayer.visible = sourceLayer.visibleByDefault;
            break;

        case LayeredRenderOptions::InitialVisibilityMode::AllVisible:
            outputLayer.visible = true;
            break;

        case LayeredRenderOptions::InitialVisibilityMode::AllHidden:
            outputLayer.visible = false;
            break;
        }

        outputLayer.object = TextObjectBuilder(
            layered.getWidth(),
            layered.getHeight()).build();

        return layered.addLayer(std::move(outputLayer));
    }

    void stampGlyphLayerToOutputLayer(
        TextObjectBuilder& builder,
        const FontDefinition& font,
        const LayeredGlyph& glyph,
        const GlyphLayer& layer,
        const int glyphOriginX,
        const int glyphOriginY,
        const StyleMode styleMode,
        const std::optional<Style>& overrideStyle,
        const bool transparentSpaces)
    {
        stampGlyphLayerToBuilder(
            builder,
            font,
            glyph,
            layer,
            glyphOriginX,
            glyphOriginY,
            styleMode,
            overrideStyle,
            transparentSpaces,
            false);
    }
}

namespace PseudoFont
{
    bool GlyphLayer::isValid() const
    {
        return width > 0 &&
            height > 0 &&
            static_cast<int>(cells.size()) == width * height;
    }

    const GlyphCell* GlyphLayer::tryGetCell(int x, int y) const
    {
        if (x < 0 || x >= width || y < 0 || y >= height)
        {
            return nullptr;
        }

        return &cells[static_cast<std::size_t>(y * width + x)];
    }

    bool LayeredGlyph::isValid() const
    {
        return codePoint != U'\0' &&
            width > 0 &&
            height > 0 &&
            !layers.empty();
    }

    const GlyphLayer* LayeredGlyph::findLayer(std::string_view name) const
    {
        const auto it = std::find_if(
            layers.begin(),
            layers.end(),
            [&](const GlyphLayer& layer)
            {
                return layer.name == name;
            });

        return (it != layers.end()) ? &(*it) : nullptr;
    }

    bool FontDefinition::isLoaded() const
    {
        return !glyphs.empty() && glyphWidth > 0 && glyphHeight > 0;
    }

    const LayeredGlyph* FontDefinition::findGlyph(char32_t codePoint) const
    {
        const auto it = glyphs.find(codePoint);
        return (it != glyphs.end()) ? &it->second : nullptr;
    }

    const LayeredGlyph* FontDefinition::findFallbackGlyph() const
    {
        return findGlyph(fallbackCodePoint);
    }

    const char* toString(const LoadWarningCode code)
    {
        switch (code)
        {
        case LoadWarningCode::None:
            return "None";
        case LoadWarningCode::DuplicateGlyphReplaced:
            return "DuplicateGlyphReplaced";
        case LoadWarningCode::DuplicateLayerReplaced:
            return "DuplicateLayerReplaced";
        case LoadWarningCode::MissingFallbackGlyph:
            return "MissingFallbackGlyph";
        case LoadWarningCode::MissingBaseLayer:
            return "MissingBaseLayer";
        case LoadWarningCode::NonUniformGlyphSize:
            return "NonUniformGlyphSize";
        case LoadWarningCode::UnknownPropertyIgnored:
            return "UnknownPropertyIgnored";
        case LoadWarningCode::ConflictingLayerMetadata:
            return "ConflictingLayerMetadata";
        default:
            return "Unknown";
        }
    }

    LoadResult loadFromFile(const std::string& filePath)
    {
        return loadFromFile(filePath, LoadOptions{});
    }

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options)
    {
        LoadResult result;

        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            result.errorMessage = "Could not open pseudo font file: " + filePath;
            return result;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::vector<std::string> lines = splitLogicalLines(buffer.str());

        if (lines.empty())
        {
            result.errorMessage = "Pseudo font file is empty: " + filePath;
            return result;
        }

        std::size_t lineIndex = 0;
        while (lineIndex < lines.size() && trim(lines[lineIndex]).empty())
        {
            ++lineIndex;
        }

        if (lineIndex >= lines.size() || !equalsIgnoreCase(trim(lines[lineIndex]), "pfont2"))
        {
            result.errorMessage = "Pseudo font file must begin with header line: pfont2";
            return result;
        }

        ++lineIndex;

        FontDefinition font;
        font.sourcePath = filePath;
        font.requiresUniformGlyphSize = options.requireUniformGlyphSize;

        enum class ParseState
        {
            TopLevel,
            InGlyph,
            InLayer
        };

        ParseState state = ParseState::TopLevel;

        LayeredGlyph currentGlyph;
        GlyphLayer currentLayer;
        bool glyphHasLayer = false;

        std::unordered_map<std::string, std::pair<int, bool>> globalLayerMetadata;

        auto finalizeLayer = [&]() -> bool
            {
                if (state != ParseState::InLayer)
                {
                    return true;
                }

                if (currentLayer.name.empty())
                {
                    result.errorMessage = "Layer name may not be empty.";
                    return false;
                }

                if (currentLayer.height <= 0 || currentLayer.width <= 0)
                {
                    result.errorMessage = "Layer '" + currentLayer.name + "' has invalid dimensions.";
                    return false;
                }

                if (!currentLayer.isValid())
                {
                    result.errorMessage = "Layer '" + currentLayer.name + "' cell data is invalid.";
                    return false;
                }

                const auto existingLayerIt = std::find_if(
                    currentGlyph.layers.begin(),
                    currentGlyph.layers.end(),
                    [&](const GlyphLayer& layer)
                    {
                        return layer.name == currentLayer.name;
                    });

                if (existingLayerIt != currentGlyph.layers.end())
                {
                    *existingLayerIt = currentLayer;
                    addWarning(
                        result,
                        LoadWarningCode::DuplicateLayerReplaced,
                        "Duplicate layer replaced in glyph.");
                }
                else
                {
                    currentGlyph.layers.push_back(currentLayer);
                }

                const auto globalMetaIt = globalLayerMetadata.find(currentLayer.name);
                if (globalMetaIt == globalLayerMetadata.end())
                {
                    globalLayerMetadata[currentLayer.name] =
                        std::make_pair(currentLayer.zIndex, currentLayer.visibleByDefault);
                }
                else
                {
                    if (globalMetaIt->second.first != currentLayer.zIndex ||
                        globalMetaIt->second.second != currentLayer.visibleByDefault)
                    {
                        addWarning(
                            result,
                            LoadWarningCode::ConflictingLayerMetadata,
                            "Layer '" + currentLayer.name + "' uses conflicting z/visibility metadata across glyphs.");
                    }
                }

                glyphHasLayer = true;
                currentLayer = GlyphLayer{};
                state = ParseState::InGlyph;
                return true;
            };

        auto finalizeGlyph = [&]() -> bool
            {
                if (state == ParseState::InLayer)
                {
                    if (!finalizeLayer())
                    {
                        return false;
                    }
                }

                if (state != ParseState::InGlyph)
                {
                    return true;
                }

                if (options.requireAtLeastOneLayerPerGlyph && !glyphHasLayer)
                {
                    result.errorMessage = "Glyph has no layers.";
                    return false;
                }

                if (font.glyphWidth <= 0 || font.glyphHeight <= 0)
                {
                    result.errorMessage = "glyph_width and glyph_height must be defined before glyph data.";
                    return false;
                }

                currentGlyph.width = font.glyphWidth;
                currentGlyph.height = font.glyphHeight;

                if (!currentGlyph.isValid())
                {
                    result.errorMessage = "Glyph definition is invalid.";
                    return false;
                }

                if (currentGlyph.findLayer("base") == nullptr)
                {
                    addWarning(
                        result,
                        LoadWarningCode::MissingBaseLayer,
                        "Glyph is missing a 'base' layer.");
                }

                const auto existingGlyphIt = font.glyphs.find(currentGlyph.codePoint);
                if (existingGlyphIt != font.glyphs.end())
                {
                    addWarning(
                        result,
                        LoadWarningCode::DuplicateGlyphReplaced,
                        "Duplicate glyph definition replaced.");
                }

                font.glyphs[currentGlyph.codePoint] = currentGlyph;

                currentGlyph = LayeredGlyph{};
                glyphHasLayer = false;
                state = ParseState::TopLevel;
                return true;
            };

        for (; lineIndex < lines.size(); ++lineIndex)
        {
            std::string rawLine = lines[lineIndex];
            if (options.trimTrailingCarriageReturn &&
                !rawLine.empty() &&
                rawLine.back() == '\r')
            {
                rawLine.pop_back();
            }

            const std::string stripped = trim(rawLine);
            if (stripped.empty() || startsWith(stripped, "#"))
            {
                continue;
            }

            if (startsWith(stripped, "[glyph ") && stripped.back() == ']')
            {
                if (!finalizeGlyph())
                {
                    return result;
                }

                const std::string token = trim(stripped.substr(7, stripped.size() - 8));

                char32_t codePoint = U'\0';
                if (!parseCodePointToken(token, codePoint))
                {
                    result.errorMessage = "Invalid glyph identifier: " + token;
                    return result;
                }

                currentGlyph = LayeredGlyph{};
                currentGlyph.codePoint = codePoint;
                glyphHasLayer = false;
                state = ParseState::InGlyph;
                continue;
            }

            if (equalsIgnoreCase(stripped, "[endglyph]"))
            {
                if (!finalizeGlyph())
                {
                    return result;
                }
                continue;
            }

            if (startsWith(stripped, "[layer ") && stripped.back() == ']')
            {
                if (state != ParseState::InGlyph)
                {
                    result.errorMessage = "Layer block encountered outside glyph block.";
                    return result;
                }

                const std::string layerName = trim(stripped.substr(7, stripped.size() - 8));
                if (layerName.empty())
                {
                    result.errorMessage = "Layer name may not be empty.";
                    return result;
                }

                currentLayer = GlyphLayer{};
                currentLayer.name = layerName;
                currentLayer.width = font.glyphWidth;
                currentLayer.height = font.glyphHeight;
                currentLayer.cells.resize(
                    static_cast<std::size_t>(currentLayer.width * currentLayer.height));

                for (GlyphCell& cell : currentLayer.cells)
                {
                    cell.glyph = U' ';
                    cell.transparent = true;
                }

                state = ParseState::InLayer;
                continue;
            }

            if (equalsIgnoreCase(stripped, "[endlayer]"))
            {
                if (!finalizeLayer())
                {
                    return result;
                }
                continue;
            }

            const std::size_t equalsPos = stripped.find('=');
            if (equalsPos == std::string::npos)
            {
                result.errorMessage = "Expected key=value line: " + stripped;
                return result;
            }

            const std::string key = trim(stripped.substr(0, equalsPos));
            const std::string value = stripped.substr(equalsPos + 1);

            if (state == ParseState::TopLevel)
            {
                if (equalsIgnoreCase(key, "name"))
                {
                    font.name = trim(value);
                    continue;
                }

                if (equalsIgnoreCase(key, "glyph_width"))
                {
                    if (!parseInt(trim(value), font.glyphWidth) || font.glyphWidth <= 0)
                    {
                        result.errorMessage = "Invalid glyph_width value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "glyph_height"))
                {
                    if (!parseInt(trim(value), font.glyphHeight) || font.glyphHeight <= 0)
                    {
                        result.errorMessage = "Invalid glyph_height value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "letter_spacing"))
                {
                    if (!parseInt(trim(value), font.spacing.letterSpacing))
                    {
                        result.errorMessage = "Invalid letter_spacing value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "word_spacing"))
                {
                    if (!parseInt(trim(value), font.spacing.wordSpacing))
                    {
                        result.errorMessage = "Invalid word_spacing value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "line_spacing"))
                {
                    if (!parseInt(trim(value), font.spacing.lineSpacing))
                    {
                        result.errorMessage = "Invalid line_spacing value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "fallback"))
                {
                    if (!parseCodePointToken(trim(value), font.fallbackCodePoint))
                    {
                        result.errorMessage = "Invalid fallback value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "transparent"))
                {
                    if (!parseCodePointToken(trim(value), font.transparentMarker))
                    {
                        result.errorMessage = "Invalid transparent marker: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (startsWith(key, "default_style."))
                {
                    Style style = font.defaultStyle.value_or(Style{});
                    if (!applyStyleProperty(style, key.substr(14), trim(value)))
                    {
                        result.errorMessage = "Invalid default_style property: " + key;
                        return result;
                    }
                    font.defaultStyle = style;
                    continue;
                }

                if (options.treatUnknownPropertiesAsWarnings)
                {
                    addWarning(
                        result,
                        LoadWarningCode::UnknownPropertyIgnored,
                        "Unknown top-level property ignored: " + key);
                    continue;
                }

                result.errorMessage = "Unknown top-level property: " + key;
                return result;
            }

            if (state == ParseState::InGlyph)
            {
                if (startsWith(key, "glyph_style."))
                {
                    Style style = currentGlyph.defaultStyle.value_or(Style{});
                    if (!applyStyleProperty(style, key.substr(12), trim(value)))
                    {
                        result.errorMessage = "Invalid glyph_style property: " + key;
                        return result;
                    }
                    currentGlyph.defaultStyle = style;
                    continue;
                }

                result.errorMessage = "Unsupported glyph-level property: " + key;
                return result;
            }

            if (state == ParseState::InLayer)
            {
                if (equalsIgnoreCase(key, "z"))
                {
                    if (!parseInt(trim(value), currentLayer.zIndex))
                    {
                        result.errorMessage = "Invalid z value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "visible"))
                {
                    if (!parseBool(trim(value), currentLayer.visibleByDefault))
                    {
                        result.errorMessage = "Invalid visible value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "offset_x"))
                {
                    if (!parseInt(trim(value), currentLayer.offsetX))
                    {
                        result.errorMessage = "Invalid offset_x value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (equalsIgnoreCase(key, "offset_y"))
                {
                    if (!parseInt(trim(value), currentLayer.offsetY))
                    {
                        result.errorMessage = "Invalid offset_y value: " + trim(value);
                        return result;
                    }
                    continue;
                }

                if (startsWith(key, "style."))
                {
                    Style style = currentLayer.defaultStyle.value_or(Style{});
                    if (!applyStyleProperty(style, key.substr(6), trim(value)))
                    {
                        result.errorMessage = "Invalid layer style property: " + key;
                        return result;
                    }
                    currentLayer.defaultStyle = style;
                    continue;
                }

                if (equalsIgnoreCase(key, "row"))
                {
                    if (currentLayer.width <= 0 || currentLayer.height <= 0)
                    {
                        result.errorMessage = "Layer dimensions are not initialized.";
                        return result;
                    }

                    int nextRow = 0;
                    while (nextRow < currentLayer.height)
                    {
                        bool rowOccupied = false;
                        for (int x = 0; x < currentLayer.width; ++x)
                        {
                            const GlyphCell* cell = currentLayer.tryGetCell(x, nextRow);
                            if (cell != nullptr && !cell->transparent)
                            {
                                rowOccupied = true;
                                break;
                            }
                        }

                        if (!rowOccupied)
                        {
                            break;
                        }

                        ++nextRow;
                    }

                    if (nextRow >= currentLayer.height)
                    {
                        result.errorMessage = "Too many row entries in layer '" + currentLayer.name + "'.";
                        return result;
                    }

                    const std::u32string rowText = UnicodeConversion::utf8ToU32(value);

                    if (static_cast<int>(rowText.size()) > currentLayer.width)
                    {
                        result.errorMessage = "Row is wider than glyph_width in layer '" + currentLayer.name + "'.";
                        return result;
                    }

                    for (int x = 0; x < currentLayer.width; ++x)
                    {
                        GlyphCell cell;
                        cell.glyph = U' ';
                        cell.transparent = true;

                        if (x < static_cast<int>(rowText.size()))
                        {
                            const char32_t cp = rowText[static_cast<std::size_t>(x)];
                            if (cp != font.transparentMarker)
                            {
                                cell.transparent = false;
                                cell.glyph = cp;
                            }
                        }

                        currentLayer.cells[static_cast<std::size_t>(nextRow * currentLayer.width + x)] = cell;
                    }

                    continue;
                }

                result.errorMessage = "Unsupported layer property: " + key;
                return result;
            }
        }

        if (!finalizeGlyph())
        {
            return result;
        }

        if (font.name.empty())
        {
            font.name = "PseudoFont";
        }

        if (font.glyphs.empty())
        {
            result.errorMessage = "Pseudo font contains no glyphs.";
            return result;
        }

        if (font.findFallbackGlyph() == nullptr)
        {
            addWarning(
                result,
                LoadWarningCode::MissingFallbackGlyph,
                "Fallback glyph is not defined; unknown characters may be skipped.");
        }

        if (options.requireUniformGlyphSize)
        {
            for (const auto& entry : font.glyphs)
            {
                const LayeredGlyph& glyph = entry.second;
                if (glyph.width != font.glyphWidth || glyph.height != font.glyphHeight)
                {
                    addWarning(
                        result,
                        LoadWarningCode::NonUniformGlyphSize,
                        "Glyph size differs from declared font glyph size.");
                }
            }
        }

        result.font = std::move(font);
        result.success = true;
        return result;
    }

    bool tryLoadFromFile(const std::string& filePath, FontDefinition& outFont)
    {
        return tryLoadFromFile(filePath, outFont, LoadOptions{});
    }

    bool tryLoadFromFile(const std::string& filePath, FontDefinition& outFont, const LoadOptions& options)
    {
        const LoadResult result = loadFromFile(filePath, options);
        if (!result.success)
        {
            outFont = FontDefinition{};
            return false;
        }

        outFont = result.font;
        return true;
    }

    TextObject generateTextObject(const FontDefinition& font, std::string_view utf8Text)
    {
        return generateTextObject(font, utf8Text, RenderOptions{});
    }

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const RenderOptions& options)
    {
        if (!font.isLoaded())
        {
            return TextObject{};
        }

        const std::vector<std::u32string> lines = splitTextLinesUtf32(utf8Text);
        const int outputWidth = computeOutputWidth(
            font,
            lines,
            options.alignment,
            options.targetWidth,
            options.useFallbackGlyphForUnknownChars);
        const int outputHeight = computeOutputHeight(font, lines.size());

        if (outputWidth <= 0 || outputHeight <= 0)
        {
            return TextObject{};
        }

        TextObjectBuilder builder(outputWidth, outputHeight);
        builder.fillTransparent();

        int yBase = 0;
        for (const std::u32string& line : lines)
        {
            int xCursor = computeAlignedLineStartX(
                font,
                line,
                outputWidth,
                options.alignment,
                options.useFallbackGlyphForUnknownChars);

            bool first = true;
            for (char32_t cp : line)
            {
                if (cp == U'\t')
                {
                    cp = U' ';
                }

                const LayeredGlyph* glyph = font.findGlyph(cp);
                if (glyph == nullptr && options.useFallbackGlyphForUnknownChars)
                {
                    glyph = font.findFallbackGlyph();
                }

                if (glyph == nullptr)
                {
                    continue;
                }

                if (!first)
                {
                    xCursor += spacingForCodePoint(font, cp);
                }

                std::vector<const GlyphLayer*> layersToRender;
                layersToRender.reserve(glyph->layers.size());

                for (const GlyphLayer& layer : glyph->layers)
                {
                    if (includeLayer(layer, options))
                    {
                        layersToRender.push_back(&layer);
                    }
                }

                std::stable_sort(
                    layersToRender.begin(),
                    layersToRender.end(),
                    [](const GlyphLayer* a, const GlyphLayer* b)
                    {
                        return a->zIndex < b->zIndex;
                    });

                for (const GlyphLayer* layer : layersToRender)
                {
                    if (layer == nullptr)
                    {
                        continue;
                    }

                    stampGlyphLayerToBuilder(
                        builder,
                        font,
                        *glyph,
                        *layer,
                        xCursor,
                        yBase,
                        options.styleMode,
                        options.overrideStyle,
                        options.transparentSpaces,
                        true);
                }

                xCursor += glyph->width;
                first = false;
            }

            yBase += font.glyphHeight + font.spacing.lineSpacing;
        }

        return builder.build();
    }

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle)
    {
        RenderOptions options;
        options.overrideStyle = overrideStyle;
        options.styleMode = StyleMode::ForceOverride;
        return generateTextObject(font, utf8Text, options);
    }

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle,
        const RenderOptions& options)
    {
        RenderOptions resolved = options;
        resolved.overrideStyle = overrideStyle;
        resolved.styleMode = StyleMode::ForceOverride;
        return generateTextObject(font, utf8Text, resolved);
    }

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text)
    {
        return generateLayeredTextObject(font, utf8Text, LayeredRenderOptions{});
    }

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const LayeredRenderOptions& options)
    {
        if (!font.isLoaded())
        {
            return Rendering::LayeredTextObject{};
        }

        const std::vector<std::u32string> lines = splitTextLinesUtf32(utf8Text);
        const int outputWidth = computeOutputWidth(
            font,
            lines,
            options.alignment,
            options.targetWidth,
            options.useFallbackGlyphForUnknownChars);
        const int outputHeight = computeOutputHeight(font, lines.size());

        if (outputWidth <= 0 || outputHeight <= 0)
        {
            return Rendering::LayeredTextObject{};
        }

        Rendering::LayeredTextObject layered(outputWidth, outputHeight);

        int yBase = 0;
        for (const std::u32string& line : lines)
        {
            int xCursor = computeAlignedLineStartX(
                font,
                line,
                outputWidth,
                options.alignment,
                options.useFallbackGlyphForUnknownChars);

            bool first = true;
            for (char32_t cp : line)
            {
                if (cp == U'\t')
                {
                    cp = U' ';
                }

                const LayeredGlyph* glyph = font.findGlyph(cp);
                if (glyph == nullptr && options.useFallbackGlyphForUnknownChars)
                {
                    glyph = font.findFallbackGlyph();
                }

                if (glyph == nullptr)
                {
                    continue;
                }

                if (!first)
                {
                    xCursor += spacingForCodePoint(font, cp);
                }

                for (const GlyphLayer& glyphLayer : glyph->layers)
                {
                    ensureOutputLayer(layered, glyphLayer, options);

                    Rendering::TextObjectLayer* outputLayer = layered.findLayer(glyphLayer.name);
                    if (outputLayer == nullptr)
                    {
                        continue;
                    }

                    TextObjectBuilder builder(outputLayer->object.getWidth(), outputLayer->object.getHeight());
                    builder.fillTransparent();

                    for (int y = 0; y < outputLayer->object.getHeight(); ++y)
                    {
                        for (int x = 0; x < outputLayer->object.getWidth(); ++x)
                        {
                            const TextObjectCell* existing = outputLayer->object.tryGetCell(x, y);
                            if (existing == nullptr)
                            {
                                continue;
                            }

                            if (existing->kind == CellKind::Empty)
                            {
                                continue;
                            }

                            builder.setCell(x, y, existing->glyph, existing->kind, existing->width, existing->style);
                        }
                    }

                    stampGlyphLayerToOutputLayer(
                        builder,
                        font,
                        *glyph,
                        glyphLayer,
                        xCursor,
                        yBase,
                        options.styleMode,
                        options.overrideStyle,
                        options.transparentSpaces);

                    outputLayer->object = builder.build();
                }

                xCursor += glyph->width;
                first = false;
            }

            yBase += font.glyphHeight + font.spacing.lineSpacing;
        }

        return layered;
    }

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle)
    {
        LayeredRenderOptions options;
        options.overrideStyle = overrideStyle;
        options.styleMode = StyleMode::ForceOverride;
        return generateLayeredTextObject(font, utf8Text, options);
    }

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle,
        const LayeredRenderOptions& options)
    {
        LayeredRenderOptions resolved = options;
        resolved.overrideStyle = overrideStyle;
        resolved.styleMode = StyleMode::ForceOverride;
        return generateLayeredTextObject(font, utf8Text, resolved);
    }
}
