#include "Rendering/Styles/ColorResolver.h"

#include "Rendering/Styles/ColorMapping.h"

namespace
{
    ColorResolutionDiagnostics makeOmitted(
        const Style::StyleColorValue& authoredColor,
        ColorSupport support,
        ColorAdaptationReason reason)
    {
        ColorResolutionDiagnostics diagnostics;
        diagnostics.authoredColor = authoredColor;
        diagnostics.supportedTier = support;
        diagnostics.reason = reason;
        diagnostics.resolvedColor.reset();
        return diagnostics;
    }

    ColorResolutionDiagnostics makeResolved(
        const Style::StyleColorValue& authoredColor,
        ColorSupport support,
        const Color& resolvedColor,
        ColorAdaptationReason reason)
    {
        ColorResolutionDiagnostics diagnostics;
        diagnostics.authoredColor = authoredColor;
        diagnostics.supportedTier = support;
        diagnostics.resolvedColor = resolvedColor;
        diagnostics.reason = reason;
        return diagnostics;
    }
}

Color ColorResolver::resolve(
    const Style::StyleColorValue& authoredColor,
    ColorSupport support)
{
    const ColorResolutionDiagnostics diagnostics =
        resolveWithDiagnostics(authoredColor, support);

    if (diagnostics.resolvedColor.has_value())
    {
        return *diagnostics.resolvedColor;
    }

    return Color::Default();
}

std::optional<Color> ColorResolver::resolve(
    const std::optional<Style::StyleColorValue>& authoredColor,
    ColorSupport support)
{
    if (!authoredColor.has_value())
    {
        return std::nullopt;
    }

    const ColorResolutionDiagnostics diagnostics =
        resolveWithDiagnostics(*authoredColor, support);

    return diagnostics.resolvedColor;
}

Color ColorResolver::resolveConcreteColor(
    const Color& color,
    ColorSupport support)
{
    return ColorMapping::mapToSupport(color, support);
}

Color ColorResolver::resolveThemeColor(
    const ThemeColor& themeColor,
    ColorSupport support)
{
    switch (support)
    {
    case ColorSupport::None:
        return resolveThemeColorForNone(themeColor);

    case ColorSupport::Basic16:
        return resolveThemeColorForBasic16(themeColor);

    case ColorSupport::Indexed256:
        return resolveThemeColorForIndexed256(themeColor);

    case ColorSupport::Rgb24:
        return resolveThemeColorForRgb24(themeColor);

    default:
        return resolveThemeColorForBasic16(themeColor);
    }
}

