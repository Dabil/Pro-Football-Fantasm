#include "Rendering/Styles/ThemeColor.h"

#include <stdexcept>

ThemeColor::ThemeColor() = default;

ThemeColor ThemeColor::Basic(const Color& basic)
{
    ThemeColor color;
    validateBasicColor(basic);
    color.m_basic = basic;
    return color;
}

ThemeColor ThemeColor::Indexed(const Color& indexed)
{
    ThemeColor color;
    validateIndexedColor(indexed);
    color.m_indexed = indexed;
    return color;
}

ThemeColor ThemeColor::Rgb(const Color& rgb)
{
    ThemeColor color;
    validateRgbColor(rgb);
    color.m_rgb = rgb;
    return color;
}

ThemeColor ThemeColor::BasicIndexed(
    const Color& basic,
    const Color& indexed)
{
    ThemeColor color;
    validateBasicColor(basic);
    validateIndexedColor(indexed);

    color.m_basic = basic;
    color.m_indexed = indexed;
    return color;
}

ThemeColor ThemeColor::BasicRgb(
    const Color& basic,
    const Color& rgb)
{
    ThemeColor color;
    validateBasicColor(basic);
    validateRgbColor(rgb);

    color.m_basic = basic;
    color.m_rgb = rgb;
    return color;
}

ThemeColor ThemeColor::IndexedRgb(
    const Color& indexed,
    const Color& rgb)
{
    ThemeColor color;
    validateIndexedColor(indexed);
    validateRgbColor(rgb);

    color.m_indexed = indexed;
    color.m_rgb = rgb;
    return color;
}

ThemeColor ThemeColor::Basic256Rgb(
    const Color& basic,
    const Color& indexed,
    const Color& rgb)
{
    ThemeColor color;
    validateBasicColor(basic);
    validateIndexedColor(indexed);
    validateRgbColor(rgb);

    color.m_basic = basic;
    color.m_indexed = indexed;
    color.m_rgb = rgb;
    return color;
}

ThemeColor ThemeColor::WithBasic(const Color& basic) const
{
    ThemeColor copy = *this;
    validateBasicColor(basic);
    copy.m_basic = basic;
    return copy;
}

ThemeColor ThemeColor::WithIndexed(const Color& indexed) const
{
    ThemeColor copy = *this;
    validateIndexedColor(indexed);
    copy.m_indexed = indexed;
    return copy;
}

ThemeColor ThemeColor::WithRgb(const Color& rgb) const
{
    ThemeColor copy = *this;
    validateRgbColor(rgb);
    copy.m_rgb = rgb;
    return copy;
}

bool ThemeColor::hasBasic() const
{
    return m_basic.has_value();
}

bool ThemeColor::hasIndexed() const
{
    return m_indexed.has_value();
}

bool ThemeColor::hasRgb() const
{
    return m_rgb.has_value();
}

const std::optional<Color>& ThemeColor::basic() const
{
    return m_basic;
}

const std::optional<Color>& ThemeColor::indexed() const
{
    return m_indexed;
}

const std::optional<Color>& ThemeColor::rgb() const
{
    return m_rgb;
}

bool ThemeColor::operator==(const ThemeColor& other) const
{
    return m_basic == other.m_basic
        && m_indexed == other.m_indexed
        && m_rgb == other.m_rgb;
}

bool ThemeColor::operator!=(const ThemeColor& other) const
{
    return !(*this == other);
}

void ThemeColor::validateBasicColor(const Color& color)
{
    if (!color.isBasic() && !color.isBasic16())
    {
        throw std::invalid_argument("ThemeColor basic slot requires a Basic16 Color.");
    }
}

void ThemeColor::validateIndexedColor(const Color& color)
{
    if (!color.isIndexed() && !color.isIndexed256())
    {
        throw std::invalid_argument("ThemeColor indexed slot requires an Indexed256 Color.");
    }
}

void ThemeColor::validateRgbColor(const Color& color)
{
    if (!color.isRgb() && !color.isTrueColor())
    {
        throw std::invalid_argument("ThemeColor rgb slot requires an RGB Color.");
    }
}