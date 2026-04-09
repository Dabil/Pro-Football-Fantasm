#include "Rendering/Styles/ColorMapping.h"

namespace
{
    int squareDistance(
        std::uint8_t r1, std::uint8_t g1, std::uint8_t b1,
        std::uint8_t r2, std::uint8_t g2, std::uint8_t b2)
    {
        const int dr = static_cast<int>(r1) - static_cast<int>(r2);
        const int dg = static_cast<int>(g1) - static_cast<int>(g2);
        const int db = static_cast<int>(b1) - static_cast<int>(b2);

        return dr * dr + dg * dg + db * db;
    }
}

Color ColorMapping::mapToSupport(const Color& color, ColorSupport support)
{
    if (color.isDefault())
    {
        return color;
    }

    switch (support)
    {
    case ColorSupport::None:
        return Color::Default();

    case ColorSupport::Basic16:
        if (color.isBasic16())
        {
            return color;
        }

        if (color.isIndexed256())
        {
            return indexedToBasic(color);
        }

        if (color.isRgb())
        {
            return rgbToBasic(color);
        }

        return Color::Default();

    case ColorSupport::Indexed256:
        if (color.isBasic16() || color.isIndexed256())
        {
            return color;
        }

        if (color.isRgb())
        {
            return rgbToIndexed(color);
        }

        return Color::Default();

    case ColorSupport::Rgb24:
        /*
            All current concrete Color kinds are usable on an RGB-capable backend.
            Do not rewrite authored Basic/Indexed colors into synthetic RGB here.
        */
        return color;

    default:
        return Color::Default();
    }
}

Color ColorMapping::rgbToIndexed(const Color& color)
{
    if (color.isDefault() || color.isIndexed256())
    {
        return color;
    }

    if (color.isBasic16())
    {
        return basicToIndexed(color);
    }

    if (!color.isRgb())
    {
        return Color::Default();
    }

    return Color::FromIndexed(
        rgbToNearestIndexed256(color.red(), color.green(), color.blue()));
}

Color ColorMapping::indexedToBasic(const Color& color)
{
    if (color.isDefault() || color.isBasic16())
    {
        return color;
    }

    if (!color.isIndexed256())
    {
        return Color::Default();
    }

    const RgbValue rgb = indexedToRgb(color.index256());
    return Color::FromBasic(rgbToNearestBasic(rgb.red, rgb.green, rgb.blue));
}

Color ColorMapping::rgbToBasic(const Color& color)
{
    if (color.isDefault() || color.isBasic16())
    {
        return color;
    }

    if (color.isIndexed256())
    {
        return indexedToBasic(color);
    }

    if (!color.isRgb())
    {
        return Color::Default();
    }

    return Color::FromBasic(
        rgbToNearestBasic(color.red(), color.green(), color.blue()));
}

Color ColorMapping::basicToIndexed(const Color& color)
{
    if (color.isDefault() || color.isIndexed256())
    {
        return color;
    }

    if (!color.isBasic16())
    {
        return Color::Default();
    }

    switch (color.basic())
    {
    case Color::Basic::Black:         return Color::FromIndexed(0);
    case Color::Basic::Red:           return Color::FromIndexed(1);
    case Color::Basic::Green:         return Color::FromIndexed(2);
    case Color::Basic::Yellow:        return Color::FromIndexed(3);
    case Color::Basic::Blue:          return Color::FromIndexed(4);
    case Color::Basic::Magenta:       return Color::FromIndexed(5);
    case Color::Basic::Cyan:          return Color::FromIndexed(6);
    case Color::Basic::White:         return Color::FromIndexed(7);

    case Color::Basic::BrightBlack:   return Color::FromIndexed(8);
    case Color::Basic::BrightRed:     return Color::FromIndexed(9);
    case Color::Basic::BrightGreen:   return Color::FromIndexed(10);
    case Color::Basic::BrightYellow:  return Color::FromIndexed(11);
    case Color::Basic::BrightBlue:    return Color::FromIndexed(12);
    case Color::Basic::BrightMagenta: return Color::FromIndexed(13);
    case Color::Basic::BrightCyan:    return Color::FromIndexed(14);
    case Color::Basic::BrightWhite:   return Color::FromIndexed(15);

    default:
        return Color::FromIndexed(7);
    }
}

