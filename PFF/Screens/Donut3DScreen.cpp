// Screens/Donut3DScreen.cpp
#include "Screens/Donut3DScreen.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Core/Rect.h"
#include "Rendering/Surface.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"

namespace
{
    constexpr float Pi = 3.1415926535f;
    constexpr float ThetaSpacing = 0.07f;
    constexpr float PhiSpacing = 0.02f;

    constexpr float R1 = 1.0f;
    constexpr float R2 = 2.0f;
    constexpr float K2 = 5.0f;

    constexpr float RotationASpeed = 1.15f;
    constexpr float RotationBSpeed = 0.45f;

    constexpr float SizeScale = 0.25f;

    constexpr char32_t LuminanceRamp[] = U".,-~:;=!*#$@";
    constexpr int LuminanceRampCount = static_cast<int>((sizeof(LuminanceRamp) / sizeof(LuminanceRamp[0])) - 1);

    constexpr Color::Basic CyclePalette[] =
    {
        Color::Basic::Blue,
        Color::Basic::Cyan,
        Color::Basic::Magenta,
        Color::Basic::Red,
        Color::Basic::Yellow,
        Color::Basic::Green
    };

    constexpr Color::Basic CyclePaletteBright[] =
    {
        Color::Basic::BrightBlue,
        Color::Basic::BrightCyan,
        Color::Basic::BrightMagenta,
        Color::Basic::BrightRed,
        Color::Basic::BrightYellow,
        Color::Basic::BrightGreen
    };

    constexpr int CyclePaletteCount = static_cast<int>(sizeof(CyclePalette) / sizeof(CyclePalette[0]));
}

Donut3DScreen::Donut3DScreen() = default;

void Donut3DScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
    m_rotationA = 0.0f;
    m_rotationB = 0.0f;
}

void Donut3DScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;

    m_rotationA += static_cast<float>(deltaTime) * RotationASpeed;
    m_rotationB += static_cast<float>(deltaTime) * RotationBSpeed;

    if (m_rotationA > (Pi * 2.0f))
    {
        m_rotationA = std::fmod(m_rotationA, Pi * 2.0f);
    }

    if (m_rotationB > (Pi * 2.0f))
    {
        m_rotationB = std::fmod(m_rotationB, Pi * 2.0f);
    }
}

void Donut3DScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < 30 || screenHeight < 12)
    {
        buffer.writeString(1, 1, "3D Donut needs a larger console window.", Themes::Warning);
        return;
    }

    const int viewportLeft = 1;
    const int viewportTop = 1;
    const int viewportWidth = std::max(0, screenWidth - 2);
    const int viewportHeight = std::max(0, screenHeight - 2);

    renderDonut(surface, viewportLeft, viewportTop, viewportWidth, viewportHeight);

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        Themes::Background);

    buffer.writeString(4, 0, "[ 3D ASCII Donut ]", Themes::Subtitle);
    buffer.writeString(4, screenHeight - 1, "[ Hue Cycle + Depth Shading + Floor Shadow ]", Themes::Subtitle);
}

void Donut3DScreen::ensureBuffers(int width, int height)
{
    const std::size_t cellCount = static_cast<std::size_t>(std::max(0, width) * std::max(0, height));

    if (m_depthBuffer.size() != cellCount)
    {
        m_depthBuffer.assign(cellCount, 0.0f);
    }
    else
    {
        std::fill(m_depthBuffer.begin(), m_depthBuffer.end(), 0.0f);
    }

    if (m_glyphBuffer.size() != cellCount)
    {
        m_glyphBuffer.assign(cellCount, U' ');
    }
    else
    {
        std::fill(m_glyphBuffer.begin(), m_glyphBuffer.end(), U' ');
    }
}

