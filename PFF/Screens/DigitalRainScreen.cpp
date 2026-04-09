#include "Screens/DigitalRainScreen.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

#include "Core/Rect.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"
#include "Utilities/Unicode/UnicodeWidth.h"


// TODO:
//  Add Shedding Glyphs
//  have dead glyphs change color or shape rarely
//  add BrightBlack digital drops

namespace
{   
    constexpr char32_t RabbitGlyph = U'\U0001F407';

    constexpr int MinimumScreenWidth = 48;
    constexpr int MinimumScreenHeight = 14;
    constexpr int FooterRows = 2;
    constexpr double PreviewAdvanceIntervalSeconds = 0.08;
    constexpr double MutationIntervalSeconds = 0.065;

    constexpr int MaximumDigitalDrops = 150;
    constexpr int DeadGlyphsSpawnRate = 25;
    constexpr int DeadGlyphBlinkRate = 10;

    struct DeadGlyph
    {
        int x = 0;
        int y = 0;
        char32_t glyph = U' ';
    };

    std::vector<DeadGlyph> m_deadGlyphs;
    double m_deadGlyphSpawnAccumulator = 0.0;

    enum class DropComposition
    {
        BrightWhiteWhite,   // 25%
        WhiteOnly,          // 25%
        GreenOnly           // 50%
    };
}

std::u32string DigitalRainScreen::buildConsoleGlyphPool()
{
    return std::u32string(
        U"0123456789A₿CDEFGHIJKLMNOPQRSTUVWXYZ"
        U"@#$%&*+=<>"
        U"[]{}()"
        U"█▓▒░"
        U"■□▪▫"
        U"▲▼"
        U"○●"
        U"ΑβϲδεφϑհιյΚλʍƞɸπθʀστυƔѡϰψȥ"
        U"┌┐└┘├┤┬┴┼"
        U"╔╗╚╝╠╣╦╩╬"
        U"│┃─━"
        U"←↑→↓");
}

std::u32string DigitalRainScreen::buildTerminalGlyphPool()
{
    return std::u32string(
        U"アァカサタナハマヤャラワガザダバパ"
        U"イィキシチニヒミリヰギジヂビピ"
        U"ウゥクスツヌフムユュルグズブヅプ"
        U"エェケセテネヘメレヱゲゼデベペ"
        U"オォコソトノホモヨョロヲゴゾドボポヴッン"
        U"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" // standard numbers and letters
        U"ΑβϲδεφϑհιյΚλʍƞɸπθʀστυƔѡϰψȥ"           //  greek alphabet 
        // U"♔♕♖♗♘♙♚♛♜♝♞♟"                // chess pieces
        U"♪♫⌘₿äü∄∃ƒ±£µℇ")
        + std::u32string(1, RabbitGlyph);
}

std::u32string DigitalRainScreen::buildGlyphPoolForHost(TerminalHostKind hostKind)
{
    switch (hostKind)
    {
    case TerminalHostKind::ClassicConsoleWindow:
    case TerminalHostKind::RedirectedOrPipe:
    case TerminalHostKind::Unknown:
        return buildConsoleGlyphPool();

    case TerminalHostKind::WindowsTerminal:
    case TerminalHostKind::VsCodeIntegratedTerminal:
    case TerminalHostKind::ConEmu:
    case TerminalHostKind::Mintty:
    case TerminalHostKind::OtherTerminalHost:
    default:
        return buildTerminalGlyphPool();
    }
}

