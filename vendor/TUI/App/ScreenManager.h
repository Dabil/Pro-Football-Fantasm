#pragma once

#include <memory>
#include <vector>

#include "Animation/TickEvent.h"

class Screen;
class Surface;

namespace Input
{
    class Event;
}

class ScreenManager
{
public:
    ScreenManager() = default;
    ~ScreenManager() = default;

    void pushScreen(std::unique_ptr<Screen> screen);
    void popScreen();
    void clearScreens();

    bool hasScreens() const;

    Screen* currentScreen();

    bool dispatchEvent(const Input::Event& event);

    void update(const Animation::TickEvent& event);

    void drawCurrentScreen(Surface& surface);

private:
    std::vector<std::unique_ptr<Screen>> m_screenStack;
};