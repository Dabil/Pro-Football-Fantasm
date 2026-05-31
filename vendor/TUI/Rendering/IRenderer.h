#pragma once

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Text/TextTypes.h"

class RenderDiagnostics;

class IRenderer
{
public:
    virtual ~IRenderer() = default;

    virtual bool initialize() = 0;
    virtual void shutdown() = 0;

    virtual void present(const ScreenBuffer& buffer) = 0;

    virtual void resize(int width, int height) = 0;
    virtual bool pollResize() = 0;

    virtual int getConsoleWidth() const = 0;
    virtual int getConsoleHeight() const = 0;

    virtual TextBackendCapabilities textCapabilities() const = 0;

    virtual const RenderDiagnostics* renderDiagnostics() const = 0;
};