#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "Rendering/Backends/ConsoleCapabilityDetector.h"
#include "Rendering/Capabilities/RendererCapabilities.h"
#include "Rendering/Diagnostics/RenderDiagnostics.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"
#include "Rendering/Styles/ColorResolutionDiagnostics.h"
#include "Rendering/FrameDiff.h"
#include "Rendering/IRenderer.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StylePolicy.h"
#include "Rendering/Text/TextTypes.h"

#define NOMINMAX
#include <windows.h>

class ConsoleRenderer : public IRenderer
{
public:
    ConsoleRenderer();
    ~ConsoleRenderer() override;

    const RenderDiagnostics* renderDiagnostics() const override
    {
        return &m_renderDiagnostics;
    }

    bool maximizeConsole();

    bool initialize() override;
    void shutdown() override;

    void present(const ScreenBuffer& frame) override;
    void resize(int width, int height) override;
    bool pollResize() override;

    int getConsoleWidth() const override;
    int getConsoleHeight() const override;

    TextBackendCapabilities textCapabilities() const override;

    void setDiagnosticsEnabled(bool enabled);
    bool diagnosticsEnabled() const;

    void setDiagnosticsOutputPath(const std::string& outputPath);
    const std::string& diagnosticsOutputPath() const;

    void setDiagnosticsAppendMode(bool appendMode);
    bool diagnosticsAppendMode() const;

    void setStartupDiagnosticsContext(const StartupDiagnosticsContext& startupDiagnostics);

    RenderDiagnostics& diagnostics();
    const RenderDiagnostics& diagnostics() const;

private:
    void writeFullFrame(const ScreenBuffer& frame);
    void writeDirtySpans(const ScreenBuffer& frame);
    void writeSpan(const ScreenBuffer& frame, int y, int xStart, int xEnd);

    void moveCursor(int x, int y);
    void setStyle(const Style& style);
    void setResolvedStyle(const Style& authoredStyle, const ResolvedStyle& resolvedStyle);
    void resetStyle();

    bool queryVisibleConsoleSize(int& width, int& height) const;
    bool configureConsole();
    void restoreConsoleState();

    void initializeDiagnosticsState();
    void flushDiagnosticsReport() const;

    void recordStyleUsage(const Style& authoredStyle, const ResolvedStyle& resolvedStyle);
    void recordColorFeature(
        StyleFeature feature,
        const std::optional<ColorResolutionDiagnostics>& diagnostics);
    void recordTextFeature(
        StyleFeature feature,
        const Style::AttributeState& authoredState,
        bool presentedEnabled,
        bool physicallyRendered,
        TextAttributeRenderMode renderMode);
    void recordBlinkFeature(
        StyleFeature feature,
        const Style::AttributeState& authoredState,
        bool presentedEnabled,
        bool emulated,
        bool physicallyRendered);

    bool shouldForceFullPresentForBlink(const ScreenBuffer& frame);
    void collectBlinkEmulationUsage(
        const ScreenBuffer& frame,
        bool& usesSlowBlinkEmulation,
        bool& usesFastBlinkEmulation) const;
    bool isBlinkVisibleForResolvedStyle(const ResolvedStyle& resolvedStyle) const;
    bool isBlinkCurrentlyVisible(bool fastBlink) const;

private:
    HANDLE m_hOut = INVALID_HANDLE_VALUE;
    HANDLE m_hIn = INVALID_HANDLE_VALUE;

    int m_consoleWidth = 0;
    int m_consoleHeight = 0;

    ScreenBuffer m_previousFrame;
    bool m_initialized = false;
    bool m_firstPresent = true;

    Style m_currentStyle{};
    StylePolicy m_stylePolicy{};
    RendererCapabilities m_capabilities{};
    RenderDiagnostics m_renderDiagnostics{};
    StartupDiagnosticsContext m_startupDiagnostics{};
    WORD m_defaultAttributes = 0;

    UINT m_originalOutputCodePage = 0;
    UINT m_originalInputCodePage = 0;

    DWORD m_originalOutputMode = 0;
    DWORD m_originalInputMode = 0;

    bool m_haveOriginalOutputMode = false;
    bool m_haveOriginalInputMode = false;
    bool m_cursorWasVisible = true;

    std::string m_rendererIdentity = "WindowsConsoleRenderer";
    bool m_virtualTerminalEnabled = false;
    bool m_virtualTerminalEnableAttempted = false;
    bool m_virtualTerminalEnableSucceeded = false;

    DWORD m_configuredOutputMode = 0;
    DWORD m_configuredInputMode = 0;
    bool m_haveConfiguredOutputMode = false;
    bool m_haveConfiguredInputMode = false;

    std::chrono::steady_clock::time_point m_blinkEpoch{};
    bool m_lastSlowBlinkVisibilityState = true;
    bool m_lastFastBlinkVisibilityState = true;
    bool m_lastFrameUsedBlinkEmulation = false;
};