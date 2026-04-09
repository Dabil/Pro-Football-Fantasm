#pragma once

class Surface;

/*
    Base interface for all screens in the TUI.

    Screens represent high-level application states such as:
    - Title Screen
    - Main Menu
    - Game Screen
    - Pause Screen
*/
class Screen
{
public:
    virtual ~Screen() = default;

    // Called once when the screen becomes active
    virtual void onEnter() {}

    // Called once when the screen is removed
    virtual void onExit() {}

    // Called every frame
    virtual void update(double deltaTime) {}

    // Called every frame to render the screen
    virtual void draw(Surface& surface) = 0;
};