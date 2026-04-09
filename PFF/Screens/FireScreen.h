#pragma once

#include <vector>

#include "Screens/Screen.h"

class Surface;
class Style;

class FireScreen : public Screen
{
public:
    FireScreen();
    ~FireScreen() override = default;

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    void ensureSimulationSize(int width, int height);
    void seedFireSource();
    void updateFire(double deltaTime);
    void coolTopRows();

    int index(int x, int y) const;
    char32_t glyphForIntensity(int intensity) const;
    Style styleForIntensity(int intensity) const;

private:
    int m_fireLeft = 0;
    int m_fireTop = 0;
    int m_fireWidth = 0;
    int m_fireHeight = 0;

    double m_elapsedSeconds = 0.0;
    double m_sourceFlickerTimer = 0.0;

    std::vector<int> m_fireBuffer;
    std::vector<int> m_nextFireBuffer;
};