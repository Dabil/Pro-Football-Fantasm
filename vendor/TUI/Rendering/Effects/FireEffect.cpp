#include "Rendering/Effects/FireEffect.h"

#include <algorithm>
#include <cstdlib>

#include "Rendering/Surface.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Animation/TickEvent.h"

namespace
{
    constexpr int MaxHeat = 65;

    int randomInt(int minValue, int maxValue)
    {
        if (maxValue <= minValue)
        {
            return minValue;
        }

        return minValue + (std::rand() % ((maxValue - minValue) + 1));
    }

    int clampHeat(int value)
    {
        return std::clamp(value, 0, MaxHeat);
    }
}

FireEffect::FireEffect()
{
}

void FireEffect::onEnter()
{
    m_elapsedSeconds = 0.0;
    m_sourceFlickerTimer = 0.0;
    m_fireBuffer.clear();
    m_nextFireBuffer.clear();
}

void FireEffect::onExit()
{
}

void FireEffect::setViewport(const Rect& viewport)
{
    m_fireLeft   = viewport.position.x;
    m_fireTop    = viewport.position.y;
    m_fireWidth  = viewport.size.width;
    m_fireHeight = viewport.size.height;
}

void FireEffect::update(const Animation::TickEvent& event)
{
    m_elapsedSeconds += event.deltaSeconds;
    m_sourceFlickerTimer += event.deltaSeconds;

    if (m_fireWidth <= 0 || m_fireHeight <= 0)
    {
        return;
    }

    updateFire(event);
}

void FireEffect::draw(Surface& surface, const Rect& viewport)
{
    ScreenBuffer& buffer = surface.buffer();

    setViewport(viewport);
    ensureSimulationSize(m_fireWidth, m_fireHeight);
    renderFireEffect(buffer);
}

void FireEffect::renderFireEffect(ScreenBuffer& buffer)
{
    buffer.fillRect(
        Rect{ Point{ m_fireLeft, m_fireTop }, Size{ m_fireWidth, m_fireHeight } },
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    for (int y = 0; y < m_fireHeight; ++y)
    {
        for (int x = 0; x < m_fireWidth; ++x)
        {
            const int linearIndex = index(x, y);
            const int intensity = clampHeat(m_fireBuffer[static_cast<std::size_t>(linearIndex)]);
            const char32_t glyph = glyphForIntensity(intensity);
            const Style style = styleForIntensity(intensity);

            buffer.writeCodePoint(m_fireLeft + x, m_fireTop + y, glyph, style);
        }
    }
}

void FireEffect::ensureSimulationSize(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        m_fireWidth = width;
        m_fireHeight = height;
        m_fireBuffer.clear();
        m_nextFireBuffer.clear();
        return;
    }

    const int paddedSize = (width * height) + width + 1;

    if (width == m_fireWidth &&
        height == m_fireHeight &&
        static_cast<int>(m_fireBuffer.size()) == paddedSize)
    {
        return;
    }

    m_fireWidth = width;
    m_fireHeight = height;

    m_fireBuffer.assign(static_cast<std::size_t>(paddedSize), 0);
    m_nextFireBuffer.clear();

    seedFireSource();
}

void FireEffect::seedFireSource()
{
    if (m_fireWidth <= 0 || m_fireHeight <= 0)
    {
        return;
    }

    std::fill(m_fireBuffer.begin(), m_fireBuffer.end(), 0);

    const int sparksPerTick = std::max(1, m_fireWidth / 9);
    const int bottomRowOffset = m_fireWidth * (m_fireHeight - 1);

    for (int i = 0; i < sparksPerTick; ++i)
    {
        const int x = randomInt(0, m_fireWidth - 1);
        const int linearIndex = bottomRowOffset + x;

        if (linearIndex >= 0 && linearIndex < static_cast<int>(m_fireBuffer.size()))
        {
            m_fireBuffer[static_cast<std::size_t>(linearIndex)] = MaxHeat;
        }
    }
}

