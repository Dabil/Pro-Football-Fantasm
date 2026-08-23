#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Animation/TickEvent.h"
#include "App/ScreenRouteRegistry.h"

class Screen;
class Surface;

namespace Input
{
    class Event;
}

class ScreenManager
{
public:
    using ScreenFactory = ScreenRouteRegistry::ScreenFactory;

    ScreenManager() = default;
    ~ScreenManager() = default;

    void pushScreen(std::unique_ptr<Screen> screen);
    void popScreen();
    void clearScreens();

    bool hasScreens() const;
    std::size_t screenCount() const;

    Screen* currentScreen();
    const Screen* currentScreen() const;

    bool registerRoute(std::string routeId, ScreenFactory factory);
    bool unregisterRoute(const std::string& routeId);
    bool hasRoute(const std::string& routeId) const;

    bool pushRoute(const std::string& routeId);
    bool replaceWithRoute(const std::string& routeId);
    std::vector<std::string> registeredRouteIds() const;

    bool dispatchEvent(const Input::Event& event);

    void update(const Animation::TickEvent& event);

    void drawCurrentScreen(Surface& surface);

private:
    ScreenRouteRegistry m_routeRegistry;
    std::vector<std::unique_ptr<Screen>> m_screenStack;
};