void Donut3DScreen::renderDonut(Surface& surface, int left, int top, int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    ScreenBuffer& buffer = surface.buffer();

    ensureBuffers(width, height);

    buffer.fillRect(
        Rect{ Point{ left, top }, Size{ width, height } },
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    std::vector<float> luminanceBuffer(static_cast<std::size_t>(width * height), 0.0f);
    std::vector<float> shadowBuffer(static_cast<std::size_t>(width * height), 0.0f);

    const float cosA = std::cos(m_rotationA);
    const float sinA = std::sin(m_rotationA);
    const float cosB = std::cos(m_rotationB);
    const float sinB = std::sin(m_rotationB);

    const float k1 = SizeScale * static_cast<float>(width) * K2 * 3.0f / (8.0f * (R1 + R2));

    float minDepth = 1000000.0f;
    float maxDepth = -1000000.0f;

    const float floorY = (static_cast<float>(height) * 0.9f);
    const float shadowShiftX = 6.0f;

    for (float theta = 0.0f; theta < (Pi * 2.0f); theta += ThetaSpacing)
    {
        const float costheta = std::cos(theta);
        const float sintheta = std::sin(theta);

        for (float phi = 0.0f; phi < (Pi * 2.0f); phi += PhiSpacing)
        {
            const float cosphi = std::cos(phi);
            const float sinphi = std::sin(phi);

            const float circlex = R2 + (R1 * costheta);
            const float circley = R1 * sintheta;

            const float x = circlex * ((cosB * cosphi) + (sinA * sinB * sinphi))
                - (circley * cosA * sinB);
            const float y = circlex * ((sinB * cosphi) - (sinA * cosB * sinphi))
                + (circley * cosA * cosB);
            const float z = K2 + (cosA * circlex * sinphi) + (circley * sinA);

            if (z <= 0.0f)
            {
                continue;
            }

            const float ooz = 1.0f / z;

            const int xp = static_cast<int>((static_cast<float>(width) * 0.5f) + (k1 * ooz * x));
            const int yp = static_cast<int>((static_cast<float>(height) * 0.5f) - (k1 * ooz * y * 0.5f));

            if (xp < 0 || xp >= width || yp < 0 || yp >= height)
            {
                continue;
            }

            const float luminance =
                (cosphi * costheta * sinB)
                - (cosA * costheta * sinphi)
                - (sinA * sintheta)
                + (cosB * ((cosA * sintheta) - (costheta * sinA * sinphi)));

            if (luminance <= 0.0f)
            {
                continue;
            }

            const int bufferIndex = index(xp, yp, width);
            const std::size_t cellIndex = static_cast<std::size_t>(bufferIndex);

            if (ooz <= m_depthBuffer[cellIndex])
            {
                continue;
            }

            m_depthBuffer[cellIndex] = ooz;
            luminanceBuffer[cellIndex] = luminance;

            int luminanceIndex = static_cast<int>(luminance * 8.0f);
            luminanceIndex = std::clamp(luminanceIndex, 0, LuminanceRampCount - 1);
            m_glyphBuffer[cellIndex] = LuminanceRamp[luminanceIndex];

            if (ooz < minDepth)
            {
                minDepth = ooz;
            }

            if (ooz > maxDepth)
            {
                maxDepth = ooz;
            }

            const float closeness = std::clamp((ooz - 0.10f) / 0.18f, 0.0f, 1.0f);
            const int shadowX = static_cast<int>(std::round(static_cast<float>(xp) + shadowShiftX + (closeness * 2.0f)));
            const int shadowY = static_cast<int>(std::round(floorY + ((static_cast<float>(yp) - (static_cast<float>(height) * 0.5f)) * 0.18f)));

            if (shadowX >= 0 && shadowX < width && shadowY >= 0 && shadowY < height)
            {
                const std::size_t shadowIndex = static_cast<std::size_t>(index(shadowX, shadowY, width));
                shadowBuffer[shadowIndex] = std::max(shadowBuffer[shadowIndex], 0.35f + (luminance * 0.45f));
            }

            const int softShadowX1 = shadowX - 1;
            const int softShadowX2 = shadowX + 1;

            if (softShadowX1 >= 0 && softShadowX1 < width && shadowY >= 0 && shadowY < height)
            {
                const std::size_t shadowIndex = static_cast<std::size_t>(index(softShadowX1, shadowY, width));
                shadowBuffer[shadowIndex] = std::max(shadowBuffer[shadowIndex], 0.18f + (luminance * 0.18f));
            }

            if (softShadowX2 >= 0 && softShadowX2 < width && shadowY >= 0 && shadowY < height)
            {
                const std::size_t shadowIndex = static_cast<std::size_t>(index(softShadowX2, shadowY, width));
                shadowBuffer[shadowIndex] = std::max(shadowBuffer[shadowIndex], 0.18f + (luminance * 0.18f));
            }
        }
    }

    if (maxDepth <= minDepth)
    {
        maxDepth = minDepth + 0.0001f;
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const std::size_t cellIndex = static_cast<std::size_t>(index(x, y, width));
            const float shadowIntensity = shadowBuffer[cellIndex];

            if (shadowIntensity <= 0.0f)
            {
                continue;
            }

            char32_t shadowGlyph = U'.';
            Style shadowStyle = style::Fg(Color::FromBasic(Color::Basic::BrightBlack))
                + style::Bg(Color::FromBasic(Color::Basic::Black));

            if (shadowIntensity > 0.65f)
            {
                shadowGlyph = U':';
                shadowStyle = style::Fg(Color::FromBasic(Color::Basic::Blue))
                    + style::Bg(Color::FromBasic(Color::Basic::Black));
            }
            else if (shadowIntensity > 0.40f)
            {
                shadowGlyph = U'.';
                shadowStyle = style::Fg(Color::FromBasic(Color::Basic::BrightBlack))
                    + style::Bg(Color::FromBasic(Color::Basic::Black));
            }

            buffer.writeCodePoint(left + x, top + y, shadowGlyph, shadowStyle);
        }
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const int bufferIndex = index(x, y, width);
            const std::size_t cellIndex = static_cast<std::size_t>(bufferIndex);

            const char32_t glyph = m_glyphBuffer[cellIndex];
            const float depth = m_depthBuffer[cellIndex];
            const float luminance = luminanceBuffer[cellIndex];

            if (glyph == U' ')
            {
                continue;
            }

            const float normalizedDepth = (depth - minDepth) / (maxDepth - minDepth);
            buffer.writeCodePoint(left + x, top + y, glyph, buildShadedStyle(normalizedDepth, luminance));
        }
    }
}

