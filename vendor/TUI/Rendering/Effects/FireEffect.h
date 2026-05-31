#pragma once

#include <vector>

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Effects/IEffects.h"

class Surface;

class FireEffect final : public IEffect
{
public:
    explicit FireEffect();
    ~FireEffect() override = default;

    void onEnter() override;
    void onExit() override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface, const Rect& viewport) override;

private:
    void setViewport(const Rect& viewport);
    void ensureSimulationSize(int width, int height);
    void seedFireSource();
    void updateFire(const Animation::TickEvent& event);
    void coolTopRows();
    void renderFireEffect(ScreenBuffer& buffer);

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