#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "Screens/Screen.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"

class Surface;
class ScreenBuffer;


class DigitalRainScreen : public Screen
{
public:
    ~DigitalRainScreen() override = default;
    explicit DigitalRainScreen(TerminalHostKind hostKind = TerminalHostKind::Unknown);

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    enum class DropComposition
    {
        BrightWhiteWhite,   // 25%
        WhiteOnly,          // 25%
        GreenOnly           // 50%
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

    void ensureLayout(int screenWidth, int screenHeight);
    void rebuildStreams();
    void resetStream(Stream& stream, bool staggerStart);
    void configureActiveStream(Stream& stream);
    void updateStreams(double deltaTime);
    void updatePreview(double deltaTime);
    void spawnDeadGlyphs(double deltaTime);

    void drawRain(ScreenBuffer& buffer) const;
    void drawOverlay(ScreenBuffer& buffer) const;
    void drawPreviewLine(ScreenBuffer& buffer, int x, int y, int availableWidth, int startIndex) const;

    int countActiveStreams() const;

    char32_t randomGlyph();

    Style styleForTrailIndex(int trailIndex, int streamLength, DropComposition composition) const;

private:
    int m_screenWidth = 0;
    int m_screenHeight = 0;

    int m_rainLeft = 1;
    int m_rainTop = 1;
    int m_rainWidth = 0;
    int m_rainHeight = 0;

    double m_elapsedSeconds = 0.0;
    double m_previewAdvanceTimer = 0.0;
    int m_previewOffset = 0;

    TerminalHostKind m_hostKind = TerminalHostKind::Unknown;
    std::u32string m_glyphPool;
    std::vector<Stream> m_streams;

    Style m_headStyle;
    Style m_secondStyle;
    Style m_thirdStyle;
    Style m_fourthStyle;
    Style m_tailStyle;
    Style m_backgroundStyle;
    Style m_titleStyle;
    Style m_labelStyle;
    Style m_previewStyle;
    Style m_borderStyle;

    std::mt19937 m_rng;
};