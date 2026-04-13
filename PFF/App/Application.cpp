// App/Application.cpp
#include "App/Application.h"
#include "App/ScreenManager.h"

#include <chrono>
#include <thread>
#include <windows.h>

#include "Rendering/ConsoleRenderer.h"
#include "Rendering/Surface.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/TerminalRenderer.h"

#include "Screens/DigitalRainScreen.h"
#include "Screens/Donut3DScreen.h"
#include "Screens/FireScreen.h"
#include "Screens/PffGameScreen.h"
#include "Screens/RendererDiagnosticsScreen.h"
#include "Screens/TerminalCapabilitiesScreen.h"
#include "Screens/WaterEffectScreen.h"

static Application* g_appInstance = nullptr;

Application::Application(
    StartupRendererSelection rendererSelection,
    StartupValidationScreenPreference validationScreenSelection,
    const StartupDiagnosticsContext& startupDiagnostics)
    : m_rendererSelection(rendererSelection)
    , m_validationScreenStart(validationScreenSelection)
    , m_startupDiagnostics(startupDiagnostics)
{
}

Application::~Application()
{
    shutdown();
}

#define NOMINMAX
#include <windows.h>

namespace
{
    void maximizeHostWindow()
    {
        HWND hwnd = GetConsoleWindow();
        if (hwnd != nullptr)
        {
            ShowWindow(hwnd, SW_MAXIMIZE);
        }
    }
}

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType)
{
    switch (ctrlType)
    {
    case CTRL_CLOSE_EVENT:
    case CTRL_SHUTDOWN_EVENT:
    case CTRL_LOGOFF_EVENT:
    {
        if (g_appInstance != nullptr)
        {
            g_appInstance->shutdown();
        }

        return TRUE;
    }
    default:
        return FALSE;
    }
}

bool Application::initialize()
{
    g_appInstance = this;
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (m_rendererSelection == StartupRendererSelection::Terminal)
    {
        auto terminalRenderer = std::make_unique<TerminalRenderer>();
        terminalRenderer->setDiagnosticsEnabled(true);
        terminalRenderer->setDiagnosticsAppendMode(false);
        terminalRenderer->setDiagnosticsOutputPath("render_diagnostics_report.txt");

        StartupDiagnosticsContext terminalDiagnostics = m_startupDiagnostics;
        terminalDiagnostics.actualRenderer = RendererKind::TerminalRenderer;
        terminalRenderer->setStartupDiagnosticsContext(terminalDiagnostics);

        if (terminalRenderer->initialize())
        {
            m_startupDiagnostics = terminalDiagnostics;
            m_renderer = std::move(terminalRenderer);
        }
    }

    if (!m_renderer)
    {
        auto consoleRenderer = std::make_unique<ConsoleRenderer>();
        consoleRenderer->setDiagnosticsEnabled(true);
        consoleRenderer->setDiagnosticsAppendMode(false);
        consoleRenderer->setDiagnosticsOutputPath("render_diagnostics_report.txt");

        StartupDiagnosticsContext consoleDiagnostics = m_startupDiagnostics;
        consoleDiagnostics.actualRenderer = RendererKind::ConsoleRenderer;
        consoleRenderer->setStartupDiagnosticsContext(consoleDiagnostics);

        if (!consoleRenderer->initialize())
        {
            return false;
        }

        m_startupDiagnostics = consoleDiagnostics;
        m_renderer = std::move(consoleRenderer);
    }

    if (m_startupDiagnostics.actualHost == TerminalHostKind::ClassicConsoleWindow)
    {
        maximizeHostWindow();
    }

    m_width = m_renderer->getConsoleWidth();
    m_height = m_renderer->getConsoleHeight();

    m_surface = std::make_unique<Surface>(m_width, m_height);
    m_screenManager = std::make_unique<ScreenManager>();

    configureAssetLibrary();

    if (m_validationScreenStart == StartupValidationScreenPreference::ValidationStartTrue)
    {
        m_currentScreenType = ScreenType::TerminalCapabilities;
    }
    else
    {
        m_currentScreenType = ScreenType::PffGame;
    }

    m_screenCycleElapsedSeconds = 0.0;

    switchToScreen(m_currentScreenType);

    m_running = true;
    return true;
}

