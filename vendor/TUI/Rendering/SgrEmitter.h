#pragma once

#include <string>

#include "Rendering/Capabilities/RendererCapabilities.h"
#include "Rendering/Styles/Style.h"

class SgrEmitter
{
public:
    SgrEmitter() = default;

    void setCapabilities(const RendererCapabilities& capabilities);

    void reset();
    const Style& currentStyle() const;

    void appendTransitionTo(std::string& out, const Style& targetStyle);
    void appendReset(std::string& out);

private:
    Style sanitizeForEmission(const Style& style) const;

    void appendFullStyle(std::string& out, const Style& style) const;
    void appendDeltaStyle(std::string& out, const Style& targetStyle) const;

private:
    RendererCapabilities m_capabilities = RendererCapabilities::Conservative();
    Style m_currentStyle{};
};