#include "Rendering/Effects/Donut3D.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Animation/TickEvent.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Surface.h"

namespace
{
    constexpr float Pi = 3.1415926535f;

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

    float wrapAngle(float angle)
    {
        const float fullTurn = Pi * 2.0f;

        if (angle > fullTurn || angle < -fullTurn)
        {
            angle = std::fmod(angle, fullTurn);
        }

        if (angle < 0.0f)
        {
            angle += fullTurn;
        }

        return angle;
    }

    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    Vec3 rotateX(const Vec3& point, float cosX, float sinX)
    {
        return Vec3{
            point.x,
            (point.y * cosX) - (point.z * sinX),
            (point.y * sinX) + (point.z * cosX)
        };
    }

    Vec3 rotateY(const Vec3& point, float cosY, float sinY)
    {
        return Vec3{
            (point.x * cosY) + (point.z * sinY),
            point.y,
            (-point.x * sinY) + (point.z * cosY)
        };
    }

    Vec3 rotateZ(const Vec3& point, float cosZ, float sinZ)
    {
        return Vec3{
            (point.x * cosZ) - (point.y * sinZ),
            (point.x * sinZ) + (point.y * cosZ),
            point.z
        };
    }

    Vec3 transformPoint(
        const Vec3& point,
        float cosX,
        float sinX,
        float cosY,
        float sinY,
        float cosZ,
        float sinZ)
    {
        return rotateZ(
            rotateY(
                rotateX(point, cosX, sinX),
                cosY,
                sinY),
            cosZ,
            sinZ);
    }

    float toRadians(float degrees)
    {
        return degrees * (Pi / 180.0f);
    }

    float toDegrees(float radians)
    {
        return radians * (180.0f / Pi);
    }
}

Donut3D::Donut3D()
{
}

void Donut3D::onEnter()
{
    m_elapsedSeconds   = 0.0;
    m_rotationXRadians = 0.0f;
    m_rotationYRadians = 0.0f;
    m_rotationZRadians = 0.0f;
}

void Donut3D::onExit()
{
}

void Donut3D::update(const Animation::TickEvent& event)
{
    m_elapsedSeconds += event.deltaSeconds;

    m_rotationXRadians = wrapAngle(m_rotationXRadians + (static_cast<float>(event.deltaSeconds) * m_rotationXSpeed));
    m_rotationYRadians = wrapAngle(m_rotationYRadians + (static_cast<float>(event.deltaSeconds) * m_rotationYSpeed));
    m_rotationZRadians = wrapAngle(m_rotationZRadians + (static_cast<float>(event.deltaSeconds) * m_rotationZSpeed));
}

void Donut3D::draw(Surface& surface, const Rect& viewPort)
{
    renderDonut(surface, viewPort);
}

void Donut3D::setOptions(const DonutEffectOptions& options)
{
    m_luminanceRamp      = options.luminanceRamp;
    m_luminanceRampCount = static_cast<int>(m_luminanceRamp.size());
    m_elapsedSeconds     = options.elapsedSeconds;
    m_rotationXRadians   = toRadians(options.rotationXDegrees);
    m_rotationYRadians   = toRadians(options.rotationYDegrees);
    m_rotationZRadians   = toRadians(options.rotationZDegrees);
    m_rotationXSpeed     = options.rotationXSpeed;
    m_rotationYSpeed     = options.rotationYSpeed;
    m_rotationZSpeed     = options.rotationZSpeed;
    m_scale              = options.scale;
    m_thetaSpacing       = options.thetaSpacing;
    m_phiSpacing         = options.phiSpacing;
    m_tubeRadius         = options.tubeRadius;
    m_torusRadius        = options.torusRadius;
    m_cameraDistance     = options.cameraDistance;
    m_showBands          = options.showBands;
}