DigitalRainScreen::DigitalRainScreen(TerminalHostKind hostKind)
    : m_hostKind(hostKind)
    , m_glyphPool(buildGlyphPoolForHost(hostKind))
    , m_rng(0x4D415452u)
{
    const Color black = Color::FromBasic(Color::Basic::Black);

    m_headStyle =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(black);

    m_secondStyle =
        style::Fg(Color::FromBasic(Color::Basic::White))
        + style::Bg(black);

    m_thirdStyle =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightGreen))
        + style::Bg(black);

    m_fourthStyle =
        style::Fg(Color::FromBasic(Color::Basic::Green))
        + style::Bg(black);

    m_tailStyle =
        style::Fg(Color::FromBasic(Color::Basic::BrightBlack))
        + style::Bg(black);

    m_backgroundStyle = style::Bg(black);

    m_titleStyle =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightGreen))
        + style::Bg(black);

    m_labelStyle =
        style::Underline
        + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(black);

    m_previewStyle =
        style::Fg(Color::FromBasic(Color::Basic::BrightGreen))
        + style::Bg(black);

    m_borderStyle =
        style::Fg(Color::FromBasic(Color::Basic::Green))
        + style::Bg(black);
}

void DigitalRainScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
    m_previewAdvanceTimer = 0.0;
    m_previewOffset = 0;
    m_streams.clear();
}

void DigitalRainScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;

    if (m_rainWidth <= 0 || m_rainHeight <= 0 || m_streams.empty())
    {
        return;
    }

    updateStreams(deltaTime);
    spawnDeadGlyphs(deltaTime);
    updatePreview(deltaTime);
}

void DigitalRainScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < MinimumScreenWidth || screenHeight < MinimumScreenHeight)
    {
        buffer.fillRect(
            Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
            U' ',
            m_backgroundStyle);

        buffer.drawFrame(
            Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } }, 
            m_borderStyle,
            U'╔', U'╗', U'╚', U'╝', U'═', U'║');

        buffer.writeString(2, 1, "Digital Rain needs a larger console window.", Themes::Warning);
        buffer.writeString(2, 3, "Recommended: at least 48 x 14.", Themes::Subtitle);
        return;
    }

    ensureLayout(screenWidth, screenHeight);

    buffer.fillRect(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        U' ',
        m_backgroundStyle);

    drawRain(buffer);

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        m_borderStyle,
        U'╔', U'╗', U'╚', U'╝', U'═', U'║');

    buffer.drawFrame(
        Rect{ Point{ 2, 1 }, Size{ screenWidth - 4, screenHeight - 2 - FooterRows} },
        m_tailStyle,
        U'┌', U'┐', U'└', U'┘', U'─', U'│');

    drawOverlay(buffer);
}

void DigitalRainScreen::ensureLayout(int screenWidth, int screenHeight)
{
    const int newRainLeft = 3;
    const int newRainTop = 2;
    const int newRainWidth = std::max(0, screenWidth - 6);
    const int newRainHeight = std::max(0, screenHeight - 4 - FooterRows);

    if (screenWidth == m_screenWidth &&
        screenHeight == m_screenHeight &&
        newRainWidth == m_rainWidth &&
        newRainHeight == m_rainHeight)
    {
        return;
    }

    m_screenWidth = screenWidth;
    m_screenHeight = screenHeight;
    m_rainLeft = newRainLeft;
    m_rainTop = newRainTop;
    m_rainWidth = newRainWidth;
    m_rainHeight = newRainHeight;

    rebuildStreams();
}

void DigitalRainScreen::rebuildStreams()
{
    if (m_rainWidth <= 0 || m_rainHeight <= 0)
    {
        m_streams.clear();
        return;
    }

    m_streams.assign(static_cast<std::size_t>(m_rainWidth), Stream{});

    for (Stream& stream : m_streams)
    {
        resetStream(stream, true);
    }
}

void DigitalRainScreen::resetStream(Stream& stream, bool staggerStart)
{
    stream.active = false;
    stream.headY = 0.0f;
    stream.speed = 0.0f;
    stream.length = 0;
    stream.mutationTimer = 0.0;
    stream.glyphs.clear();
    stream.composition = DropComposition::GreenOnly;

    std::uniform_real_distribution<double> delayDistribution(
        0.0,
        staggerStart ? 4.8 : 2.5);

    stream.respawnDelay = delayDistribution(m_rng);

    if (!staggerStart)
    {
        configureActiveStream(stream);
    }
}

