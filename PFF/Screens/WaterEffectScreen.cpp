#include "Screens/WaterEffectScreen.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Core/Rect.h"
#include "Rendering/Surface.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/BannerThemes.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/Styles/StyleBuilder.h"

namespace
{
    constexpr int QuietThreshold = 2;
    constexpr int StrongLetterThreshold = 26;
    constexpr int MaxVisualAmplitude = 64;
    constexpr float Pi = 3.1415926535f;

    // Approximate "5-10 frames later" using a 60 FPS-style timing window.
    constexpr float SecondaryDropDelaySeconds = 45.0f / 60.0f;
    constexpr float SecondaryDropStrengthScale = 0.5f;

    constexpr char32_t toUpperAscii(char32_t c)
    {
        if (c >= U'a' && c <= U'z')
        {
            return c - (U'a' - U'A');
        }

        return c;
    }

    constexpr char32_t toLowerAscii(char32_t c)
    {
        if (c >= U'A' && c <= U'Z')
        {
            return c + (U'a' - U'A');
        }

        return c;
    }

    bool isAsciiLetter(char32_t c)
    {
        return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z');
    }

    float randomRange(float minValue, float maxValue)
    {
        const float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
        return minValue + ((maxValue - minValue) * t);
    }

    double randomRange(double minValue, double maxValue)
    {
        const double t = static_cast<double>(std::rand()) / static_cast<double>(RAND_MAX);
        return minValue + ((maxValue - minValue) * t);
    }
}

WaterEffectScreen::WaterEffectScreen()
{
    m_modeColor[0] = style::Bold + style::Fg(Color::FromBasic(Color::Basic::Green)) + style::Bg(Color::FromBasic(Color::Basic::Black));
    m_modeColor[1] = style::Bold + style::Fg(Color::FromBasic(Color::Basic::Blue)) + style::Bg(Color::FromBasic(Color::Basic::Black));
    m_modeColor[2] = style::SlowBlink + style::Fg(Color::FromBasic(Color::Basic::Red)) + style::Bg(Color::FromBasic(Color::Basic::Black));
}

void WaterEffectScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
    m_droplets.clear();

    m_rainMode = RainMode::Sprinkle;
    m_nextDropTime = randomRange(0.8, 2.0);
    m_nextRainModeChangeTime = randomRange(6.0, 12.0);
}

void WaterEffectScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;

    if (m_waveWidth <= 2 || m_waveHeight <= 2)
    {
        return;
    }

    updateRainTiming();
    updateDroplets(deltaTime);
}

void WaterEffectScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < 30 || screenHeight < 12)
    {
        buffer.writeString(1, 1, "WaterEffect needs a larger console window.", Themes::Warning);
        return;
    }

    m_waveLeft = 1;
    m_waveTop = 1;
    m_waveWidth = std::max(0, screenWidth - 2);
    m_waveHeight = std::max(0, screenHeight - 2);

    ensureSimulationSize(m_waveWidth, m_waveHeight);
    renderWaveField(surface);

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        style::Fg(Color::FromBasic(Color::Basic::White)) + style::Bg(Color::FromBasic(Color::Basic::Black)));

    buffer.writeString(4, 0, "[ Rain Drops Water Effect ]", Themes::Subtitle);

    int startModeXpos = 4;

    buffer.writeString(startModeXpos, screenHeight - 1, "[ Mode:", Themes::Subtitle);
    startModeXpos += 7;

    switch (m_rainMode)
    {
    case RainMode::Sprinkle:
        buffer.writeString(startModeXpos, screenHeight - 1, " Sprinkle ", m_modeColor[int(RainMode::Sprinkle)]);
        startModeXpos += 10;
        break;

    case RainMode::LightRain:
        buffer.writeString(startModeXpos, screenHeight - 1, " Light Rain ", m_modeColor[int(RainMode::LightRain)]);
        startModeXpos += 12;
        break;

    case RainMode::Pouring:
        buffer.writeString(startModeXpos, screenHeight - 1, " Pouring ", m_modeColor[int(RainMode::Pouring)]);
        startModeXpos += 9;
        break;
    }

    buffer.writeChar(startModeXpos, screenHeight - 1, U']', Themes::Subtitle);
}

