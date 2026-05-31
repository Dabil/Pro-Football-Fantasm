#include "App/ScreenManager.h"

#include "Screens/Screen.h"
#include "Rendering/Surface.h"

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

Screen* ScreenManager::currentScreen()
{
    if (m_screenStack.empty())
    {
        return nullptr;
    }

    return m_screenStack.back().get();
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
