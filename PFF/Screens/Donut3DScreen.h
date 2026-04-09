// Screens/Donut3DScreen.h
#pragma once

#include <vector>

#include "Screens/Screen.h"

class Surface;
class Style;

class Donut3DScreen : public Screen
{
public:
    Donut3DScreen();
    ~Donut3DScreen() override = default;

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    void ensureBuffers(int width, int height);
    void renderDonut(Surface& surface, int left, int top, int width, int height);

    int index(int x, int y, int width) const;
    Style buildShadedStyle(float normalizedDepth, float luminance) const;

private:
    double m_elapsedSeconds = 0.0;
    float m_rotationA = 0.0f;
    float m_rotationB = 0.0f;

    std::vector<float> m_depthBuffer;
    std::vector<char32_t> m_glyphBuffer;
};