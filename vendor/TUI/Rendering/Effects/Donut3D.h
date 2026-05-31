#pragma once

#include <string>
#include <vector>

#include "Core/Rect.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Effects/IEffects.h"

class Surface;

struct DonutEffectOptions
{
    std::u32string luminanceRamp;
    double elapsedSeconds;

    float rotationXDegrees;
    float rotationYDegrees;
    float rotationZDegrees;

    float rotationXSpeed;
    float rotationYSpeed;
    float rotationZSpeed;

    float scale;
    float thetaSpacing;
    float phiSpacing;
    float tubeRadius;
    float torusRadius;
    float cameraDistance;

    bool showBands;
};

class Donut3D final : public IEffect
{
public:
    explicit Donut3D();
    ~Donut3D() override = default;

    void onEnter() override;
    void onExit() override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface, const Rect& viewPort) override;
    void setOptions(const DonutEffectOptions& options);
    DonutEffectOptions getOptions() const;

private:
    void ensureBuffers(int width, int height);
    void renderDonut(Surface& surface, const Rect& viewPort);
    int index(int x, int y, int width) const;
    Style buildShadedStyle(float normalizedDepth, float luminance) const;

private:
    // options
    std::u32string m_luminanceRamp = U".,-~:;=!*#$@";
    
    double m_elapsedSeconds  = 0.0;
    
    float m_rotationXRadians = 0.0;   // rotation angles in radians
    float m_rotationYRadians = 0.0f;  // rotation angles in radians
    float m_rotationZRadians = 0.0f;  // rotation angles in radians
    
    float m_rotationXSpeed   = 1.15f; 
    float m_rotationYSpeed   = 0.0f;  
    float m_rotationZSpeed   = 0.45f; 

    float m_scale            = 0.4f;  // viewport-relative render scale
    
    float m_thetaSpacing     = 0.06f; // angular sampling step around tube cross-section
    float m_phiSpacing       = 0.02f; // angular sampling step around torus ring
    
    float m_tubeRadius       = 1.0f;  // object-space geometry size for the tube radius
    float m_torusRadius      = 2.0f;  // object-space geometry size for the torus radius
    float m_cameraDistance   = 5.0f;  // z-depth offset from objefct to the viewport camera

    bool m_showBands = false;

    int m_luminanceRampCount = static_cast<int>(m_luminanceRamp.size());
    std::vector<float> m_depthBuffer;
    std::vector<char32_t> m_glyphBuffer;
};