void DigitalRainScreen::configureActiveStream(Stream& stream)
{
    std::uniform_real_distribution<float> speedDistribution(0.25f, 28.0f);
    std::uniform_int_distribution<int> lengthDistribution(6, std::max(6, std::min(m_rainHeight, 24)));
    std::uniform_real_distribution<float> headOffsetDistribution(1.0f, static_cast<float>(std::max(2, m_rainHeight / 2)));
    std::uniform_int_distribution<int> compositionDistribution(0, 99);

    stream.active = true;
    stream.speed = speedDistribution(m_rng);
    stream.length = lengthDistribution(m_rng);
    stream.headY = -headOffsetDistribution(m_rng);
    stream.mutationTimer = 0.0;
    stream.glyphs.assign(static_cast<std::size_t>(stream.length), U' ');

    const int compositionRoll = compositionDistribution(m_rng);

    if (compositionRoll < 25)
    {
        stream.composition = DropComposition::BrightWhiteWhite;
    }
    else if (compositionRoll < 50)
    {
        stream.composition = DropComposition::WhiteOnly;
    }
    else
    {
        stream.composition = DropComposition::GreenOnly;
    }

    for (char32_t& glyph : stream.glyphs)
    {
        glyph = randomGlyph();
    }
}

void DigitalRainScreen::updateStreams(double deltaTime)
{
    std::uniform_int_distribution<int> mutationCountDistribution(1, 3);

    for (Stream& stream : m_streams)
    {
        if (!stream.active)
        {
            stream.respawnDelay -= deltaTime;

            if (stream.respawnDelay <= 0.0)
            {
                // Enforce global active stream cap
                if (countActiveStreams() < MaximumDigitalDrops)
                {
                    configureActiveStream(stream);
                }
                else
                {
                    // Defer activation slightly to avoid tight retry loop
                    stream.respawnDelay = 0.1;
                }
            }

            continue;
        }

        const int previousHeadRow = static_cast<int>(std::floor(stream.headY));
        stream.headY += stream.speed * static_cast<float>(deltaTime);
        const int currentHeadRow = static_cast<int>(std::floor(stream.headY));

        for (int row = previousHeadRow; row < currentHeadRow; ++row)
        {
            if (!stream.glyphs.empty())
            {
                for (int i = stream.length - 1; i > 0; --i)
                {
                    stream.glyphs[static_cast<std::size_t>(i)] = stream.glyphs[static_cast<std::size_t>(i - 1)];
                }

                stream.glyphs[0] = randomGlyph();
            }
        }

        stream.mutationTimer += deltaTime;

        while (stream.mutationTimer >= MutationIntervalSeconds)
        {
            stream.mutationTimer -= MutationIntervalSeconds;

            const int mutationCount = mutationCountDistribution(m_rng);
            if (!stream.glyphs.empty())
            {
                std::uniform_int_distribution<int> glyphIndexDistribution(0, static_cast<int>(stream.glyphs.size()) - 1);

                for (int i = 0; i < mutationCount; ++i)
                {
                    stream.glyphs[static_cast<std::size_t>(glyphIndexDistribution(m_rng))] = randomGlyph();
                }
            }
        }

        if (stream.headY - static_cast<float>(stream.length) > static_cast<float>(m_rainHeight))
        {
            resetStream(stream, false);
        }
    }
}

void DigitalRainScreen::updatePreview(double deltaTime)
{
    if (m_glyphPool.empty())
    {
        return;
    }

    m_previewAdvanceTimer += deltaTime;

    while (m_previewAdvanceTimer >= PreviewAdvanceIntervalSeconds)
    {
        m_previewAdvanceTimer -= PreviewAdvanceIntervalSeconds;
        m_previewOffset = (m_previewOffset + 1) % static_cast<int>(m_glyphPool.size());
    }
}