DonutEffectOptions Donut3D::getOptions() const
{
    DonutEffectOptions options;

    options.luminanceRamp    = m_luminanceRamp;
    options.elapsedSeconds   = m_elapsedSeconds;
    options.rotationXDegrees = toDegrees(m_rotationXRadians);
    options.rotationYDegrees = toDegrees(m_rotationYRadians);
    options.rotationZDegrees = toDegrees(m_rotationZRadians);
    options.rotationXSpeed   = m_rotationXSpeed;
    options.rotationYSpeed   = m_rotationYSpeed;
    options.rotationZSpeed   = m_rotationZSpeed;
    options.scale            = m_scale;
    options.thetaSpacing     = m_thetaSpacing;
    options.phiSpacing       = m_phiSpacing;
    options.tubeRadius       = m_tubeRadius;
    options.torusRadius      = m_torusRadius;
    options.cameraDistance   = m_cameraDistance;
    options.showBands        = m_showBands;

    return options;
}

void Donut3D::ensureBuffers(int width, int height)
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

void Donut3D::renderDonut(Surface& surface, const Rect& viewPort)
{
    if (viewPort.size.width <= 0 || viewPort.size.height <= 0)
    {
        return;
    }

    if (m_luminanceRamp.empty())
    {
        return;
    }

    ScreenBuffer& buffer = surface.buffer();

    ensureBuffers(viewPort.size.width, viewPort.size.height);

    buffer.fillRect(
        viewPort,
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    std::vector<float> luminanceBuffer(static_cast<std::size_t>(viewPort.size.width * viewPort.size.height), 0.0f);
    std::vector<float> shadowBuffer(static_cast<std::size_t>(viewPort.size.width * viewPort.size.height), 0.0f);

    const float cosX = std::cos(m_rotationXRadians);
    const float sinX = std::sin(m_rotationXRadians);
    const float cosY = std::cos(m_rotationYRadians);
    const float sinY = std::sin(m_rotationYRadians);
    const float cosZ = std::cos(m_rotationZRadians);
    const float sinZ = std::sin(m_rotationZRadians);

    const float k1 = m_scale * static_cast<float>(viewPort.size.width) * m_cameraDistance * 3.0f / (8.0f * (m_tubeRadius + m_torusRadius));

    float minDepth = 1000000.0f;
    float maxDepth = -1000000.0f;

    const float floorY = (static_cast<float>(viewPort.size.height) * 0.9f);
    const float shadowShiftX = 6.0f;

    for (float theta = 0.0f; theta < (Pi * 2.0f); theta += m_thetaSpacing)
    {
        const float costheta = std::cos(theta);
        const float sintheta = std::sin(theta);

        for (float phi = 0.0f; phi < (Pi * 2.0f); phi += m_phiSpacing)
        {
            // define the taurus
            const float cosphi = std::cos(phi);
            const float sinphi = std::sin(phi);

            const float circlex = m_torusRadius + (m_tubeRadius * costheta);
            const float circley = m_tubeRadius * sintheta;

            const Vec3 localPoint{
                circlex * cosphi,
                circley,
                circlex * sinphi
            };

            const Vec3 transformedPoint = transformPoint(
                localPoint,
                cosX,
                sinX,
                cosY,
                sinY,
                cosZ,
                sinZ);

            const float x = transformedPoint.x;
            const float y = transformedPoint.y;
            const float z = m_cameraDistance + transformedPoint.z;

            if (z <= 0.0f)
            {
                continue;
            }

            const float ooz = 1.0f / z;

            const int xp = static_cast<int>((static_cast<float>(viewPort.size.width) * 0.5f) + (k1 * ooz * x));
            const int yp = static_cast<int>((static_cast<float>(viewPort.size.height) * 0.5f) - (k1 * ooz * y * 0.5f));

            if (xp < 0 || xp >= viewPort.size.width || yp < 0 || yp >= viewPort.size.height)
            {
                continue;
            }

            const Vec3 localNormal{
                costheta * cosphi,
                sintheta,
                costheta * sinphi
            };

            const Vec3 transformedNormal = transformPoint(
                localNormal,
                cosX,
                sinX,
                cosY,
                sinY,
                cosZ,
                sinZ);

            const float luminance =
                transformedNormal.y
                - transformedNormal.z;

            if (luminance <= 0.0f)
            {
                continue;
            }

            const int bufferIndex = index(xp, yp, viewPort.size.width);
            const std::size_t cellIndex = static_cast<std::size_t>(bufferIndex);

            if (ooz <= m_depthBuffer[cellIndex])
            {
                continue;
            }

            m_depthBuffer[cellIndex] = ooz;
            luminanceBuffer[cellIndex] = luminance;

            if (m_showBands)
            {
                // cut 4 bands equal distances apart
                const float bandSpacing = Pi * 0.5f;
                const float seamWidth = 0.04f;

                const float bandPhase =
                    std::fmod(phi, bandSpacing);

                const bool isBand = bandPhase < seamWidth
                    || bandPhase >(bandSpacing - seamWidth);

                if (isBand)
                {
                    m_glyphBuffer[cellIndex] = U' ';
                }
                else
                {
                    int luminanceIndex = static_cast<int>(luminance * 8.0f);
                    luminanceIndex = std::clamp(luminanceIndex, 0, m_luminanceRampCount - 1);
                    m_glyphBuffer[cellIndex] = m_luminanceRamp[static_cast<std::size_t>(luminanceIndex)];
                }
            }
            else
            {
                int luminanceIndex = static_cast<int>(luminance * 8.0f);
                luminanceIndex = std::clamp(luminanceIndex, 0, m_luminanceRampCount - 1);
                m_glyphBuffer[cellIndex] = m_luminanceRamp[static_cast<std::size_t>(luminanceIndex)];
            }

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
            const int shadowY = static_cast<int>(std::round(floorY + ((static_cast<float>(yp) - (static_cast<float>(viewPort.size.height) * 0.5f)) * 0.18f)));

            if (shadowX >= 0 && shadowX < viewPort.size.width && shadowY >= 0 && shadowY < viewPort.size.height)
            {
                const std::size_t shadowIndex = static_cast<std::size_t>(index(shadowX, shadowY, viewPort.size.width));
                shadowBuffer[shadowIndex] = std::max(shadowBuffer[shadowIndex], 0.35f + (luminance * 0.45f));
            }

            const int softShadowX1 = shadowX - 1;
            const int softShadowX2 = shadowX + 1;

            if (softShadowX1 >= 0 && softShadowX1 < viewPort.size.width && shadowY >= 0 && shadowY < viewPort.size.height)
            {
                const std::size_t shadowIndex = static_cast<std::size_t>(index(softShadowX1, shadowY, viewPort.size.width));
                shadowBuffer[shadowIndex] = std::max(shadowBuffer[shadowIndex], 0.18f + (luminance * 0.18f));
            }

            if (softShadowX2 >= 0 && softShadowX2 < viewPort.size.width && shadowY >= 0 && shadowY < viewPort.size.height)
            {
                const std::size_t shadowIndex = static_cast<std::size_t>(index(softShadowX2, shadowY, viewPort.size.width));
                shadowBuffer[shadowIndex] = std::max(shadowBuffer[shadowIndex], 0.18f + (luminance * 0.18f));
            }
        }
    }

    if (maxDepth <= minDepth)
    {
        maxDepth = minDepth + 0.0001f;
    }

    for (int y = 0; y < viewPort.size.height; ++y)
    {
        for (int x = 0; x < viewPort.size.width; ++x)
        {
            const std::size_t cellIndex = static_cast<std::size_t>(index(x, y, viewPort.size.width));
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

            buffer.writeCodePoint(viewPort.position.x + x, viewPort.position.y + y, shadowGlyph, shadowStyle);
        }
    }

    for (int y = 0; y < viewPort.size.height; ++y)
    {
        for (int x = 0; x < viewPort.size.width; ++x)
        {
            const int bufferIndex = index(x, y, viewPort.size.width);
            const std::size_t cellIndex = static_cast<std::size_t>(bufferIndex);

            const char32_t glyph = m_glyphBuffer[cellIndex];
            const float depth = m_depthBuffer[cellIndex];
            const float luminance = luminanceBuffer[cellIndex];

            if (glyph == U' ')
            {
                continue;
            }

            const float normalizedDepth = (depth - minDepth) / (maxDepth - minDepth);
            buffer.writeCodePoint(viewPort.position.x + x, viewPort.position.y + y, glyph, buildShadedStyle(normalizedDepth, luminance));
        }
    }
}

int Donut3D::index(int x, int y, int width) const
{
    return (y * width) + x;
}

Style Donut3D::buildShadedStyle(float normalizedDepth, float luminance) const
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