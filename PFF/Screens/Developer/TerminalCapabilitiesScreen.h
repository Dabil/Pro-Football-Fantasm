#pragma once

#include "Screens/Screen.h"

class IRenderer;
class Surface;
class ScreenBuffer;
class CapabilityReport;

class TerminalCapabilitiesScreen : public Screen
{
public:
    explicit TerminalCapabilitiesScreen(const IRenderer* renderer);
    ~TerminalCapabilitiesScreen() override = default;

    void onEnter() override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    void drawSummaryPanel(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width) const;
    void drawFeatureMatrix(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;
    void drawColorTierPanel(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;
    void drawTextCapabilityPanel(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;

private:
    const IRenderer* m_renderer = nullptr;
    double m_elapsedSeconds = 0.0;
};