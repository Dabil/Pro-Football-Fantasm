#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Assets/Models/AsciiBannerFont.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

class TextObjectBuilder;

class AsciiBanner
{
public:
    enum class ComposeMode
    {
        FullWidth,
        Kern
    };

    enum class Alignment
    {
        Left,
        Center,
        Right
    };

    struct RenderOptions
    {
        ComposeMode composeMode = ComposeMode::Kern;
        Alignment alignment = Alignment::Left;
        std::size_t targetWidth = 0;

        bool replaceHardBlankWithSpace = true;
        bool trimTrailingSpaces = true;
        bool useFallbackGlyphForUnknownChars = true;

        bool transparentSpaces = true;
    };

public:
    static std::string render(
        const AsciiBannerFont& font,
        std::string_view text);

    static std::string render(
        const AsciiBannerFont& font,
        std::string_view text,
        const RenderOptions& options);

    static std::vector<std::string> renderLines(
        const AsciiBannerFont& font,
        std::string_view text);

    static std::vector<std::string> renderLines(
        const AsciiBannerFont& font,
        std::string_view text,
        const RenderOptions& options);

    static TextObject generateTextObject(
        const AsciiBannerFont& font,
        std::string_view text);

    static TextObject generateTextObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const RenderOptions& options);

    static TextObject generateTextObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& style);

    static TextObject generateTextObject(
        const AsciiBannerFont& font,
        std::string_view text,
        const Style& style,
        const RenderOptions& options);

private:
    static std::string trimRight(const std::string& s);
    static std::vector<std::string> splitLines(std::string_view text);

    static int getMinimumLeadingSpaces(const AsciiBannerFont::GlyphRows& glyph);
    static int getMinimumTrailingSpaces(const std::vector<std::string>& lines);

    static const AsciiBannerFont::GlyphRows* findGlyph(
        const AsciiBannerFont& font,
        int codePoint);

    static const AsciiBannerFont::GlyphRows& getFallbackGlyph(
        const AsciiBannerFont& font);

    static std::vector<std::string> renderSingleLine(
        const AsciiBannerFont& font,
        std::string_view text,
        const RenderOptions& options);

    static void appendGlyphFullWidth(
        std::vector<std::string>& outputLines,
        const AsciiBannerFont::GlyphRows& glyph);

    static void appendGlyphKern(
        std::vector<std::string>& outputLines,
        const AsciiBannerFont::GlyphRows& glyph);

    static std::vector<std::string> alignLines(
        const std::vector<std::string>& lines,
        Alignment alignment,
        std::size_t targetWidth);

    static std::string joinLines(
        const std::vector<std::string>& lines,
        bool trimTrailingSpaces);

    static int measureRenderedLineWidth(std::string_view line);

    static void appendRenderedLineToBuilder(
        TextObjectBuilder& builder,
        int y,
        std::string_view line,
        bool transparentSpaces,
        const std::optional<Style>& style);
};
