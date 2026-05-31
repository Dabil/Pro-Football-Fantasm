#pragma once

#include <cstdint>
#include <memory>

#include "App/TerminalLauncher.h"
#include "Assets/AssetLibrary.h"
#include "Animation/TickClock.h"
#include "Animation/TickEvent.h"
#include "Input/CommandMap.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"

class IRenderer;
class ScreenManager;
class Surface;

namespace Input
{
    class Event;
    struct CommandEvent;

    class InputManager;
}

class ApplicationHost
{
public:
    explicit ApplicationHost(
        StartupRendererSelection rendererSelection,
        StartupValidationScreenPreference validationScreenSelection,
        const StartupDiagnosticsContext& startupDiagnostics);

    virtual ~ApplicationHost();

    ApplicationHost(const ApplicationHost&) = delete;
    ApplicationHost& operator=(const ApplicationHost&) = delete;

    bool initialize();
    void run();
    void shutdown();

    bool isRunning() const;

protected:
    ScreenManager& screenManager();
    const ScreenManager& screenManager() const;

    IRenderer& renderer();
    const IRenderer& renderer() const;

    Surface& surface();
    const Surface& surface() const;

    Assets::AssetLibrary& assetLibrary();
    const Assets::AssetLibrary& assetLibrary() const;

    const StartupDiagnosticsContext& startupDiagnostics() const;

    int width() const;
    int height() const;
    std::uint64_t frameIndex() const;

    void requestShutdown();
    void requestRender();

protected:
    virtual void configureAssetLibrary();
    virtual bool onInitialize();
    virtual void onShutdown();

    virtual bool dispatchEvent(const Input::Event& event);
    virtual bool dispatchToActiveScreen(const Input::Event& event);
    virtual bool handleApplicationCommand(const Input::CommandEvent& commandEvent);

    virtual void update(const Animation::TickEvent& event);
    virtual void render();

private:
    bool initializeRenderer();
    bool initializeInput();

    void handleResize();

private:
    StartupRendererSelection m_rendererSelection = StartupRendererSelection::Console;
    StartupValidationScreenPreference m_validationScreenStart = StartupValidationScreenPreference::ValidationStartFalse;
    StartupDiagnosticsContext m_startupDiagnostics{};

    std::unique_ptr<ScreenManager> m_screenManager;
    std::unique_ptr<IRenderer> m_renderer;
    std::unique_ptr<Surface> m_surface;
    std::unique_ptr<Input::InputManager> m_inputManager;

    Input::CommandMap m_commandMap;
    Assets::AssetLibrary m_assetLibrary;

    Animation::TickClock m_tickClock;

    bool m_running = false;
    bool m_renderRequested = true;

    int m_width = 0;
    int m_height = 0;

    std::uint64_t m_frameIndex = 0;
};

