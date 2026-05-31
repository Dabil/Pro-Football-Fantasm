#pragma once

#include <optional>

#include "Rendering/Capabilities/ColorSupport.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/ColorResolutionDiagnostics.h"
#include "Rendering/Styles/Style.h"

enum class ColorRenderMode
{
    Direct,
    DowngradeToBasic,
    DowngradeToIndexed256,
    Omit
};

enum class TextAttributeRenderMode
{
    Direct,
    Approximate,
    Omit
};

enum class BlinkRenderMode
{
    Direct,
    Omit,
    Emulate
};

struct ResolvedStyle
{
    Style presentedStyle;
    bool emulateSlowBlink = false;
    bool emulateFastBlink = false;

    std::optional<ColorResolutionDiagnostics> foregroundColorDiagnostics;
    std::optional<ColorResolutionDiagnostics> backgroundColorDiagnostics;
};

class StylePolicy
{
public:
    StylePolicy();

    static StylePolicy PreserveIntent();
    static StylePolicy BasicConsole();
    static StylePolicy BasicConsoleWithBlinkEmulation();

    ResolvedStyle resolve(const Style& style) const;

    ColorRenderMode basicColorMode() const;
    ColorRenderMode indexed256ColorMode() const;
    ColorRenderMode trueColorColorMode() const;
    ColorRenderMode rgbColorMode() const;

    TextAttributeRenderMode boldMode() const;
    TextAttributeRenderMode dimMode() const;
    TextAttributeRenderMode underlineMode() const;
    TextAttributeRenderMode reverseMode() const;
    TextAttributeRenderMode invisibleMode() const;
    TextAttributeRenderMode strikeMode() const;

    BlinkRenderMode slowBlinkMode() const;
    BlinkRenderMode fastBlinkMode() const;

    StylePolicy withBasicColorMode(ColorRenderMode mode) const;
    StylePolicy withIndexed256ColorMode(ColorRenderMode mode) const;
    StylePolicy withTrueColorColorMode(ColorRenderMode mode) const;
    StylePolicy withRgbColorMode(ColorRenderMode mode) const;

    StylePolicy withBoldMode(TextAttributeRenderMode mode) const;
    StylePolicy withDimMode(TextAttributeRenderMode mode) const;
    StylePolicy withUnderlineMode(TextAttributeRenderMode mode) const;
    StylePolicy withReverseMode(TextAttributeRenderMode mode) const;
    StylePolicy withInvisibleMode(TextAttributeRenderMode mode) const;
    StylePolicy withStrikeMode(TextAttributeRenderMode mode) const;

    StylePolicy withSlowBlinkMode(BlinkRenderMode mode) const;
    StylePolicy withFastBlinkMode(BlinkRenderMode mode) const;

private:
    std::optional<ColorSupport> selectColorSupportForAuthoredColor(
        const Style::StyleColorValue& authoredColor) const;

    std::optional<ColorSupport> selectColorSupportForConcreteColor(
        const Color& color) const;

    std::optional<ColorSupport> selectColorSupportForThemeColor(
        const ThemeColor& themeColor) const;

    std::optional<ColorSupport> supportFromMode(
        ColorRenderMode mode,
        ColorSupport directSupport) const;

private:
    ColorRenderMode m_basicColorMode = ColorRenderMode::Direct;
    ColorRenderMode m_indexed256ColorMode = ColorRenderMode::Direct;
    ColorRenderMode m_trueColorColorMode = ColorRenderMode::Direct;

    TextAttributeRenderMode m_boldMode = TextAttributeRenderMode::Direct;
    TextAttributeRenderMode m_dimMode = TextAttributeRenderMode::Direct;
    TextAttributeRenderMode m_underlineMode = TextAttributeRenderMode::Direct;
    TextAttributeRenderMode m_reverseMode = TextAttributeRenderMode::Direct;
    TextAttributeRenderMode m_invisibleMode = TextAttributeRenderMode::Direct;
    TextAttributeRenderMode m_strikeMode = TextAttributeRenderMode::Direct;

    BlinkRenderMode m_slowBlinkMode = BlinkRenderMode::Direct;
    BlinkRenderMode m_fastBlinkMode = BlinkRenderMode::Direct;
};