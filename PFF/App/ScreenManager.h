#pragma once

#include <memory>
#include <vector>

class Screen;
class Surface;

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

    void update(double deltaTime);
    void drawCurrentScreen(Surface& surface);

private:
    std::vector<std::unique_ptr<Screen>> m_screenStack;
};