void Application::run()
{
    using clock = std::chrono::high_resolution_clock;

    auto previousTime = clock::now();

    while (m_running)
    {
        auto currentTime = clock::now();
        std::chrono::duration<double> delta = currentTime - previousTime;
        previousTime = currentTime;

        handleResize();

        update(delta.count());
        render();

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void Application::shutdown()
{
    if (!m_running)
    {
        return;
    }

    m_running = false;

    m_screenManager.reset();

    if (m_renderer)
    {
        m_renderer->shutdown();
    }
}

void Application::handleResize()
{
    if (m_renderer->pollResize())
    {
        m_width = m_renderer->getConsoleWidth();
        m_height = m_renderer->getConsoleHeight();

        m_surface->resize(m_width, m_height);
    }
}

void Application::update(double deltaTime)
{
    updateScreenCycle(deltaTime);
    m_screenManager->update(deltaTime);
}

void Application::render()
{
    m_surface->clear(Themes::Disabled);

    m_screenManager->drawCurrentScreen(*m_surface);

    m_renderer->present(m_surface->buffer());
}

void Application::switchToScreen(ScreenType screenType)
{
    m_screenManager->clearScreens();

    switch (screenType)
    {
    case ScreenType::TerminalCapabilities:
        m_screenManager->pushScreen(std::make_unique<TerminalCapabilitiesScreen>(m_renderer.get()));
        break;

    case ScreenType::RendererDiagnostics:
        m_screenManager->pushScreen(std::make_unique<RendererDiagnosticsScreen>(m_renderer.get()));
        break;

    case ScreenType::PffGame:
        m_screenManager->pushScreen(std::make_unique<PffGameScreen>(m_assetLibrary));
        break;

    case ScreenType::DigitalRain:
        m_screenManager->pushScreen(
            std::make_unique<DigitalRainScreen>(m_startupDiagnostics.actualHost));
        break;

    case ScreenType::WaterEffect:
        m_screenManager->pushScreen(std::make_unique<WaterEffectScreen>());
        break;

    case ScreenType::Donut3D:
        m_screenManager->pushScreen(std::make_unique<Donut3DScreen>());
        break;

    case ScreenType::Fire:
        m_screenManager->pushScreen(std::make_unique<FireScreen>());
        break;
    }

    m_currentScreenType = screenType;
}

void Application::advanceScreen()
{
    switch (m_currentScreenType)
    {
    case ScreenType::PffGame:
        break;

    case ScreenType::DigitalRain:
        switchToScreen(ScreenType::WaterEffect);
        break;

    case ScreenType::WaterEffect:
        switchToScreen(ScreenType::Donut3D);
        break;

    case ScreenType::Donut3D:
        switchToScreen(ScreenType::Fire);
        break;

    case ScreenType::Fire:
        switchToScreen(ScreenType::DigitalRain);
        break;

    case ScreenType::TerminalCapabilities:
        switchToScreen(ScreenType::RendererDiagnostics);
        break;

    case ScreenType::RendererDiagnostics:
        switchToScreen(ScreenType::TerminalCapabilities);
        break;
    }
}

void Application::updateScreenCycle(double deltaTime)
{
    m_screenCycleElapsedSeconds += deltaTime;

    while (m_screenCycleElapsedSeconds >= m_screenCycleIntervalSeconds)
    {
        m_screenCycleElapsedSeconds -= m_screenCycleIntervalSeconds;
        advanceScreen();
    }
}

void Application::configureAssetLibrary()
{
    m_assetLibrary.setAssetsRoot("Assets");

    m_assetLibrary.registerAlias("pff.field", "XP/Field.xp");
    m_assetLibrary.registerAlias("pff.scoreboard", "XP/ScoreBoard.xp");
    m_assetLibrary.registerAlias("pff.scoreboard_font", "XP/ScoreBoardFont.xp");
}