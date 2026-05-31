#include "Rendering/Effects/DigitalRainEffect.h"

#include <vector>

#include "Rendering/Surface.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Utilities/Unicode/UnicodeWidth.h"
#include "Utilities/Unicode/UnicodeConversion.h"
#include "Animation/TickEvent.h"

namespace
{
    constexpr char32_t RabbitGlyph = U'\U0001F407';
}

std::u32string DigitalRainEffect::buildConsoleGlyphPool()
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

//        U"♜♞♝♛♚♝♞♜♖♘♗♕♔♗♘♖"
//        + std::u32string(1, RabbitGlyph)
std::u32string DigitalRainEffect::buildTerminalGlyphPool()
{
    return std::u32string(
        U"アァカサタナハマヤャラワガザダバパ"
        U"イィキシチニヒミリヰギジヂビピ"
        U"ウゥクスツヌフムユュルグズブヅプ"
        U"エェケセテネヘメレヱゲゼデベペ"
        U"オォコソトノホモヨョロヲゴゾドボポヴッン"
        U"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ" // standard numbers and letters
        U"ΑβϲδεφϑհιյΚλʍƞɸπθʀστυƔѡϰψȥ"           // greek alphabet
        U"♪♫⌘₿äü∄∃ƒ±£µℇ");
}

std::u32string DigitalRainEffect::buildGlyphPoolForHost(TerminalHostKind hostKind)
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

DigitalRainEffect::DigitalRainEffect(TerminalHostKind hostKind)
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
}

void DigitalRainEffect::onEnter()
{
    m_elapsedSeconds = 0.0;
    m_streams.clear();

    m_deadGlyphs.clear();
    m_deadGlyphSpawnAccumulator = 0.0;
}

void DigitalRainEffect::update(const Animation::TickEvent& event)
{
    if (m_rainWidth <= 0 || m_rainHeight <= 0 || m_streams.empty())
    {
        return;
    }

    m_elapsedSeconds += event.deltaSeconds;

    updateStreams(event);
    spawnDeadGlyphs(event);
}

void DigitalRainEffect::draw(Surface& surface, const Rect& viewport)
{
    ScreenBuffer& buffer = surface.buffer();

    ensureLayout(viewport);
    drawRain(buffer);
}

void DigitalRainEffect::setOptions(const DigitalRainEffectOptions& options)
{
    m_glyphPool = options.glyphPool;
    m_maxStreams = options.maxStreams;
    m_mutationIntervalSeconds = options.mutationIntervalSeconds;
    m_deadGlyphsSpawnRate = options.deadGlyphsSpawnRate;
    m_deadGlyphBlinkRate = options.deadGlyphBlinkRate;
}

const std::u32string& DigitalRainEffect::getGlyphPool() const
{
    return m_glyphPool;
}

void DigitalRainEffect::ensureLayout(const Rect& viewport)
{
    const int newRainLeft = viewport.position.x;
    const int newRainTop = viewport.position.y;
    const int newRainWidth = viewport.size.width - 1;
    const int newRainHeight = viewport.size.height;

    const bool sizeChanged =
        newRainWidth != m_rainWidth ||
        newRainHeight != m_rainHeight;

    m_rainLeft = newRainLeft;
    m_rainTop = newRainTop;
    m_rainWidth = newRainWidth;
    m_rainHeight = newRainHeight;

    if (sizeChanged)
    {
        m_deadGlyphs.clear();
        m_deadGlyphSpawnAccumulator = 0.0;

        rebuildStreams();
    }
}

void DigitalRainEffect::rebuildStreams()
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

void DigitalRainEffect::resetStream(Stream& stream, bool staggerStart)
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

void DigitalRainEffect::configureActiveStream(Stream& stream)
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

int DigitalRainEffect::countActiveStreams() const
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

void DigitalRainEffect::updateStreams(const Animation::TickEvent& event)
{
    std::uniform_int_distribution<int> mutationCountDistribution(1, 3);

    for (Stream& stream : m_streams)
    {
        if (!stream.active)
        {
            stream.respawnDelay -= event.deltaSeconds;

            if (stream.respawnDelay <= 0.0)
            {
                // Enforce global active stream cap
                if (countActiveStreams() < m_maxStreams)
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
        stream.headY += stream.speed * static_cast<float>(event.deltaSeconds);
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

        stream.mutationTimer += event.deltaSeconds;

        while (stream.mutationTimer >= m_mutationIntervalSeconds)
        {
            stream.mutationTimer -= m_mutationIntervalSeconds;

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

void DigitalRainEffect::spawnDeadGlyphs(const Animation::TickEvent& event)
{
    // Accumulate time
    m_deadGlyphSpawnAccumulator += event.deltaSeconds;

    // How many glyphs per second
    double spawnRate = (double)m_deadGlyphsSpawnRate;

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

void DigitalRainEffect::drawRain(ScreenBuffer& buffer) const
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

char32_t DigitalRainEffect::randomGlyph()
{
    if (m_glyphPool.empty())
    {
        return U' ';
    }

    std::uniform_int_distribution<int> glyphDistribution(0, static_cast<int>(m_glyphPool.size()) - 1);
    return m_glyphPool[static_cast<std::size_t>(glyphDistribution(m_rng))];
}

Style DigitalRainEffect::styleForTrailIndex(
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