#pragma once

#include <optional>

#include "Rendering/Capabilities/ColorSupport.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/ColorResolutionDiagnostics.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/ThemeColor.h"

class ColorResolver
{
public:
    static Color resolve(
        const Style::StyleColorValue& authoredColor,
        ColorSupport support);

    static std::optional<Color> resolve(
        const std::optional<Style::StyleColorValue>& authoredColor,
        ColorSupport support);

    static Color resolveConcreteColor(
        const Color& color,
        ColorSupport support);

    static Color resolveThemeColor(
        const ThemeColor& themeColor,
        ColorSupport support);

    static ColorResolutionDiagnostics resolveWithDiagnostics(
        const Style::StyleColorValue& authoredColor,
        ColorSupport support);

    static ColorResolutionDiagnostics omittedByPolicy(
        const Style::StyleColorValue& authoredColor);

private:
    static Color resolveThemeColorForNone(const ThemeColor& themeColor);
    static Color resolveThemeColorForBasic16(const ThemeColor& themeColor);
    static Color resolveThemeColorForIndexed256(const ThemeColor& themeColor);
    static Color resolveThemeColorForRgb24(const ThemeColor& themeColor);
};