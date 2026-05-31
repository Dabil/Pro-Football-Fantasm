#include "Rendering/Styles/StylePolicy.h"

#include "Rendering/Styles/ColorResolver.h"

StylePolicy::StylePolicy() = default;

StylePolicy StylePolicy::PreserveIntent()
{
    return StylePolicy{};
}

StylePolicy StylePolicy::BasicConsole()
{
    StylePolicy policy;
    policy.m_basicColorMode = ColorRenderMode::Direct;
    policy.m_indexed256ColorMode = ColorRenderMode::DowngradeToBasic;
    policy.m_trueColorColorMode = ColorRenderMode::DowngradeToBasic;

    policy.m_boldMode = TextAttributeRenderMode::Approximate;
    policy.m_dimMode = TextAttributeRenderMode::Approximate;
    policy.m_underlineMode = TextAttributeRenderMode::Approximate;
    policy.m_reverseMode = TextAttributeRenderMode::Approximate;
    policy.m_invisibleMode = TextAttributeRenderMode::Approximate;
    policy.m_strikeMode = TextAttributeRenderMode::Omit;

    policy.m_slowBlinkMode = BlinkRenderMode::Omit;
    policy.m_fastBlinkMode = BlinkRenderMode::Omit;

    return policy;
}

StylePolicy StylePolicy::BasicConsoleWithBlinkEmulation()
{
    StylePolicy policy = BasicConsole();
    policy.m_slowBlinkMode = BlinkRenderMode::Emulate;
    policy.m_fastBlinkMode = BlinkRenderMode::Emulate;
    return policy;
}

ResolvedStyle StylePolicy::resolve(const Style& style) const
{
    ResolvedStyle result;
    result.presentedStyle = style;

    if (style.hasForegroundColorValue())
    {
        const Style::StyleColorValue& authoredColor = *style.foregroundColorValue();
        const std::optional<ColorSupport> support =
            selectColorSupportForAuthoredColor(authoredColor);

        if (support.has_value())
        {
            result.foregroundColorDiagnostics =
                ColorResolver::resolveWithDiagnostics(authoredColor, *support);
        }
        else
        {
            result.foregroundColorDiagnostics =
                ColorResolver::omittedByPolicy(authoredColor);
        }

        if (result.foregroundColorDiagnostics->resolvedColor.has_value())
        {
            result.presentedStyle =
                result.presentedStyle.withForeground(
                    *result.foregroundColorDiagnostics->resolvedColor);
        }
        else
        {
            result.presentedStyle = result.presentedStyle.withoutForeground();
        }
    }

    if (style.hasBackgroundColorValue())
    {
        const Style::StyleColorValue& authoredColor = *style.backgroundColorValue();
        const std::optional<ColorSupport> support =
            selectColorSupportForAuthoredColor(authoredColor);

        if (support.has_value())
        {
            result.backgroundColorDiagnostics =
                ColorResolver::resolveWithDiagnostics(authoredColor, *support);
        }
        else
        {
            result.backgroundColorDiagnostics =
                ColorResolver::omittedByPolicy(authoredColor);
        }

        if (result.backgroundColorDiagnostics->resolvedColor.has_value())
        {
            result.presentedStyle =
                result.presentedStyle.withBackground(
                    *result.backgroundColorDiagnostics->resolvedColor);
        }
        else
        {
            result.presentedStyle = result.presentedStyle.withoutBackground();
        }
    }

    if (result.presentedStyle.bold() && m_boldMode == TextAttributeRenderMode::Omit)
    {
        result.presentedStyle = result.presentedStyle.withBold(false);
    }

    if (result.presentedStyle.dim() && m_dimMode == TextAttributeRenderMode::Omit)
    {
        result.presentedStyle = result.presentedStyle.withDim(false);
    }

    if (result.presentedStyle.underline() && m_underlineMode == TextAttributeRenderMode::Omit)
    {
        result.presentedStyle = result.presentedStyle.withUnderline(false);
    }

    if (result.presentedStyle.reverse() && m_reverseMode == TextAttributeRenderMode::Omit)
    {
        result.presentedStyle = result.presentedStyle.withReverse(false);
    }

    if (result.presentedStyle.invisible() && m_invisibleMode == TextAttributeRenderMode::Omit)
    {
        result.presentedStyle = result.presentedStyle.withInvisible(false);
    }

    if (result.presentedStyle.strike() && m_strikeMode == TextAttributeRenderMode::Omit)
    {
        result.presentedStyle = result.presentedStyle.withStrike(false);
    }

    if (result.presentedStyle.slowBlink())
    {
        if (m_slowBlinkMode == BlinkRenderMode::Omit)
        {
            result.presentedStyle = result.presentedStyle.withSlowBlink(false);
        }
        else if (m_slowBlinkMode == BlinkRenderMode::Emulate)
        {
            result.presentedStyle = result.presentedStyle.withSlowBlink(false);
            result.emulateSlowBlink = true;
        }
    }

    if (result.presentedStyle.fastBlink())
    {
        if (m_fastBlinkMode == BlinkRenderMode::Omit)
        {
            result.presentedStyle = result.presentedStyle.withFastBlink(false);
        }
        else if (m_fastBlinkMode == BlinkRenderMode::Emulate)
        {
            result.presentedStyle = result.presentedStyle.withFastBlink(false);
            result.emulateFastBlink = true;
        }
    }

    return result;
}

