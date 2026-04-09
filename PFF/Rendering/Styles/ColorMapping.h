#pragma once

#include <cstdint>

#include "Rendering/Capabilities/ColorSupport.h"
#include "Rendering/Styles/Color.h"

class ColorMapping
{
public:
    struct RgbValue
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
    };

public:
    static Color mapToSupport(const Color& color, ColorSupport support);

    static Color rgbToIndexed(const Color& color);
    static Color indexedToBasic(const Color& color);
    static Color rgbToBasic(const Color& color);

    static Color basicToIndexed(const Color& color);

    static RgbValue toRgb(const Color& color);

private:
    static RgbValue basicToRgb(Color::Basic color);
    static RgbValue indexedToRgb(std::uint8_t index);

    static Color::Basic rgbToNearestBasic(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue);

    static std::uint8_t rgbToNearestIndexed256(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue);

    static std::uint8_t cubeValue(int component);
};