int DigitalRainScreen::countActiveStreams() const
{
    int count = 0;

    for (const Stream& stream : m_streams)
    {
        if (stream.active)
        {
            ++count;
        }
    }

    return count;
}

void DigitalRainScreen::spawnDeadGlyphs(double deltaTime)
{
    // Accumulate time
    m_deadGlyphSpawnAccumulator += deltaTime;

    // How many glyphs per second
    constexpr double spawnRate = static_cast<double>(DeadGlyphsSpawnRate);

    if (spawnRate <= 0.0)
    {
        return;
    }

    const double interval = 1.0 / spawnRate;

    // Spawn as many as needed this frame
    while (m_deadGlyphSpawnAccumulator >= interval)
    {
        m_deadGlyphSpawnAccumulator -= interval;

        if (m_rainWidth <= 0 || m_rainHeight <= 0)
        {
            continue;
        }

        std::uniform_int_distribution<int> xDist(0, m_rainWidth - 1);
        std::uniform_int_distribution<int> yDist(0, m_rainHeight - 1);

        DeadGlyph glyph;
        glyph.x = xDist(m_rng);
        glyph.y = yDist(m_rng);
        glyph.glyph = randomGlyph();

        m_deadGlyphs.push_back(glyph);
    }

    // Optional: cap to prevent unbounded growth
    constexpr std::size_t maxDeadGlyphs = 512;
    if (m_deadGlyphs.size() > maxDeadGlyphs)
    {
        m_deadGlyphs.erase(
            m_deadGlyphs.begin(),
            m_deadGlyphs.begin() + (m_deadGlyphs.size() - maxDeadGlyphs));
    }
}

void DigitalRainScreen::drawRain(ScreenBuffer& buffer) const
{
    buffer.fillRect(
        Rect{ Point{ m_rainLeft, m_rainTop }, Size{ m_rainWidth, m_rainHeight } },
        U' ',
        m_backgroundStyle);

    // Draw dead glyphs (background noise layer)
    for (const DeadGlyph& glyph : m_deadGlyphs)
    {
        buffer.writeCodePoint(
            m_rainLeft + glyph.x,
            m_rainTop + glyph.y,
            glyph.glyph,
            m_tailStyle); // dim / "dead"
    }

    for (int column = 0; column < m_rainWidth; ++column)
    {
        const Stream& stream = m_streams[static_cast<std::size_t>(column)];
        if (!stream.active)
        {
            continue;
        }

        const int headRow = static_cast<int>(std::floor(stream.headY));

        for (int trailIndex = 0; trailIndex < stream.length; ++trailIndex)
        {
            const int y = headRow - trailIndex;

            if (y < 0 || y >= m_rainHeight)
            {
                continue;
            }

            const char32_t glyph = stream.glyphs[static_cast<std::size_t>(trailIndex)];
            const Style style = styleForTrailIndex(trailIndex, stream.length, stream.composition);
            buffer.writeCodePoint(m_rainLeft + column, m_rainTop + y, glyph, style);
        }
    }
}

void DigitalRainScreen::drawOverlay(ScreenBuffer& buffer) const
{
    buffer.writeString(4, 0, "[ Digital Rain Unicode Validation ]", m_titleStyle);

    const int footerLabelY = m_screenHeight - 3;
    const int footerPreviewY = m_screenHeight - 2;
    
    buffer.writeString(2, footerLabelY, "Pool", m_labelStyle);
    buffer.writeChar(6, footerLabelY, ':', m_headStyle);
    drawPreviewLine(buffer, 8, footerLabelY, std::max(0, m_screenWidth - 10), m_previewOffset);
    
    // Use this footer for console
    /*
    buffer.writeString(2, footerPreviewY, "Sample:", m_labelStyle);

    buffer.writeCodePoint(10, footerPreviewY, U'█', m_previewStyle);
    buffer.writeCodePoint(12, footerPreviewY, U'◆', m_previewStyle);
    buffer.writeCodePoint(14, footerPreviewY, U'╬', m_previewStyle);
    buffer.writeCodePoint(16, footerPreviewY, U'↑', m_previewStyle);
    buffer.writeCodePoint(18, footerPreviewY, U'△', m_previewStyle);

    buffer.writeString(21, footerPreviewY, "Console-safe fallback glyphs active", m_labelStyle);
    */
    // Use this footer for Terminal
    
    buffer.writeCodePoint(10, footerPreviewY, U'ア', m_previewStyle);
    buffer.writeCodePoint(12, footerPreviewY, U'♔', m_previewStyle);
    buffer.writeCodePoint(14, footerPreviewY, U'₿', m_previewStyle);
    buffer.writeCodePoint(16, footerPreviewY, U'♪', m_previewStyle);
    buffer.writeCodePoint(18, footerPreviewY, RabbitGlyph, m_previewStyle);

    buffer.writeString(21, footerPreviewY, "Katakana + chess + music + rabbit + math", m_labelStyle);
}