ColorRenderMode StylePolicy::basicColorMode() const
{
    return m_basicColorMode;
}

ColorRenderMode StylePolicy::indexed256ColorMode() const
{
    return m_indexed256ColorMode;
}

ColorRenderMode StylePolicy::trueColorColorMode() const
{
    return m_trueColorColorMode;
}

ColorRenderMode StylePolicy::rgbColorMode() const
{
    return trueColorColorMode();
}

TextAttributeRenderMode StylePolicy::boldMode() const
{
    return m_boldMode;
}

TextAttributeRenderMode StylePolicy::dimMode() const
{
    return m_dimMode;
}

TextAttributeRenderMode StylePolicy::underlineMode() const
{
    return m_underlineMode;
}

TextAttributeRenderMode StylePolicy::reverseMode() const
{
    return m_reverseMode;
}

TextAttributeRenderMode StylePolicy::invisibleMode() const
{
    return m_invisibleMode;
}

TextAttributeRenderMode StylePolicy::strikeMode() const
{
    return m_strikeMode;
}

BlinkRenderMode StylePolicy::slowBlinkMode() const
{
    return m_slowBlinkMode;
}

BlinkRenderMode StylePolicy::fastBlinkMode() const
{
    return m_fastBlinkMode;
}

StylePolicy StylePolicy::withBasicColorMode(ColorRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_basicColorMode = mode;
    return copy;
}

StylePolicy StylePolicy::withIndexed256ColorMode(ColorRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_indexed256ColorMode = mode;
    return copy;
}

StylePolicy StylePolicy::withTrueColorColorMode(ColorRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_trueColorColorMode = mode;
    return copy;
}

StylePolicy StylePolicy::withRgbColorMode(ColorRenderMode mode) const
{
    return withTrueColorColorMode(mode);
}

StylePolicy StylePolicy::withBoldMode(TextAttributeRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_boldMode = mode;
    return copy;
}

StylePolicy StylePolicy::withDimMode(TextAttributeRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_dimMode = mode;
    return copy;
}

StylePolicy StylePolicy::withUnderlineMode(TextAttributeRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_underlineMode = mode;
    return copy;
}

StylePolicy StylePolicy::withReverseMode(TextAttributeRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_reverseMode = mode;
    return copy;
}

StylePolicy StylePolicy::withInvisibleMode(TextAttributeRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_invisibleMode = mode;
    return copy;
}

StylePolicy StylePolicy::withStrikeMode(TextAttributeRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_strikeMode = mode;
    return copy;
}

StylePolicy StylePolicy::withSlowBlinkMode(BlinkRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_slowBlinkMode = mode;
    return copy;
}

StylePolicy StylePolicy::withFastBlinkMode(BlinkRenderMode mode) const
{
    StylePolicy copy = *this;
    copy.m_fastBlinkMode = mode;
    return copy;
}

std::optional<ColorSupport> StylePolicy::selectColorSupportForAuthoredColor(
    const Style::StyleColorValue& authoredColor) const
{
    if (authoredColor.hasConcreteColor())
    {
        return selectColorSupportForConcreteColor(*authoredColor.concreteColor());
    }

    if (authoredColor.hasThemeColor())
    {
        return selectColorSupportForThemeColor(*authoredColor.themeColor());
    }

    return std::nullopt;
}

std::optional<ColorSupport> StylePolicy::selectColorSupportForConcreteColor(
    const Color& color) const
{
    if (color.isDefault())
    {
        return ColorSupport::Basic16;
    }

    if (color.isBasic() || color.isBasic16())
    {
        return supportFromMode(m_basicColorMode, ColorSupport::Basic16);
    }

    if (color.isIndexed() || color.isIndexed256())
    {
        return supportFromMode(m_indexed256ColorMode, ColorSupport::Indexed256);
    }

    if (color.isRgb() || color.isTrueColor())
    {
        return supportFromMode(m_trueColorColorMode, ColorSupport::Rgb24);
    }

    return std::nullopt;
}

std::optional<ColorSupport> StylePolicy::selectColorSupportForThemeColor(
    const ThemeColor& themeColor) const
{
    if (themeColor.hasRgb())
    {
        return supportFromMode(m_trueColorColorMode, ColorSupport::Rgb24);
    }

    if (themeColor.hasIndexed())
    {
        return supportFromMode(m_indexed256ColorMode, ColorSupport::Indexed256);
    }

    if (themeColor.hasBasic())
    {
        return supportFromMode(m_basicColorMode, ColorSupport::Basic16);
    }

    return std::nullopt;
}

std::optional<ColorSupport> StylePolicy::supportFromMode(
    ColorRenderMode mode,
    ColorSupport directSupport) const
{
    switch (mode)
    {
    case ColorRenderMode::Direct:
        return directSupport;

    case ColorRenderMode::DowngradeToBasic:
        return ColorSupport::Basic16;

    case ColorRenderMode::DowngradeToIndexed256:
        return ColorSupport::Indexed256;

    case ColorRenderMode::Omit:
        return std::nullopt;

    default:
        return directSupport;
    }
}