int Donut3DScreen::index(int x, int y, int width) const
{
    return (y * width) + x;
}

Style Donut3DScreen::buildShadedStyle(float normalizedDepth, float luminance) const
{
    normalizedDepth = std::clamp(normalizedDepth, 0.0f, 1.0f);
    luminance = std::clamp(luminance, 0.0f, 1.0f);

    const int hueOffset = static_cast<int>(m_elapsedSeconds * 0.05) % CyclePaletteCount;

    int depthBand = 0;
    if (normalizedDepth < 0.20f)
    {
        depthBand = 0;
    }
    else if (normalizedDepth < 0.45f)
    {
        depthBand = 1;
    }
    else if (normalizedDepth < 0.75f)
    {
        depthBand = 2;
    }
    else
    {
        depthBand = 3;
    }

    const int paletteIndex = (hueOffset + depthBand) % CyclePaletteCount;

    const Color baseColor = Color::FromBasic(CyclePalette[paletteIndex]);
    const Color brightColor = Color::FromBasic(CyclePaletteBright[paletteIndex]);
    const Color hottestColor = (depthBand >= 3)
        ? Color::FromBasic(Color::Basic::BrightWhite)
        : brightColor;

    if (luminance < 0.25f)
    {
        return style::Fg(baseColor)
            + style::Bg(Color::FromBasic(Color::Basic::Black));
    }

    if (luminance < 0.55f)
    {
        return style::Fg(brightColor)
            + style::Bg(Color::FromBasic(Color::Basic::Black));
    }

    return style::Bold
        + style::Fg(hottestColor)
        + style::Bg(Color::FromBasic(Color::Basic::Black));
}