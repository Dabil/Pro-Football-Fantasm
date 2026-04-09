#pragma once

#include "Screens/Screen.h"

class IRenderer;
class Surface;
class ScreenBuffer;
class CapabilityReport;

class RendererDiagnosticsScreen : public Screen
{
public:
    explicit RendererDiagnosticsScreen(const IRenderer* renderer);
    ~RendererDiagnosticsScreen() override = default;

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    void drawBackendState(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;
    void drawPerformance(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;
    void drawAdaptationTable(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;
    void drawExamples(ScreenBuffer& buffer, const CapabilityReport& report, int x, int y, int width, int height) const;

private:
    const IRenderer* m_renderer = nullptr;
    double m_elapsedSeconds = 0.0;
};