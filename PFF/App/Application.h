#pragma once

#include <memory>

#include "App/TerminalLauncher.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"

class ScreenManager;
class IRenderer;
class Surface;

class Application
{
public:
    explicit Application(
        StartupRendererSelection rendererSelection,
        StartupValidationScreenPreference validationScreenSelection,
        const StartupDiagnosticsContext& startupDiagnostics);
    ~Application();

    bool initialize();
    void run();
    void shutdown();

private:
    enum class ScreenType
    {
        TerminalCapabilities,
        RendererDiagnostics,
        PffField,
        DigitalRain,
        WaterEffect,
        Donut3D,
        Fire
    };

private:
    void handleResize();
    void update(double deltaTime);
    void render();

    void switchToScreen(ScreenType screenType);
    void advanceScreen();
    void updateScreenCycle(double deltaTime);

private:
    StartupRendererSelection m_rendererSelection = StartupRendererSelection::Console;
    StartupValidationScreenPreference m_validationScreenStart = StartupValidationScreenPreference::ValidationStartFalse;
    StartupDiagnosticsContext m_startupDiagnostics{};

    std::unique_ptr<ScreenManager> m_screenManager;
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<Surface> m_surface;

    bool m_running = false;

    int m_width = 0;
    int m_height = 0;

    ScreenType m_currentScreenType = ScreenType::PffField;
    double m_screenCycleElapsedSeconds = 0.0;
    double m_screenCycleIntervalSeconds = 60.0;
};