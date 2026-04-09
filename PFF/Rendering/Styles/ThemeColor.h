#pragma once

#include <optional>

#include "Rendering/Styles/Color.h"

class ThemeColor
{
public:
    ThemeColor();

    static ThemeColor Basic(const Color& basic);
    static ThemeColor Indexed(const Color& indexed);
    static ThemeColor Rgb(const Color& rgb);

    static ThemeColor BasicIndexed(
        const Color& basic,
        const Color& indexed);

    static ThemeColor BasicRgb(
        const Color& basic,
        const Color& rgb);

    static ThemeColor IndexedRgb(
        const Color& indexed,
        const Color& rgb);

    static ThemeColor Basic256Rgb(
        const Color& basic,
        const Color& indexed,
        const Color& rgb);

    ThemeColor WithBasic(const Color& basic) const;
    ThemeColor WithIndexed(const Color& indexed) const;
    ThemeColor WithRgb(const Color& rgb) const;

    bool hasBasic() const;
    bool hasIndexed() const;
    bool hasRgb() const;

    const std::optional<Color>& basic() const;
    const std::optional<Color>& indexed() const;
    const std::optional<Color>& rgb() const;

    bool operator==(const ThemeColor& other) const;
    bool operator!=(const ThemeColor& other) const;

private:
    static void validateBasicColor(const Color& color);
    static void validateIndexedColor(const Color& color);
    static void validateRgbColor(const Color& color);

private:
    std::optional<Color> m_basic;
    std::optional<Color> m_indexed;
    std::optional<Color> m_rgb;
};