ColorMapping::RgbValue ColorMapping::toRgb(const Color& color)
{
    if (color.isRgb())
    {
        return { color.red(), color.green(), color.blue() };
    }

    if (color.isBasic16())
    {
        return basicToRgb(color.basic());
    }

    if (color.isIndexed256())
    {
        return indexedToRgb(color.index256());
    }

    return { 0, 0, 0 };
}

ColorMapping::RgbValue ColorMapping::basicToRgb(Color::Basic color)
{
    switch (color)
    {
    case Color::Basic::Black:         return { 0, 0, 0 };
    case Color::Basic::Red:           return { 128, 0, 0 };
    case Color::Basic::Green:         return { 0, 128, 0 };
    case Color::Basic::Yellow:        return { 128, 128, 0 };
    case Color::Basic::Blue:          return { 0, 0, 128 };
    case Color::Basic::Magenta:       return { 128, 0, 128 };
    case Color::Basic::Cyan:          return { 0, 128, 128 };
    case Color::Basic::White:         return { 192, 192, 192 };

    case Color::Basic::BrightBlack:   return { 128, 128, 128 };
    case Color::Basic::BrightRed:     return { 255, 0, 0 };
    case Color::Basic::BrightGreen:   return { 0, 255, 0 };
    case Color::Basic::BrightYellow:  return { 255, 255, 0 };
    case Color::Basic::BrightBlue:    return { 0, 0, 255 };
    case Color::Basic::BrightMagenta: return { 255, 0, 255 };
    case Color::Basic::BrightCyan:    return { 0, 255, 255 };
    case Color::Basic::BrightWhite:   return { 255, 255, 255 };

    default:
        return { 192, 192, 192 };
    }
}

ColorMapping::RgbValue ColorMapping::indexedToRgb(std::uint8_t index)
{
    if (index < 16)
    {
        switch (index)
        {
        case 0:  return basicToRgb(Color::Basic::Black);
        case 1:  return basicToRgb(Color::Basic::Red);
        case 2:  return basicToRgb(Color::Basic::Green);
        case 3:  return basicToRgb(Color::Basic::Yellow);
        case 4:  return basicToRgb(Color::Basic::Blue);
        case 5:  return basicToRgb(Color::Basic::Magenta);
        case 6:  return basicToRgb(Color::Basic::Cyan);
        case 7:  return basicToRgb(Color::Basic::White);
        case 8:  return basicToRgb(Color::Basic::BrightBlack);
        case 9:  return basicToRgb(Color::Basic::BrightRed);
        case 10: return basicToRgb(Color::Basic::BrightGreen);
        case 11: return basicToRgb(Color::Basic::BrightYellow);
        case 12: return basicToRgb(Color::Basic::BrightBlue);
        case 13: return basicToRgb(Color::Basic::BrightMagenta);
        case 14: return basicToRgb(Color::Basic::BrightCyan);
        case 15: return basicToRgb(Color::Basic::BrightWhite);
        default: break;
        }
    }

    if (index >= 16 && index <= 231)
    {
        const int adjusted = static_cast<int>(index) - 16;
        const int r = adjusted / 36;
        const int g = (adjusted / 6) % 6;
        const int b = adjusted % 6;

        return {
            cubeValue(r),
            cubeValue(g),
            cubeValue(b)
        };
    }

    if (index >= 232)
    {
        const std::uint8_t gray =
            static_cast<std::uint8_t>(8 + (index - 232) * 10);

        return { gray, gray, gray };
    }

    return { 0, 0, 0 };
}

