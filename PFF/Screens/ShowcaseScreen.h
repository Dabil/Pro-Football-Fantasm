#pragma once

#include "Screens/Screen.h"

class ShowcaseScreen : public Screen
{
public:
    ShowcaseScreen();
    ~ShowcaseScreen() override = default;

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    void drawControlDeck(Surface& surface);
    void drawRetroTerminal(Surface& surface);
    void drawNeonDialog(Surface& surface);
    void drawOpsWall(Surface& surface);

private:
    double m_elapsedSeconds = 0.0;
};