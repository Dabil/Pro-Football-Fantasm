#include "App/ApplicationHost.h"

#include "App/ScreenManager.h"
#include "Screens/Screen.h"
#include "Input/Event.h"
#include "Input/InputManager.h"
#include "Rendering/ConsoleRenderer.h"
#include "Rendering/Surface.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/TerminalRenderer.h"

#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#define NOMINMAX
#include <windows.h>

namespace
{
    ApplicationHost* g_applicationHostInstance = nullptr;

    void maximizeHostWindow()
    {
        HWND hwnd = GetConsoleWindow();
        if (hwnd != nullptr)
        {
            ShowWindow(hwnd, SW_MAXIMIZE);
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
            if (g_applicationHostInstance != nullptr)
            {
                g_applicationHostInstance->shutdown();
            }

            return TRUE;
        }
        default:
            return FALSE;
        }
    }

    constexpr std::string_view RouteCommandPrefix = "route:";
    constexpr std::string_view ReplaceRouteCommandPrefix = "replace-route:";

    bool startsWith(std::string_view value, std::string_view prefix)
    {
        return value.size() >= prefix.size()
            && value.substr(0, prefix.size()) == prefix;
    }
}

ApplicationHost::ApplicationHost(
    StartupRendererSelection rendererSelection,
    StartupValidationScreenPreference validationScreenSelection,
    const StartupDiagnosticsContext& startupDiagnostics)
    : m_rendererSelection(rendererSelection)
    , m_validationScreenStart(validationScreenSelection)
    , m_startupDiagnostics(startupDiagnostics)
{
}

ApplicationHost::~ApplicationHost()
{
    shutdown();
}

bool ApplicationHost::initialize()
{
    g_applicationHostInstance = this;
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (!initializeRenderer())
    {
        return false;
    }

    if (m_startupDiagnostics.actualHost == TerminalHostKind::ClassicConsoleWindow)
    {
        maximizeHostWindow();
    }

    m_width = m_renderer->getConsoleWidth();
    m_height = m_renderer->getConsoleHeight();

    m_surface = std::make_unique<Surface>(m_width, m_height);
    m_screenManager = std::make_unique<ScreenManager>();

    if (!initializeInput())
    {
        shutdown();
        return false;
    }

    m_commandMap = Input::CommandMap::createDefault();

    configureAssetLibrary();

    if (!onInitialize())
    {
        shutdown();
        return false;
    }

    m_running = true;
    m_renderRequested = true;

    return true;
}