ColorResolutionDiagnostics ColorResolver::resolveWithDiagnostics(
    const Style::StyleColorValue& authoredColor,
    ColorSupport support)
{
    if (support == ColorSupport::None)
    {
        return makeOmitted(
            authoredColor,
            support,
            ColorAdaptationReason::OmittedNoColorSupport);
    }

    if (authoredColor.hasConcreteColor())
    {
        const Color& color = *authoredColor.concreteColor();

        if (color.isBasic16())
        {
            return makeResolved(
                authoredColor,
                support,
                ColorMapping::mapToSupport(color, support),
                ColorAdaptationReason::ConcreteBasicUsedDirect);
        }

        if (color.isIndexed256())
        {
            if (support >= ColorSupport::Indexed256)
            {
                return makeResolved(
                    authoredColor,
                    support,
                    ColorMapping::mapToSupport(color, support),
                    ColorAdaptationReason::ConcreteIndexedUsedDirect);
            }

            return makeResolved(
                authoredColor,
                support,
                ColorMapping::indexedToBasic(color),
                ColorAdaptationReason::ConcreteIndexedDowngradedToBasic16);
        }

        if (color.isRgb())
        {
            if (support >= ColorSupport::Rgb24)
            {
                return makeResolved(
                    authoredColor,
                    support,
                    color,
                    ColorAdaptationReason::ConcreteRgbUsedDirect);
            }

            if (support >= ColorSupport::Indexed256)
            {
                return makeResolved(
                    authoredColor,
                    support,
                    ColorMapping::rgbToIndexed(color),
                    ColorAdaptationReason::ConcreteRgbDowngradedToIndexed256);
            }

            return makeResolved(
                authoredColor,
                support,
                ColorMapping::rgbToBasic(color),
                ColorAdaptationReason::ConcreteRgbDowngradedToBasic16);
        }

        return makeResolved(
            authoredColor,
            support,
            ColorMapping::mapToSupport(color, support),
            ColorAdaptationReason::ConcreteBasicUsedDirect);
    }

    if (authoredColor.hasThemeColor())
    {
        const ThemeColor& themeColor = *authoredColor.themeColor();

        switch (support)
        {
        case ColorSupport::Basic16:
            if (themeColor.hasRgb())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    ColorMapping::rgbToBasic(*themeColor.rgb()),
                    ColorAdaptationReason::ThemeRgbDowngradedToBasic16);
            }

            if (themeColor.hasIndexed())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    ColorMapping::indexedToBasic(*themeColor.indexed()),
                    ColorAdaptationReason::ThemeIndexedDowngradedToBasic16);
            }

            if (themeColor.hasBasic())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    *themeColor.basic(),
                    ColorAdaptationReason::ThemeBasicUsedDirect);
            }

            break;

        case ColorSupport::Indexed256:
            if (themeColor.hasRgb())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    ColorMapping::rgbToIndexed(*themeColor.rgb()),
                    ColorAdaptationReason::ThemeRgbDowngradedToIndexed256);
            }

            if (themeColor.hasIndexed())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    *themeColor.indexed(),
                    ColorAdaptationReason::ThemeIndexedUsedDirect);
            }

            if (themeColor.hasBasic())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    *themeColor.basic(),
                    ColorAdaptationReason::ThemeBasicUsedDirect);
            }

            break;

        case ColorSupport::Rgb24:
            if (themeColor.hasRgb())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    *themeColor.rgb(),
                    ColorAdaptationReason::ThemeRgbUsedDirect);
            }

            if (themeColor.hasIndexed())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    *themeColor.indexed(),
                    ColorAdaptationReason::ThemeIndexedUsedDirect);
            }

            if (themeColor.hasBasic())
            {
                return makeResolved(
                    authoredColor,
                    support,
                    *themeColor.basic(),
                    ColorAdaptationReason::ThemeBasicUsedDirect);
            }

            break;

        case ColorSupport::None:
        default:
            break;
        }
    }

    return makeOmitted(
        authoredColor,
        support,
        ColorAdaptationReason::OmittedNoColorSupport);
}

ColorResolutionDiagnostics ColorResolver::omittedByPolicy(
    const Style::StyleColorValue& authoredColor)
{
    return makeOmitted(
        authoredColor,
        ColorSupport::None,
        ColorAdaptationReason::OmittedByPolicy);
}

Color ColorResolver::resolveThemeColorForNone(const ThemeColor& themeColor)
{
    (void)themeColor;
    return Color::Default();
}

Color ColorResolver::resolveThemeColorForBasic16(const ThemeColor& themeColor)
{
    if (themeColor.hasRgb())
    {
        return ColorMapping::rgbToBasic(*themeColor.rgb());
    }

    if (themeColor.hasIndexed())
    {
        return ColorMapping::indexedToBasic(*themeColor.indexed());
    }

    if (themeColor.hasBasic())
    {
        return ColorMapping::mapToSupport(*themeColor.basic(), ColorSupport::Basic16);
    }

    return Color::Default();
}

Color ColorResolver::resolveThemeColorForIndexed256(const ThemeColor& themeColor)
{
    if (themeColor.hasRgb())
    {
        return ColorMapping::rgbToIndexed(*themeColor.rgb());
    }

    if (themeColor.hasIndexed())
    {
        return ColorMapping::mapToSupport(*themeColor.indexed(), ColorSupport::Indexed256);
    }

    if (themeColor.hasBasic())
    {
        return ColorMapping::mapToSupport(*themeColor.basic(), ColorSupport::Indexed256);
    }

    return Color::Default();
}

Color ColorResolver::resolveThemeColorForRgb24(const ThemeColor& themeColor)
{
    if (themeColor.hasRgb())
    {
        return *themeColor.rgb();
    }

    if (themeColor.hasIndexed())
    {
        return *themeColor.indexed();
    }

    if (themeColor.hasBasic())
    {
        return *themeColor.basic();
    }

    return Color::Default();
}