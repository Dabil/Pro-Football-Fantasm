#pragma once

#include <memory>

#include "Input/RawKeyboardSource.h"
#include "Rendering/Diagnostics/RenderDiagnostics.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"
#include "Rendering/IRenderer.h"

class SdlRenderer final : public IRenderer
{
public:
    explicit SdlRenderer(const StartupDiagnosticsContext& startup);
    ~SdlRenderer() override;

    bool initialize() override;
    void shutdown() override;
    void present(const ScreenBuffer& buffer) override;
    void resize(int width, int height) override;
    bool pollResize() override;
    int getConsoleWidth() const override;
    int getConsoleHeight() const override;
    TextBackendCapabilities textCapabilities() const override;
    const RenderDiagnostics* renderDiagnostics() const override;

    std::unique_ptr<Input::IRawKeyboardSource> createKeyboardSource();
    bool closeRequested() const;
    void setTextInputCaret(std::optional<Point> position);

private:
    class KeyboardSource;
    struct State;

    void reportStartup(const std::string& error);

    StartupDiagnosticsContext m_startup;
    RenderDiagnostics m_diagnostics;
    std::unique_ptr<State> m_state;
};