void ApplicationHost::run()
{
    m_tickClock.reset();

    while (m_running)
    {
        m_tickClock.tick();

        Animation::TickEvent tickEvent =
            Animation::TickEvent::fromClock(m_tickClock, m_frameIndex);

        handleResize();

        if (m_inputManager)
        {
            m_inputManager->poll();

            std::vector<Input::Event> events;

            for (const Input::Event& inputEvent : m_inputManager->drainEvents())
            {
                events.push_back(inputEvent);

                if (const Input::KeyEvent* keyEvent = inputEvent.asKey())
                {
                    std::optional<Input::Command> command = m_commandMap.map(*keyEvent);
                    if (command.has_value())
                    {
                        events.push_back(Input::Event::command(command.value()));
                    }
                }
            }

            events.push_back(Input::Event::tick(tickEvent));

            for (const Input::Event& event : events)
            {
                dispatchEvent(event);

                if (!m_running)
                {
                    break;
                }
            }
        }

        if (!m_running)
        {
            break;
        }

        update(tickEvent);

        if (m_renderRequested)
        {
            render();
            m_renderRequested = false;
        }
        else
        {
            render();
        }

        ++m_frameIndex;

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

void ApplicationHost::shutdown()
{
    const bool wasRunning = m_running;
    m_running = false;

    if (wasRunning)
    {
        onShutdown();
    }

    m_screenManager.reset();

    if (m_renderer)
    {
        m_renderer->shutdown();
        m_renderer.reset();
    }

    if (m_inputManager)
    {
        m_inputManager->shutdown();
        m_inputManager.reset();
    }

    m_surface.reset();

    if (g_applicationHostInstance == this)
    {
        SetConsoleCtrlHandler(ConsoleCtrlHandler, FALSE);
        g_applicationHostInstance = nullptr;
    }
}

bool ApplicationHost::isRunning() const
{
    return m_running;
}

ScreenManager& ApplicationHost::screenManager()
{
    return *m_screenManager;
}

const ScreenManager& ApplicationHost::screenManager() const
{
    return *m_screenManager;
}

IRenderer& ApplicationHost::renderer()
{
    return *m_renderer;
}

const IRenderer& ApplicationHost::renderer() const
{
    return *m_renderer;
}

Surface& ApplicationHost::surface()
{
    return *m_surface;
}

const Surface& ApplicationHost::surface() const
{
    return *m_surface;
}

Assets::AssetLibrary& ApplicationHost::assetLibrary()
{
    return m_assetLibrary;
}

const Assets::AssetLibrary& ApplicationHost::assetLibrary() const
{
    return m_assetLibrary;
}

const StartupDiagnosticsContext& ApplicationHost::startupDiagnostics() const
{
    return m_startupDiagnostics;
}

int ApplicationHost::width() const
{
    return m_width;
}

int ApplicationHost::height() const
{
    return m_height;
}

std::uint64_t ApplicationHost::frameIndex() const
{
    return m_frameIndex;
}

void ApplicationHost::requestShutdown()
{
    shutdown();
}

void ApplicationHost::requestRender()
{
    m_renderRequested = true;
}

bool ApplicationHost::registerScreenRoute(std::string routeId, ScreenFactory factory)
{
    if (!m_screenManager)
    {
        return false;
    }

    return m_screenManager->registerRoute(std::move(routeId), std::move(factory));
}

bool ApplicationHost::hasScreenRoute(const std::string& routeId) const
{
    if (!m_screenManager)
    {
        return false;
    }

    return m_screenManager->hasRoute(routeId);
}

bool ApplicationHost::navigateTo(const std::string& routeId)
{
    if (!m_screenManager)
    {
        return false;
    }

    const bool navigated = m_screenManager->pushRoute(routeId);

    if (navigated)
    {
        requestRender();
    }

    return navigated;
}

bool ApplicationHost::replaceWith(const std::string& routeId)
{
    if (!m_screenManager)
    {
        return false;
    }

    const bool replaced = m_screenManager->replaceWithRoute(routeId);

    if (replaced)
    {
        requestRender();
    }

    return replaced;
}

bool ApplicationHost::goBack()
{
    if (!m_screenManager || !m_screenManager->hasScreens())
    {
        return false;
    }

    m_screenManager->popScreen();
    requestRender();

    if (!m_screenManager->hasScreens())
    {
        shutdown();
    }

    return true;
}

bool ApplicationHost::handleCommandId(std::string_view commandId)
{
    if (commandId.empty())
    {
        return false;
    }

    if (startsWith(commandId, RouteCommandPrefix))
    {
        const std::string routeId(commandId.substr(RouteCommandPrefix.size()));
        return navigateTo(routeId);
    }

    if (startsWith(commandId, ReplaceRouteCommandPrefix))
    {
        const std::string routeId(commandId.substr(ReplaceRouteCommandPrefix.size()));
        return replaceWith(routeId);
    }

    if (commandId == "nav:back" || commandId == "route:back")
    {
        return goBack();
    }

    return false;
}

void ApplicationHost::configureAssetLibrary()
{
}

bool ApplicationHost::onInitialize()
{
    return true;
}

void ApplicationHost::onShutdown()
{
}

bool ApplicationHost::dispatchEvent(const Input::Event& event)
{
    if (dispatchToActiveScreen(event))
    {
        return true;
    }

    if (const Input::CommandEvent* commandEvent = event.asCommand())
    {
        return handleApplicationCommand(*commandEvent);
    }

    return false;
}

bool ApplicationHost::dispatchToActiveScreen(const Input::Event& event)
{
    if (!m_screenManager)
    {
        return false;
    }

    return m_screenManager->dispatchEvent(event);
}

bool ApplicationHost::handleApplicationCommand(const Input::CommandEvent& commandEvent)
{
    switch (commandEvent.command.code)
    {
    case Input::CommandCode::Quit:
    case Input::CommandCode::Close:
    case Input::CommandCode::Cancel:
        shutdown();
        return true;

    case Input::CommandCode::Refresh:
        requestRender();
        return true;

    case Input::CommandCode::Back:
        return goBack();

    default:
        return false;
    }
}

void ApplicationHost::update(const Animation::TickEvent& event)
{
    if (m_screenManager)
    {
        m_screenManager->update(event);
    }
}

void ApplicationHost::render()
{
    if (!m_surface || !m_screenManager || !m_renderer)
    {
        return;
    }

    m_surface->clear(Themes::Disabled);

    m_screenManager->drawCurrentScreen(*m_surface);

    m_renderer->present(m_surface->buffer());
}

bool ApplicationHost::initializeRenderer()
{
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

    return true;
}

bool ApplicationHost::initializeInput()
{
    m_inputManager = std::make_unique<Input::InputManager>();

    return m_inputManager->initialize();
}

void ApplicationHost::handleResize()
{
    if (!m_renderer || !m_surface)
    {
        return;
    }

    if (m_renderer->pollResize())
    {
        const Size previousSize{ m_width, m_height };

        m_width = m_renderer->getConsoleWidth();
        m_height = m_renderer->getConsoleHeight();

        const Size currentSize{ m_width, m_height };

        m_surface->resize(m_width, m_height);

        Input::Event resizeEvent = Input::Event::resize(previousSize, currentSize);
        dispatchEvent(resizeEvent);

        requestRender();
    }
}