void DigitalRainScreen::drawPreviewLine(ScreenBuffer& buffer, int x, int y, int availableWidth, int startIndex) const
{
    if (availableWidth <= 0 || m_glyphPool.empty())
    {
        return;
    }

    int writeX = x;
    int glyphIndex = startIndex % static_cast<int>(m_glyphPool.size());

    while (writeX < x + availableWidth)
    {
        const char32_t glyph = m_glyphPool[static_cast<std::size_t>(glyphIndex)];
        const CellWidth width = UnicodeWidth::measureCodePointWidth(glyph);
        const int glyphWidth = (width == CellWidth::Two) ? 2 : 1;

        if (writeX + glyphWidth > x + availableWidth)
        {
            break;
        }

        buffer.writeCodePoint(writeX, y, glyph, m_previewStyle);
        writeX += glyphWidth;
        glyphIndex = (glyphIndex + 1) % static_cast<int>(m_glyphPool.size());
    }
}

char32_t DigitalRainScreen::randomGlyph()
{
    if (m_glyphPool.empty())
    {
        return U' ';
    }

    std::uniform_int_distribution<int> glyphDistribution(0, static_cast<int>(m_glyphPool.size()) - 1);
    return m_glyphPool[static_cast<std::size_t>(glyphDistribution(m_rng))];
}

Style DigitalRainScreen::styleForTrailIndex(
    int trailIndex,
    int streamLength,
    DropComposition composition) const
{
    if (streamLength <= 0)
    {
        return m_tailStyle;
    }

    const int brightBlackStart = std::max(0, streamLength - 2);
    if (trailIndex >= brightBlackStart)
    {
        return m_tailStyle;
    }

    int brightGreenStart = 0;
    int greenStart = 0;

    switch (composition)
    {
    case DropComposition::BrightWhiteWhite:
        // 1 BrightWhite
        // 1 White
        // half middle BrightGreen
        // half middle Green
        // last 2 BrightBlack
        if (trailIndex == 0)
        {
            return m_headStyle;
        }

        if (trailIndex == 1)
        {
            return m_secondStyle;
        }

        brightGreenStart = 2;
        break;

    case DropComposition::WhiteOnly:
        // 1 White
        // half middle BrightGreen
        // half middle Green
        // last 2 BrightBlack
        if (trailIndex == 0)
        {
            return m_secondStyle;
        }

        brightGreenStart = 1;
        break;

    case DropComposition::GreenOnly:
    default:
        // half BrightGreen
        // half Green
        // last 2 BrightBlack
        brightGreenStart = 0;
        break;
    }

    const int middleCount = std::max(0, brightBlackStart - brightGreenStart);
    if (middleCount <= 0)
    {
        return m_fourthStyle;
    }

    const int brightGreenCount = (middleCount + 1) / 2;
    greenStart = brightGreenStart + brightGreenCount;

    if (trailIndex < greenStart)
    {
        return m_thirdStyle;
    }

    return m_fourthStyle;
}