void WaterEffectScreen::ensureSimulationSize(int width, int height)
{
    if (width == m_waveWidth &&
        height == m_waveHeight &&
        static_cast<int>(m_textMask.size()) == width * height)
    {
        return;
    }

    m_waveWidth = width;
    m_waveHeight = height;

    const std::size_t cellCount = static_cast<std::size_t>(m_waveWidth * m_waveHeight);

    m_textMask.assign(cellCount, U' ');
    rebuildTextMask();

    m_droplets.clear();

    m_rainMode = RainMode::Sprinkle;
    m_nextDropTime = m_elapsedSeconds + randomRange(0.8, 2.0);
    m_nextRainModeChangeTime = m_elapsedSeconds + randomRange(6.0, 12.0);
}

void WaterEffectScreen::rebuildTextMask()
{
    if (m_waveWidth <= 0 || m_waveHeight <= 0)
    {
        return;
    }

    std::fill(m_textMask.begin(), m_textMask.end(), U' ');

    const int centerY = m_waveHeight / 2;

    const int primaryX = std::max(0, (m_waveWidth - static_cast<int>(m_primaryText.size())) / 2);
    const int primaryY = std::clamp(centerY - 1, 0, std::max(0, m_waveHeight - 1));

    for (std::size_t i = 0; i < m_primaryText.size(); ++i)
    {
        const int x = primaryX + static_cast<int>(i);
        const int y = primaryY;

        if (x >= 0 && x < m_waveWidth && y >= 0 && y < m_waveHeight)
        {
            m_textMask[static_cast<std::size_t>(index(x, y))] = m_primaryText[i];
        }
    }

    const int secondaryX = std::max(0, (m_waveWidth - static_cast<int>(m_secondaryText.size())) / 2);
    const int secondaryY = std::clamp(centerY + 1, 0, std::max(0, m_waveHeight - 1));

    for (std::size_t i = 0; i < m_secondaryText.size(); ++i)
    {
        const int x = secondaryX + static_cast<int>(i);
        const int y = secondaryY;

        if (x >= 0 && x < m_waveWidth && y >= 0 && y < m_waveHeight)
        {
            m_textMask[static_cast<std::size_t>(index(x, y))] = m_secondaryText[i];
        }
    }
}

void WaterEffectScreen::spawnRandomDroplet()
{
    if (m_waveWidth <= 4 || m_waveHeight <= 4)
    {
        return;
    }

    Droplet droplet;
    droplet.x = static_cast<float>(1 + (std::rand() % (m_waveWidth - 2)));
    droplet.y = static_cast<float>(1 + (std::rand() % (m_waveHeight - 2)));
    droplet.age = 0.0f;
    droplet.lifetime = randomRange(1.8f, 2.8f);
    droplet.speed = randomRange(6.0f, 10.0f);
    droplet.wavelength = randomRange(3.0f, 5.0f);
    droplet.strength = randomRange(28.0f, 52.0f);

    m_droplets.push_back(droplet);

    Droplet secondaryDroplet = droplet;
    secondaryDroplet.age = -SecondaryDropDelaySeconds;
    secondaryDroplet.strength *= SecondaryDropStrengthScale;

    m_droplets.push_back(secondaryDroplet);
}

void WaterEffectScreen::updateDroplets(double deltaTime)
{
    const float dt = static_cast<float>(deltaTime);

    for (Droplet& droplet : m_droplets)
    {
        droplet.age += dt;
    }

    m_droplets.erase(
        std::remove_if(
            m_droplets.begin(),
            m_droplets.end(),
            [](const Droplet& droplet)
            {
                return droplet.age >= droplet.lifetime;
            }),
        m_droplets.end());
}

void WaterEffectScreen::updateRainTiming()
{
    if (m_elapsedSeconds >= m_nextRainModeChangeTime)
    {
        chooseNextRainMode();
    }

    while (m_elapsedSeconds >= m_nextDropTime)
    {
        spawnRandomDroplet();

        double interval = 3.5;

        switch (m_rainMode)
        {
        case RainMode::Sprinkle:
            interval = randomRange(1.00, 1.50);
            break;

        case RainMode::LightRain:
            interval = randomRange(0.50, 0.75);
            break;

        case RainMode::Pouring:
            interval = randomRange(0.05, 0.25);
            break;
        }

        m_nextDropTime += interval;
    }
}

