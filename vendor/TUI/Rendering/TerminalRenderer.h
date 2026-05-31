#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <chrono>
#include <vector>

#include "Rendering/Backends/TerminalCapabilityDetector.h"
#include "Rendering/Capabilities/RendererCapabilities.h"
#include "Rendering/Diagnostics/RenderDiagnostics.h"
#include "Rendering/Diagnostics/StartupDiagnosticsContext.h"
#include "Rendering/FrameDiff.h"
#include "Rendering/IRenderer.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/SgrEmitter.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StylePolicy.h"
#include "Rendering/TerminalPresentPolicy.h"
#include "Rendering/TerminalSessionOptions.h"
#include "Rendering/Text/TextTypes.h"
#include "Rendering/VtFrameEmitter.h"
#include "Rendering/VTRun.h"

#define NOMINMAX
#include <windows.h>

class TerminalRenderer : public IRenderer
{
public:
    TerminalRenderer();
    ~TerminalRenderer() override;

    const RenderDiagnostics* renderDiagnostics() const override
    {
        return &m_renderDiagnostics;
    }

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

    void setSessionOptions(const TerminalSessionOptions& options);
    const TerminalSessionOptions& sessionOptions() const;

    RenderDiagnostics& diagnostics();
    const RenderDiagnostics& diagnostics() const;

private:
    VtFrameEmitterStats writeFullFrame(const ScreenBuffer& frame, bool clearScreenFirst);
    VtFrameEmitterStats writeDirtySpans(const ScreenBuffer& frame, const std::vector<DirtySpan>& spans);

    void appendSpanRuns(
        VtFrameEmitter& emitter,
        const ScreenBuffer& frame,
        int y,
        int xStart,
        int xEnd);

    VtRun buildRun(
        const ScreenBuffer& frame,
        int y,
        int xStart,
        int xEnd,
        int& nextX);

    bool supportsUnicodeClusterEmission() const;

    void resetStyle();

    bool queryVisibleConsoleSize(int& width, int& height) const;
    bool configureTerminal();
    void restoreTerminalState();

    bool enterTerminalSession();
    void leaveTerminalSession();

    void initializeDiagnosticsState();
    void flushDiagnosticsReport() const;

    void writeRaw(std::string_view text);
    void clearScreen();

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

    void recordPresentPerformance(
        const TerminalPresentDecision& decision,
        const TerminalPresentMetrics& metrics,
        const VtFrameEmitterStats& emitterStats,
        bool skippedPresent);

    std::u32string maskBlinkHiddenCellText(
        const std::u32string& clusterText,
        CellWidth width) const;

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

    SgrEmitter m_sgrEmitter{};
    StylePolicy m_stylePolicy{};
    RendererCapabilities m_capabilities{};
    RenderDiagnostics m_renderDiagnostics{};
    TerminalPresentPolicy m_presentPolicy{};
    StartupDiagnosticsContext m_startupDiagnostics{};
    TerminalSessionOptions m_sessionOptions = TerminalSessionOptions::FullScreenDefaults();

    UINT m_originalOutputCodePage = 0;
    UINT m_originalInputCodePage = 0;

    DWORD m_originalOutputMode = 0;
    DWORD m_originalInputMode = 0;

    bool m_haveOriginalOutputMode = false;
    bool m_haveOriginalInputMode = false;
    bool m_cursorWasVisible = true;

    std::string m_rendererIdentity = "TerminalRenderer";
    bool m_virtualTerminalEnabled = false;
    bool m_virtualTerminalEnableAttempted = false;
    bool m_virtualTerminalEnableSucceeded = false;

    DWORD m_configuredOutputMode = 0;
    DWORD m_configuredInputMode = 0;
    bool m_haveConfiguredOutputMode = false;
    bool m_haveConfiguredInputMode = false;

    bool m_terminalSessionActive = false;
    bool m_alternateScreenActive = false;
    bool m_cursorHiddenBySession = false;

    std::chrono::steady_clock::time_point m_blinkEpoch{};
    bool m_lastSlowBlinkVisibilityState = true;
    bool m_lastFastBlinkVisibilityState = true;
    bool m_lastFrameUsedBlinkEmulation = false;
};