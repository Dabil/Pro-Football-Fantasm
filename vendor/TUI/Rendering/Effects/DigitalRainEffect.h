#pragma once

#include <vector>
#include <random>

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"
#include "Rendering/Effects/IEffects.h"

class Surface;

struct DigitalRainEffectOptions
{
    std::u32string glyphPool;
    int maxStreams = 150;
    int deadGlyphsSpawnRate = 25;
    int deadGlyphBlinkRate = 10;
    double mutationIntervalSeconds = 0.065;
};

class DigitalRainEffect final : public IEffect
{
public:
	explicit DigitalRainEffect(TerminalHostKind hostKind = TerminalHostKind::Unknown);
    ~DigitalRainEffect() override = default;

	void onEnter() override;
    void onExit() override {};
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface, const Rect& viewport) override;
    void setOptions(const DigitalRainEffectOptions& options);

    const std::u32string& getGlyphPool() const;

private:

    enum class DropComposition
    {
        BrightWhiteWhite,   // 25%
        WhiteOnly,          // 25%
        GreenOnly           // 50%
    };

    struct DeadGlyph
    {
        int x = 0;
        int y = 0;
        char32_t glyph = U' ';
    };

    struct Stream
    {
        bool active = false;
        float headY = 0.0f;
        float speed = 0.0f;
        int length = 0;
        double respawnDelay = 0.0;
        double mutationTimer = 0.0;
        DropComposition composition = DropComposition::GreenOnly;
        std::vector<char32_t> glyphs;
    };

private:
    static std::u32string buildConsoleGlyphPool();
    static std::u32string buildTerminalGlyphPool();
    static std::u32string buildGlyphPoolForHost(TerminalHostKind hostKind);

    void ensureLayout(const Rect& viewport);
    void rebuildStreams();
    void resetStream(Stream& stream, bool staggerStart);
    void configureActiveStream(Stream& stream);
    void updateStreams(const Animation::TickEvent& event);
    void spawnDeadGlyphs(const Animation::TickEvent& event);
    void drawRain(ScreenBuffer& buffer) const;
    int countActiveStreams() const;
    char32_t randomGlyph();
    Style styleForTrailIndex(int trailIndex, int streamLength, DropComposition composition) const;

private:
    int m_rainLeft = 1;
    int m_rainTop = 1;
    int m_rainWidth = 0;
    int m_rainHeight = 0;
 
    // options
    int m_maxStreams = 150;
    int m_deadGlyphsSpawnRate = 25;
    int m_deadGlyphBlinkRate = 10;
    double m_mutationIntervalSeconds = 0.065;
    std::u32string m_glyphPool;

    double m_elapsedSeconds = 0.0;

    TerminalHostKind m_hostKind = TerminalHostKind::Unknown;
    std::vector<Stream> m_streams;

    // stream colors
    Style m_backgroundStyle;
    Style m_headStyle;
    Style m_secondStyle;
    Style m_thirdStyle;
    Style m_fourthStyle;
    Style m_tailStyle;

    std::vector<DeadGlyph> m_deadGlyphs;
    double m_deadGlyphSpawnAccumulator = 0.0;

    std::mt19937 m_rng;
};