void WaterEffectScreen::chooseNextRainMode()
{
    const int roll = std::rand() % 100;

    if (roll < 50)
    {
        m_rainMode = RainMode::Sprinkle;
        m_nextRainModeChangeTime = m_elapsedSeconds + randomRange(6.0, 12.0);
    }
    else if (roll < 85)
    {
        m_rainMode = RainMode::LightRain;
        m_nextRainModeChangeTime = m_elapsedSeconds + randomRange(6.0, 10.0);
    }
    else
    {
        m_rainMode = RainMode::Pouring;
        m_nextRainModeChangeTime = m_elapsedSeconds + randomRange(4.0, 8.0);
    }
}

void WaterEffectScreen::renderWaveField(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    for (int y = 0; y < m_waveHeight; ++y)
    {
        for (int x = 0; x < m_waveWidth; ++x)
        {
            const int amplitude = computeAmplitudeAtCell(x, y);
            const char32_t sourceGlyph = m_textMask[static_cast<std::size_t>(index(x, y))];

            const char32_t waveGlyph = selectWaveGlyph(amplitude, sourceGlyph);
            const Style waveStyle = selectWaveStyle(amplitude, sourceGlyph);

            buffer.writeCodePoint(m_waveLeft + x, m_waveTop + y, waveGlyph, waveStyle);
        }
    }
}

int WaterEffectScreen::index(int x, int y) const
{
    return (y * m_waveWidth) + x;
}

int WaterEffectScreen::computeAmplitudeAtCell(int x, int y) const
{
    float total = 0.0f;

    for (const Droplet& droplet : m_droplets)
    {
        if (droplet.age < 0.0f)
        {
            continue;
        }

        const float dx = static_cast<float>(x) - droplet.x;
        const float dy = static_cast<float>(y) - droplet.y;
        const float distance = std::sqrt((dx * dx) + (dy * dy));

        const float radius = droplet.age * droplet.speed;
        const float ringDistance = std::fabs(distance - radius);

        const float thickness = droplet.wavelength * 0.75f;
        if (ringDistance > thickness)
        {
            continue;
        }

        const float ageT = (droplet.lifetime > 0.0f) ? (droplet.age / droplet.lifetime) : 1.0f;
        const float fade = std::max(0.0f, 1.0f - ageT);

        const float normalizedRing = 1.0f - (ringDistance / thickness);
        const float ringProfile = normalizedRing * normalizedRing;

        const float phase = ((distance - radius) / droplet.wavelength) * Pi;
        const float oscillation = std::cos(phase);

        total += droplet.strength * fade * ringProfile * oscillation;
    }

    total = std::clamp(total, -static_cast<float>(MaxVisualAmplitude), static_cast<float>(MaxVisualAmplitude));
    return static_cast<int>(std::round(total));
}

char32_t WaterEffectScreen::selectWaveGlyph(int amplitude, char32_t sourceGlyph) const
{
    const int magnitude = std::min(std::abs(amplitude), MaxVisualAmplitude);

    if (magnitude <= QuietThreshold)
    {
        return U' ';
    }

    if (sourceGlyph != U' ' && isAsciiLetter(sourceGlyph) && magnitude >= StrongLetterThreshold)
    {
        return (amplitude >= 0) ? toUpperAscii(sourceGlyph) : toLowerAscii(sourceGlyph);
    }

    static const std::u32string ramp = U" .,:;abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

    const int rampIndex =
        std::clamp(
            (magnitude * static_cast<int>(ramp.size() - 1)) / MaxVisualAmplitude,
            0,
            static_cast<int>(ramp.size() - 1));

    return ramp[static_cast<std::size_t>(rampIndex)];
}

Style WaterEffectScreen::selectWaveStyle(int amplitude, char32_t sourceGlyph) const
{
    const int magnitude = std::abs(amplitude);

    if (sourceGlyph != U' ' && magnitude >= StrongLetterThreshold)
    {
        return style::Bold
            + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
            + style::Bg(Color::FromBasic(Color::Basic::Blue));
    }

    if (magnitude >= 42)
    {
        return style::Bold
            + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
            + style::Bg(Color::FromBasic(Color::Basic::Blue));
    }

    if (magnitude >= 24)
    {
        return style::Fg(Color::FromBasic(Color::Basic::BrightCyan))
            + style::Bg(Color::FromBasic(Color::Basic::Blue));
    }

    if (magnitude >= 10)
    {
        return style::Fg(Color::FromBasic(Color::Basic::Cyan))
            + style::Bg(Color::FromBasic(Color::Basic::Blue));
    }

    return style::Fg(Color::FromBasic(Color::Basic::BrightBlue))
        + style::Bg(Color::FromBasic(Color::Basic::Blue));
}