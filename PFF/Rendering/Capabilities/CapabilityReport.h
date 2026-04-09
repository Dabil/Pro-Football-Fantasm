#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "Rendering/Capabilities/RendererCapabilities.h"
#include "Rendering/Diagnostics/RendererSelectionTrace.h"
#include "Rendering/Styles/ColorResolutionDiagnostics.h"
#include "Rendering/Styles/StylePolicy.h"

enum class StyleAdaptationKind
{
    Direct,
    Downgraded,
    Approximated,
    Omitted,
    Emulated,
    LogicalOnly
};

enum class StyleFeature
{
    ForegroundColor,
    BackgroundColor,
    Bold,
    Dim,
    Underline,
    SlowBlink,
    FastBlink,
    Reverse,
    Invisible,
    Strike
};

enum class LogicalStyleValueState
{
    Unspecified,
    ExplicitlyEnabled,
    ExplicitlyDisabled
};

struct StyleAdaptationCounter
{
    StyleFeature feature = StyleFeature::ForegroundColor;
    StyleAdaptationKind kind = StyleAdaptationKind::Direct;
    std::size_t count = 0;
};

struct StyleAdaptationExample
{
    StyleFeature feature = StyleFeature::ForegroundColor;
    StyleAdaptationKind kind = StyleAdaptationKind::Direct;
    LogicalStyleValueState logicalState = LogicalStyleValueState::Unspecified;
    std::string detail;
};

struct StyleLogicalStateExample
{
    StyleFeature feature = StyleFeature::ForegroundColor;
    LogicalStyleValueState logicalState = LogicalStyleValueState::Unspecified;
    std::string detail;
};

struct ColorAdaptationExample
{
    StyleFeature feature = StyleFeature::ForegroundColor;
    StyleAdaptationKind kind = StyleAdaptationKind::Direct;
    ColorSupport supportedTier = ColorSupport::None;
    ColorAdaptationReason reason = ColorAdaptationReason::OmittedNoColorSupport;
    Style::StyleColorValue authoredColor;
    std::optional<Color> resolvedColor;
};


struct TerminalPresentPerformanceSnapshot
{
    std::size_t presentCallCount = 0;
    std::size_t skippedPresentCount = 0;
    std::size_t fullRedrawCount = 0;
    std::size_t diffPresentCount = 0;
    std::size_t forcedFullRedrawCount = 0;

    std::size_t changedCellCount = 0;
    std::size_t dirtySpanCount = 0;
    std::size_t cursorMoveCount = 0;
    std::size_t emittedRunCount = 0;

    std::size_t emittedTextBytes = 0;
    std::size_t emittedSgrBytes = 0;
    std::size_t emittedControlBytes = 0;
    std::size_t emittedTotalBytes = 0;

    std::string lastPresentStrategy = "Unknown";
    std::string lastPresentReason;
};

struct BackendStateSnapshot
{
    std::string startupConfigSource = "startup.config";
    bool startupConfigFileFound = false;

    std::string configuredHostPreference = "Auto";
    std::string configuredRendererPreference = "Auto";

    std::string requestedHostKind = "Unknown";
    std::string actualHostKind = "Unknown";

    std::string requestedRendererIdentity = "Unknown";
    std::string rendererIdentity = "Unknown";

    std::string activeRenderPath = "Unknown";

    bool relaunchAttempted = false;
    bool relaunchPerformed = false;

    bool launchedByWindowsTerminalFlag = false;
    bool windowsTerminalSessionHint = false;

    bool virtualTerminalEnableAttempted = false;
    bool virtualTerminalEnableSucceeded = false;
    bool virtualTerminalProcessingActive = false;
    bool activeRenderPathUsesVirtualTerminalOutput = false;

    std::uint32_t configuredOutputMode = 0;
    std::uint32_t configuredInputMode = 0;

    bool hasConfiguredOutputMode = false;
    bool hasConfiguredInputMode = false;
};

class CapabilityReport
{
public:
    CapabilityReport();

    void setCapabilities(const RendererCapabilities& capabilities);
    void setPolicy(const StylePolicy& policy);
    void setBackendState(const BackendStateSnapshot& backendState);
    void setRendererSelectionTrace(const RendererSelectionTrace& selectionTrace);

    const RendererCapabilities& capabilities() const;
    const StylePolicy& policy() const;
    const BackendStateSnapshot& backendState() const;
    const RendererSelectionTrace& rendererSelectionTrace() const;

    void setTerminalPresentPerformance(const TerminalPresentPerformanceSnapshot& performance);
    const TerminalPresentPerformanceSnapshot& terminalPresentPerformance() const;

    void recordDirect(StyleFeature feature);
    void recordDowngraded(StyleFeature feature);
    void recordApproximated(StyleFeature feature);
    void recordOmitted(StyleFeature feature);
    void recordEmulated(StyleFeature feature);
    void recordLogicalOnly(StyleFeature feature);

    void addExample(
        StyleFeature feature,
        StyleAdaptationKind kind,
        const std::string& detail,
        LogicalStyleValueState logicalState = LogicalStyleValueState::Unspecified);

    void addLogicalStateExample(
        StyleFeature feature,
        LogicalStyleValueState logicalState,
        const std::string& detail);

    void addColorAdaptationExample(
        StyleFeature feature,
        StyleAdaptationKind kind,
        const ColorResolutionDiagnostics& diagnostics);

    std::size_t getCount(StyleFeature feature, StyleAdaptationKind kind) const;
    std::size_t getTotalCount(StyleAdaptationKind kind) const;

    const std::vector<StyleAdaptationCounter>& counters() const;
    const std::vector<StyleAdaptationExample>& examples() const;
    const std::vector<StyleLogicalStateExample>& logicalStateExamples() const;
    const std::vector<ColorAdaptationExample>& colorAdaptationExamples() const;

    void clearRuntimeData();
    bool hasRuntimeData() const;

    static const char* toString(ColorSupport support);
    static const char* toString(RendererFeatureSupport support);
    static const char* toString(ColorRenderMode mode);
    static const char* toString(TextAttributeRenderMode mode);
    static const char* toString(BlinkRenderMode mode);
    static const char* toString(StyleFeature feature);
    static const char* toString(StyleAdaptationKind kind);
    static const char* toString(LogicalStyleValueState state);
    static const char* toString(ColorAdaptationReason reason);

    static std::string formatColor(const Color& color);
    static std::string formatAuthoredColor(const Style::StyleColorValue& colorValue);

private:
    void increment(StyleFeature feature, StyleAdaptationKind kind);

private:
    RendererCapabilities m_capabilities{};
    StylePolicy m_policy{};
    BackendStateSnapshot m_backendState{};
    RendererSelectionTrace m_rendererSelectionTrace{};
    TerminalPresentPerformanceSnapshot m_terminalPresentPerformance{};

    std::vector<StyleAdaptationCounter> m_counters;
    std::vector<StyleAdaptationExample> m_examples;
    std::vector<StyleLogicalStateExample> m_logicalStateExamples;
    std::vector<ColorAdaptationExample> m_colorAdaptationExamples;
};