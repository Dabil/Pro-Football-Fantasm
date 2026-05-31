#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Rendering/Objects/LayeredTextObject.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace PseudoFont
{
    enum class Alignment
    {
        Left,
        Center,
        Right
    };

    struct GlyphCell
    {
        char32_t glyph = U' ';
        bool transparent = true;
        std::optional<Style> style;
    };

    struct GlyphLayer
    {
        std::string name;
        int zIndex = 0;
        bool visibleByDefault = true;
        int offsetX = 0;
        int offsetY = 0;

        int width = 0;
        int height = 0;
        std::vector<GlyphCell> cells;

        std::optional<Style> defaultStyle;

        bool isValid() const;
        const GlyphCell* tryGetCell(int x, int y) const;
    };

    struct LayeredGlyph
    {
        char32_t codePoint = U'\0';
        int width = 0;
        int height = 0;
        std::vector<GlyphLayer> layers;
        std::optional<Style> defaultStyle;

        bool isValid() const;
        const GlyphLayer* findLayer(std::string_view name) const;
    };

    struct SpacingRules
    {
        int letterSpacing = 1;
        int wordSpacing = 3;
        int lineSpacing = 0;
    };

    struct FontDefinition
    {
        using GlyphMap = std::unordered_map<char32_t, LayeredGlyph>;

        std::string name;
        std::string sourcePath;
        GlyphMap glyphs;

        int glyphWidth = 0;
        int glyphHeight = 0;
        bool requiresUniformGlyphSize = true;

        SpacingRules spacing;
        std::optional<Style> defaultStyle;

        char32_t transparentMarker = U'.';
        char32_t fallbackCodePoint = U'?';

        bool isLoaded() const;
        const LayeredGlyph* findGlyph(char32_t codePoint) const;
        const LayeredGlyph* findFallbackGlyph() const;
    };

    enum class LoadWarningCode
    {
        None,
        DuplicateGlyphReplaced,
        DuplicateLayerReplaced,
        MissingFallbackGlyph,
        MissingBaseLayer,
        NonUniformGlyphSize,
        UnknownPropertyIgnored,
        ConflictingLayerMetadata
    };

    struct LoadWarning
    {
        LoadWarningCode code = LoadWarningCode::None;
        std::string message;
    };

    struct LoadOptions
    {
        bool requireUniformGlyphSize = true;
        bool requireAtLeastOneLayerPerGlyph = true;
        bool trimTrailingCarriageReturn = true;
        bool treatUnknownPropertiesAsWarnings = true;
    };

    struct LoadResult
    {
        FontDefinition font;
        bool success = false;
        std::vector<LoadWarning> warnings;
        std::string errorMessage;
    };

    enum class StyleMode
    {
        PreserveAuthored,
        ApplyOverrideWhereMissing,
        ForceOverride
    };

    enum class LayerSelectionMode
    {
        VisibleByDefaultOnly,
        AllLayers,
        NamedLayerSet
    };

    struct RenderOptions
    {
        Alignment alignment = Alignment::Left;
        std::size_t targetWidth = 0;

        bool useFallbackGlyphForUnknownChars = true;
        bool transparentSpaces = true;

        LayerSelectionMode layerSelectionMode = LayerSelectionMode::VisibleByDefaultOnly;
        std::vector<std::string> enabledLayerNames;

        StyleMode styleMode = StyleMode::PreserveAuthored;
        std::optional<Style> overrideStyle;
    };

    struct LayeredRenderOptions
    {
        Alignment alignment = Alignment::Left;
        std::size_t targetWidth = 0;

        bool useFallbackGlyphForUnknownChars = true;
        bool transparentSpaces = true;

        enum class InitialVisibilityMode
        {
            UseFontDefaults,
            AllVisible,
            AllHidden
        } initialVisibilityMode = InitialVisibilityMode::UseFontDefaults;

        StyleMode styleMode = StyleMode::PreserveAuthored;
        std::optional<Style> overrideStyle;
    };

    LoadResult loadFromFile(const std::string& filePath);
    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options);

    bool tryLoadFromFile(const std::string& filePath, FontDefinition& outFont);
    bool tryLoadFromFile(const std::string& filePath, FontDefinition& outFont, const LoadOptions& options);

    const char* toString(LoadWarningCode code);

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text);

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const RenderOptions& options);

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle);

    TextObject generateTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle,
        const RenderOptions& options);

    // Returns a composition-aware layered asset that preserves named layers.
    // Layer offsets remain as layer metadata and are not re-baked into the returned
    // TextObject surfaces. Flatten later when a single placement-ready asset is needed.
    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text);

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const LayeredRenderOptions& options);

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle);

    Rendering::LayeredTextObject generateLayeredTextObject(
        const FontDefinition& font,
        std::string_view utf8Text,
        const Style& overrideStyle,
        const LayeredRenderOptions& options);
}