Color::Basic ColorMapping::rgbToNearestBasic(
    std::uint8_t red,
    std::uint8_t green,
    std::uint8_t blue)
{
    struct Candidate
    {
        Color::Basic color;
        RgbValue rgb;
    };

    static const Candidate candidates[] =
    {
        { Color::Basic::Black,         { 0, 0, 0 } },
        { Color::Basic::Red,           { 128, 0, 0 } },
        { Color::Basic::Green,         { 0, 128, 0 } },
        { Color::Basic::Yellow,        { 128, 128, 0 } },
        { Color::Basic::Blue,          { 0, 0, 128 } },
        { Color::Basic::Magenta,       { 128, 0, 128 } },
        { Color::Basic::Cyan,          { 0, 128, 128 } },
        { Color::Basic::White,         { 192, 192, 192 } },
        { Color::Basic::BrightBlack,   { 128, 128, 128 } },
        { Color::Basic::BrightRed,     { 255, 0, 0 } },
        { Color::Basic::BrightGreen,   { 0, 255, 0 } },
        { Color::Basic::BrightYellow,  { 255, 255, 0 } },
        { Color::Basic::BrightBlue,    { 0, 0, 255 } },
        { Color::Basic::BrightMagenta, { 255, 0, 255 } },
        { Color::Basic::BrightCyan,    { 0, 255, 255 } },
        { Color::Basic::BrightWhite,   { 255, 255, 255 } }
    };

    Color::Basic bestColor = Color::Basic::White;
    int bestDistance = -1;

    for (const Candidate& candidate : candidates)
    {
        const int distance = squareDistance(
            red, green, blue,
            candidate.rgb.red, candidate.rgb.green, candidate.rgb.blue);

        if (bestDistance < 0 || distance < bestDistance)
        {
            bestDistance = distance;
            bestColor = candidate.color;
        }
    }

    return bestColor;
}

std::uint8_t ColorMapping::rgbToNearestIndexed256(
    std::uint8_t red,
    std::uint8_t green,
    std::uint8_t blue)
{
    /*
        Preserve the current project’s practical xterm-like mapping strategy:
        compare the nearest cube color and nearest grayscale entry, then choose
        whichever is closer.
    */

    int bestCubeDistance = -1;
    std::uint8_t bestCubeIndex = 16;

    for (int r = 0; r < 6; ++r)
    {
        for (int g = 0; g < 6; ++g)
        {
            for (int b = 0; b < 6; ++b)
            {
                const std::uint8_t rr = cubeValue(r);
                const std::uint8_t gg = cubeValue(g);
                const std::uint8_t bb = cubeValue(b);

                const int distance = squareDistance(
                    red, green, blue,
                    rr, gg, bb);

                if (bestCubeDistance < 0 || distance < bestCubeDistance)
                {
                    bestCubeDistance = distance;
                    bestCubeIndex = static_cast<std::uint8_t>(
                        16 + (36 * r) + (6 * g) + b);
                }
            }
        }
    }

    int bestGrayDistance = -1;
    std::uint8_t bestGrayIndex = 232;

    for (int i = 0; i < 24; ++i)
    {
        const std::uint8_t gray = static_cast<std::uint8_t>(8 + i * 10);

        const int distance = squareDistance(
            red, green, blue,
            gray, gray, gray);

        if (bestGrayDistance < 0 || distance < bestGrayDistance)
        {
            bestGrayDistance = distance;
            bestGrayIndex = static_cast<std::uint8_t>(232 + i);
        }
    }

    if (bestGrayDistance < bestCubeDistance)
    {
        return bestGrayIndex;
    }

    return bestCubeIndex;
}

std::uint8_t ColorMapping::cubeValue(int component)
{
    static const std::uint8_t values[6] = { 0, 95, 135, 175, 215, 255 };
    return values[component];
}