void FireEffect::updateFire(const Animation::TickEvent& event)
{
    if (m_fireWidth <= 0 || m_fireHeight <= 0)
    {
        return;
    }

    const int visibleSize = m_fireWidth * m_fireHeight;
    const int sparksPerTick = std::max(1, m_fireWidth / 9);
    const int bottomRowOffset = m_fireWidth * (m_fireHeight - 1);

    if (m_sourceFlickerTimer >= 0.03)
    {
        m_sourceFlickerTimer = 0.0;

        for (int i = 0; i < sparksPerTick; ++i)
        {
            const int x = randomInt(0, m_fireWidth - 1);
            const int linearIndex = bottomRowOffset + x;

            if (linearIndex >= 0 && linearIndex < static_cast<int>(m_fireBuffer.size()))
            {
                m_fireBuffer[static_cast<std::size_t>(linearIndex)] = MaxHeat;
            }
        }
    }

    for (int i = 0; i < visibleSize; ++i)
    {
        const int iRight = i + 1;
        const int iBelow = i + m_fireWidth;
        const int iBelowRight = i + m_fireWidth + 1;

        const int current = m_fireBuffer[static_cast<std::size_t>(i)];
        const int right = (iRight < static_cast<int>(m_fireBuffer.size()))
            ? m_fireBuffer[static_cast<std::size_t>(iRight)]
            : 0;
        const int below = (iBelow < static_cast<int>(m_fireBuffer.size()))
            ? m_fireBuffer[static_cast<std::size_t>(iBelow)]
            : 0;
        const int belowRight = (iBelowRight < static_cast<int>(m_fireBuffer.size()))
            ? m_fireBuffer[static_cast<std::size_t>(iBelowRight)]
            : 0;

        int averaged = (current + right + below + belowRight) / 4;
        averaged = clampHeat(averaged);

        m_fireBuffer[static_cast<std::size_t>(i)] = averaged;
    }

    coolTopRows();
}

void FireEffect::coolTopRows()
{
    const int rowsToCool = std::min(2, m_fireHeight);

    for (int y = 0; y < rowsToCool; ++y)
    {
        for (int x = 0; x < m_fireWidth; ++x)
        {
            const int linearIndex = index(x, y);
            int& heat = m_fireBuffer[static_cast<std::size_t>(linearIndex)];

            if (heat > 0)
            {
                heat = std::max(0, heat - 1);
            }
        }
    }
}

int FireEffect::index(int x, int y) const
{
    return (y * m_fireWidth) + x;
}

char32_t FireEffect::glyphForIntensity(int intensity) const
{
    static constexpr char32_t Glyphs[] =
    {
        U' ',
        U'.',
        U':',
        U'^',
        U'*',
        U'x',
        U's',
        U'S',
        U'#',
        U'$'
    };

    intensity = clampHeat(intensity);

    if (intensity <= 0)
    {
        return Glyphs[0];
    }

    if (intensity > 9)
    {
        return Glyphs[9];
    }

    return Glyphs[intensity];
}

Style FireEffect::styleForIntensity(int intensity) const
{
    const Color black = Color::FromBasic(Color::Basic::Black);

    intensity = clampHeat(intensity);

    if (intensity <= 0)
    {
        return style::Fg(Color::FromBasic(Color::Basic::Black))
            + style::Bg(Color::FromBasic(Color::Basic::Black));
    }

    if (intensity <= 4)
    {
        return style::Fg(Color::FromBasic(Color::Basic::BrightBlack))
            + style::Bg(black);
    }

    if (intensity <= 9)
    {
        return style::Bold
            + style::Fg(Color::FromBasic(Color::Basic::Red))
            + style::Bg(black);
    }

    if (intensity <= 15)
    {
        return style::Bold
            + style::Fg(Color::FromBasic(Color::Basic::Yellow))
            + style::Bg(black);
    }

    if (intensity <= 50)
    {
        return style::Bold
            + style::Fg(Color::FromBasic(Color::Basic::White))
            + style::Bg(black);
    }

    return style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::White))
        + style::Bg(black);
}