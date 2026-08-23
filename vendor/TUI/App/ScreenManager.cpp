#include "App/ScreenManager.h"

#include "Screens/Screen.h"
#include "Rendering/Surface.h"

#include <utility>

void ScreenManager::pushScreen(std::unique_ptr<Screen> screen)
{
    if (screen)
    {
        screen->onEnter();
        m_screenStack.push_back(std::move(screen));
    }
}

void ScreenManager::popScreen()
{
    if (!m_screenStack.empty())
    {
        m_screenStack.back()->onExit();
        m_screenStack.pop_back();
    }
}

void ScreenManager::clearScreens()
{
    while (!m_screenStack.empty())
    {
        popScreen();
    }
}

bool ScreenManager::hasScreens() const
{
    return !m_screenStack.empty();
}

std::size_t ScreenManager::screenCount() const
{
    return m_screenStack.size();
}

Screen* ScreenManager::currentScreen()
{
    if (m_screenStack.empty())
    {
        return nullptr;
    }

    return m_screenStack.back().get();
}

const Screen* ScreenManager::currentScreen() const
{
    if (m_screenStack.empty())
    {
        return nullptr;
    }

    return m_screenStack.back().get();
}

bool ScreenManager::registerRoute(std::string routeId, ScreenFactory factory)
{
    return m_routeRegistry.registerRoute(std::move(routeId), std::move(factory));
}

bool ScreenManager::unregisterRoute(const std::string& routeId)
{
    return m_routeRegistry.unregisterRoute(routeId);
}

bool ScreenManager::hasRoute(const std::string& routeId) const
{
    return m_routeRegistry.hasRoute(routeId);
}

bool ScreenManager::pushRoute(const std::string& routeId)
{
    std::unique_ptr<Screen> screen = m_routeRegistry.createScreen(routeId);

    if (!screen)
    {
        return false;
    }

    pushScreen(std::move(screen));
    return true;
}

bool ScreenManager::replaceWithRoute(const std::string& routeId)
{
    std::unique_ptr<Screen> screen = m_routeRegistry.createScreen(routeId);

    if (!screen)
    {
        return false;
    }

    clearScreens();
    pushScreen(std::move(screen));
    return true;
}

std::vector<std::string> ScreenManager::registeredRouteIds() const
{
    return m_routeRegistry.registeredRouteIds();
}

bool ScreenManager::dispatchEvent(const Input::Event& event)
{
    Screen* screen = currentScreen();

    if (!screen)
    {
        return false;
    }

    return screen->handleEvent(event);
}

void ScreenManager::update(const Animation::TickEvent& event)
{
    Screen* screen = currentScreen();

    if (screen)
    {
        screen->update(event);
    }
}

void ScreenManager::drawCurrentScreen(Surface& surface)
{
    Screen* screen = currentScreen();

    if (screen)
    {
        screen->draw(surface);
    }
}