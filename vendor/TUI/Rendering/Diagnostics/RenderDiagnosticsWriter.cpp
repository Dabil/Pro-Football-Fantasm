#include "Rendering/Diagnostics/RenderDiagnosticsWriter.h"

#include <fstream>
#include <ios>
#include <sstream>

#include "Rendering/Diagnostics/AuthorRenderHints.h"

namespace
{
    void writeSectionHeader(std::ofstream& out, const char* title)
    {
        out << title << "\n";

        for (const char* p = title; *p != '\0'; ++p)
        {
            out << '=';
        }

        out << "\n\n";
    }

    std::string formatOptionalBackendFlags(std::uint32_t flags)
    {
        if (flags == 0)
        {
            return "None";
        }

        std::ostringstream stream;
        stream << "0x" << std::hex << std::uppercase << flags;
        return stream.str();
    }

    std::string formatModeValue(std::uint32_t mode, bool hasMode)
    {
        if (!hasMode)
        {
            return "Unavailable";
        }

        std::ostringstream stream;
        stream << "0x" << std::hex << std::uppercase << mode;
        return stream.str();
    }

    const char* vtAvailabilityText(const BackendStateSnapshot& backendState)
    {
        if (!backendState.virtualTerminalEnableAttempted)
        {
            return "Not probed";
        }

        return backendState.virtualTerminalEnableSucceeded ?
            "Available (enable attempt succeeded)" :
            "Unavailable on current configuration (enable attempt failed)";
    }

    const char* activePathSummaryText(const BackendStateSnapshot& backendState)
    {
        return backendState.activeRenderPathUsesVirtualTerminalOutput ?
            "VT output path" :
            "Non-VT attribute path";
    }

    const char* featureCapabilityText(const CapabilityReport& report, StyleFeature feature)
    {
        const RendererCapabilities& capabilities = report.capabilities();

        switch (feature)
        {
        case StyleFeature::ForegroundColor:
        case StyleFeature::BackgroundColor:
            return CapabilityReport::toString(capabilities.colorTier);

        case StyleFeature::Bold:
            return CapabilityReport::toString(capabilities.bold);

        case StyleFeature::Dim:
            return CapabilityReport::toString(capabilities.dim);

        case StyleFeature::Underline:
            return CapabilityReport::toString(capabilities.underline);

        case StyleFeature::SlowBlink:
            return CapabilityReport::toString(capabilities.slowBlink);

        case StyleFeature::FastBlink:
            return CapabilityReport::toString(capabilities.fastBlink);

        case StyleFeature::Reverse:
            return CapabilityReport::toString(capabilities.reverse);

        case StyleFeature::Invisible:
            return CapabilityReport::toString(capabilities.invisible);

        case StyleFeature::Strike:
            return CapabilityReport::toString(capabilities.strike);

        default:
            return "Unknown";
        }
    }

    const char* featurePolicyText(const CapabilityReport& report, StyleFeature feature)
    {
        const StylePolicy& policy = report.policy();

        switch (feature)
        {
        case StyleFeature::ForegroundColor:
            return CapabilityReport::toString(policy.rgbColorMode());

        case StyleFeature::BackgroundColor:
            return CapabilityReport::toString(policy.rgbColorMode());

        case StyleFeature::Bold:
            return CapabilityReport::toString(policy.boldMode());

        case StyleFeature::Dim:
            return CapabilityReport::toString(policy.dimMode());

        case StyleFeature::Underline:
            return CapabilityReport::toString(policy.underlineMode());

        case StyleFeature::SlowBlink:
            return CapabilityReport::toString(policy.slowBlinkMode());

        case StyleFeature::FastBlink:
            return CapabilityReport::toString(policy.fastBlinkMode());

        case StyleFeature::Reverse:
            return CapabilityReport::toString(policy.reverseMode());

        case StyleFeature::Invisible:
            return CapabilityReport::toString(policy.invisibleMode());

        case StyleFeature::Strike:
            return CapabilityReport::toString(policy.strikeMode());

        default:
            return "Unknown";
        }
    }

    void writeFeatureReferenceLine(std::ofstream& out, const CapabilityReport& report, StyleFeature feature)
    {
        out
            << CapabilityReport::toString(feature)
            << ": capability=" << featureCapabilityText(report, feature)
            << ", policy=" << featurePolicyText(report, feature)
            << "\n";
    }

    void writeSelectionTrace(std::ofstream& out, const RendererSelectionTrace& trace)
    {
        if (!trace.hasEntries())
        {
            out << "No renderer selection trace was recorded.\n";
            return;
        }

        for (const RendererSelectionTraceEntry& entry : trace.entries())
        {
            out
                << "["
                << RendererSelectionTrace::toString(entry.stage)
                << "] decision=" << entry.decision
                << ", detail=" << entry.detail
                << "\n";
        }
    }

    void writeFeatureOutcomeSummary(std::ofstream& out, const CapabilityReport& report, StyleFeature feature)
    {
        const std::size_t direct = report.getCount(feature, StyleAdaptationKind::Direct);
        const std::size_t approximated = report.getCount(feature, StyleAdaptationKind::Approximated);
        const std::size_t downgraded = report.getCount(feature, StyleAdaptationKind::Downgraded);
        const std::size_t emulated = report.getCount(feature, StyleAdaptationKind::Emulated);
        const std::size_t omitted = report.getCount(feature, StyleAdaptationKind::Omitted);
        const std::size_t logicalOnly = report.getCount(feature, StyleAdaptationKind::LogicalOnly);

        out
            << CapabilityReport::toString(feature)
            << ": emitted-direct=" << direct
            << ", emitted-approximated=" << approximated
            << ", emitted-downgraded=" << downgraded
            << ", emulated=" << emulated
            << ", omitted=" << omitted
            << ", logical-only=" << logicalOnly
            << "\n";
    }

}

bool RenderDiagnosticsWriter::write(const RenderDiagnostics& diagnostics)
{
    if (!diagnostics.isEnabled())
    {
        return true;
    }

    const std::ios::openmode mode =
        std::ios::out |
        (diagnostics.appendMode() ? std::ios::app : std::ios::trunc);

    std::ofstream out(diagnostics.outputPath(), mode);
    if (!out)
    {
        return false;
    }

    const CapabilityReport& report = diagnostics.report();
    const RendererCapabilities& capabilities = report.capabilities();
    const StylePolicy& policy = report.policy();
    const BackendStateSnapshot& backendState = report.backendState();

    writeSectionHeader(out, "Render Diagnostics Report");

    out << "Configuration\n";
    out << "-------------\n";
    out << "Output path: " << diagnostics.outputPath() << "\n";
    out << "Append mode: " << (diagnostics.appendMode() ? "true" : "false") << "\n";
    out << "Diagnostics enabled: " << (diagnostics.isEnabled() ? "true" : "false") << "\n\n";

    out << "Startup Host / Renderer Selection\n";
    out << "---------------------------------\n";
    out << "Startup config source: " << backendState.startupConfigSource << "\n";
    out << "Startup config file found: " << (backendState.startupConfigFileFound ? "true" : "false") << "\n";
    out << "Configured host preference: " << backendState.configuredHostPreference << "\n";
    out << "Configured renderer preference: " << backendState.configuredRendererPreference << "\n";
    out << "Requested host: " << backendState.requestedHostKind << "\n";
    out << "Actual host: " << backendState.actualHostKind << "\n";
    out << "Requested renderer: " << backendState.requestedRendererIdentity << "\n";
    out << "Actual renderer: " << backendState.rendererIdentity << "\n";
    out << "Active render path: " << backendState.activeRenderPath << "\n";
    out << "Launcher relaunch attempted: " << (backendState.relaunchAttempted ? "true" : "false") << "\n";
    out << "Launcher relaunch performed: " << (backendState.relaunchPerformed ? "true" : "false") << "\n";
    out << "Launched-by-WT flag present: " << (backendState.launchedByWindowsTerminalFlag ? "true" : "false") << "\n";
    out << "WT_SESSION hint present: " << (backendState.windowsTerminalSessionHint ? "true" : "false") << "\n\n";

    out << "Renderer Selection Trace\n";
    out << "------------------------\n";
    writeSelectionTrace(out, report.rendererSelectionTrace());
    out << "\n";

    out << "Backend Activation State\n";
    out << "------------------------\n";
    out << "Renderer identity: " << backendState.rendererIdentity << "\n";
    out << "Active render path: " << backendState.activeRenderPath << "\n";
    out << "Active render path kind: " << activePathSummaryText(backendState) << "\n";
    out << "Active render path uses VT output: "
        << (backendState.activeRenderPathUsesVirtualTerminalOutput ? "true" : "false") << "\n";
    out << "VT enable attempted: "
        << (backendState.virtualTerminalEnableAttempted ? "true" : "false") << "\n";
    out << "VT enable succeeded: "
        << (backendState.virtualTerminalEnableSucceeded ? "true" : "false") << "\n";
    out << "VT availability summary: " << vtAvailabilityText(backendState) << "\n";
    out << "VT processing active in console mode: "
        << (backendState.virtualTerminalProcessingActive ? "true" : "false") << "\n";
    out << "Configured output mode: "
        << formatModeValue(backendState.configuredOutputMode, backendState.hasConfiguredOutputMode) << "\n";
    out << "Configured input mode: "
        << formatModeValue(backendState.configuredInputMode, backendState.hasConfiguredInputMode) << "\n";

    if (backendState.virtualTerminalProcessingActive &&
        !backendState.activeRenderPathUsesVirtualTerminalOutput)
    {
        out << "Interpretation: VT processing is active/available at the console mode level, "
            << "but the renderer is still presenting through its conservative non-VT Win32 attribute path.\n";
    }
    else if (!backendState.virtualTerminalProcessingActive &&
        !backendState.activeRenderPathUsesVirtualTerminalOutput)
    {
        out << "Interpretation: VT processing is not active for this session, and the renderer is presenting through its conservative non-VT Win32 attribute path.\n";
    }
    else
    {
        out << "Interpretation: The active renderer/output path matches the VT activation state reported above.\n";
    }

    out << "\n";

    out << "Detected Backend Capabilities\n";
    out << "-----------------------------\n";
    out << "Virtual terminal processing: "
        << (capabilities.virtualTerminalProcessing ? "true" : "false") << "\n";
    out << "Unicode output: "
        << (capabilities.unicodeOutput ? "true" : "false") << "\n";
    out << "Preserve-style-safe fallback: "
        << (capabilities.usesPreserveStyleSafeFallback() ? "true" : "false") << "\n";
    out << "Optional backend flags: "
        << formatOptionalBackendFlags(capabilities.optionalBackendFlags) << "\n";
    out << "Color tier: "
        << CapabilityReport::toString(capabilities.colorTier) << "\n";
    out << "Bright basic colors: "
        << CapabilityReport::toString(capabilities.brightBasicColors) << "\n";
    out << "Bold: "
        << CapabilityReport::toString(capabilities.bold) << "\n";
    out << "Dim: "
        << CapabilityReport::toString(capabilities.dim) << "\n";
    out << "Underline: "
        << CapabilityReport::toString(capabilities.underline) << "\n";
    out << "Reverse: "
        << CapabilityReport::toString(capabilities.reverse) << "\n";
    out << "Invisible: "
        << CapabilityReport::toString(capabilities.invisible) << "\n";
    out << "Strike: "
        << CapabilityReport::toString(capabilities.strike) << "\n";
    out << "Slow blink: "
        << CapabilityReport::toString(capabilities.slowBlink) << "\n";
    out << "Fast blink: "
        << CapabilityReport::toString(capabilities.fastBlink) << "\n\n";

    out << "Resolved Renderer Adaptation Policy\n";
    out << "----------------------------------\n";
    out << "Basic colors: "
        << CapabilityReport::toString(policy.basicColorMode()) << "\n";
    out << "Indexed256 colors: "
        << CapabilityReport::toString(policy.indexed256ColorMode()) << "\n";
    out << "RGB colors: "
        << CapabilityReport::toString(policy.rgbColorMode()) << "\n";
    out << "Bold: "
        << CapabilityReport::toString(policy.boldMode()) << "\n";
    out << "Dim: "
        << CapabilityReport::toString(policy.dimMode()) << "\n";
    out << "Underline: "
        << CapabilityReport::toString(policy.underlineMode()) << "\n";
    out << "Reverse: "
        << CapabilityReport::toString(policy.reverseMode()) << "\n";
    out << "Invisible: "
        << CapabilityReport::toString(policy.invisibleMode()) << "\n";
    out << "Strike: "
        << CapabilityReport::toString(policy.strikeMode()) << "\n";
    out << "Slow blink: "
        << CapabilityReport::toString(policy.slowBlinkMode()) << "\n";
    out << "Fast blink: "
        << CapabilityReport::toString(policy.fastBlinkMode()) << "\n\n";

    out << "Per-Feature Capability/Policy Reference\n";
    out << "--------------------------------------\n";
    writeFeatureReferenceLine(out, report, StyleFeature::ForegroundColor);
    writeFeatureReferenceLine(out, report, StyleFeature::BackgroundColor);
    writeFeatureReferenceLine(out, report, StyleFeature::Bold);
    writeFeatureReferenceLine(out, report, StyleFeature::Dim);
    writeFeatureReferenceLine(out, report, StyleFeature::Underline);
    writeFeatureReferenceLine(out, report, StyleFeature::SlowBlink);
    writeFeatureReferenceLine(out, report, StyleFeature::FastBlink);
    writeFeatureReferenceLine(out, report, StyleFeature::Reverse);
    writeFeatureReferenceLine(out, report, StyleFeature::Invisible);
    writeFeatureReferenceLine(out, report, StyleFeature::Strike);
    out << "\n";

    out << "Adaptation Summary Counts\n";
    out << "-------------------------\n";

    for (const StyleAdaptationCounter& counter : report.counters())
    {
        if (counter.count == 0)
        {
            continue;
        }

        out
            << CapabilityReport::toString(counter.feature)
            << " / "
            << CapabilityReport::toString(counter.kind)
            << ": "
            << counter.count
            << "\n";
    }

    if (!report.hasRuntimeData())
    {
        out << "No runtime adaptation events were recorded.\n";
    }

    out << "\n";

    out << "Observed Output Feature Outcomes\n";
    out << "--------------------------------\n";
    writeFeatureOutcomeSummary(out, report, StyleFeature::ForegroundColor);
    writeFeatureOutcomeSummary(out, report, StyleFeature::BackgroundColor);
    writeFeatureOutcomeSummary(out, report, StyleFeature::Bold);
    writeFeatureOutcomeSummary(out, report, StyleFeature::Dim);
    writeFeatureOutcomeSummary(out, report, StyleFeature::Underline);
    writeFeatureOutcomeSummary(out, report, StyleFeature::SlowBlink);
    writeFeatureOutcomeSummary(out, report, StyleFeature::FastBlink);
    writeFeatureOutcomeSummary(out, report, StyleFeature::Reverse);
    writeFeatureOutcomeSummary(out, report, StyleFeature::Invisible);
    writeFeatureOutcomeSummary(out, report, StyleFeature::Strike);
    out << "\n";

    out << "Totals By Outcome Kind\n";
    out << "----------------------\n";
    out << "Direct: " << report.getTotalCount(StyleAdaptationKind::Direct) << "\n";
    out << "Downgraded: " << report.getTotalCount(StyleAdaptationKind::Downgraded) << "\n";
    out << "Approximated: " << report.getTotalCount(StyleAdaptationKind::Approximated) << "\n";
    out << "Emulated: " << report.getTotalCount(StyleAdaptationKind::Emulated) << "\n";
    out << "Omitted: " << report.getTotalCount(StyleAdaptationKind::Omitted) << "\n";
    out << "LogicalOnly: " << report.getTotalCount(StyleAdaptationKind::LogicalOnly) << "\n";
    out << "\n";

    out << "Color Adaptation Decisions\n";
    out << "--------------------------\n";

    if (report.colorAdaptationExamples().empty())
    {
        out << "No color adaptation examples recorded.\n";
    }
    else
    {
        for (const ColorAdaptationExample& example : report.colorAdaptationExamples())
        {
            out
                << "["
                << CapabilityReport::toString(example.feature)
                << " / "
                << CapabilityReport::toString(example.kind)
                << "] authored=" << CapabilityReport::formatAuthoredColor(example.authoredColor)
                << ", supported tier=" << CapabilityReport::toString(example.supportedTier)
                << ", resolved=";

            if (example.resolvedColor.has_value())
            {
                out << CapabilityReport::formatColor(*example.resolvedColor);
            }
            else
            {
                out << "Omitted";
            }

            out
                << ", reason=" << CapabilityReport::toString(example.reason)
                << "\n";
        }
    }

    out << "\n";

    out << "Tri-State Logical Style Examples\n";
    out << "--------------------------------\n";
    out << "Legend: Unspecified = field absent in authored style, ExplicitlyEnabled = field authored true, ExplicitlyDisabled = field authored false.\n";

    if (report.logicalStateExamples().empty())
    {
        out << "No tri-state logical style examples recorded.\n";
    }
    else
    {
        for (const StyleLogicalStateExample& example : report.logicalStateExamples())
        {
            out
                << "["
                << CapabilityReport::toString(example.feature)
                << " / logical="
                << CapabilityReport::toString(example.logicalState)
                << "] "
                << example.detail
                << "\n";
        }
    }

    out << "\n";

    out << "Representative Examples\n";
    out << "-----------------------\n";

    if (report.examples().empty())
    {
        out << "No examples recorded.\n";
    }
    else
    {
        for (const StyleAdaptationExample& example : report.examples())
        {
            out
                << "["
                << CapabilityReport::toString(example.feature)
                << " / "
                << CapabilityReport::toString(example.kind)
                << " / logical="
                << CapabilityReport::toString(example.logicalState)
                << "] capability=" << featureCapabilityText(report, example.feature)
                << ", policy=" << featurePolicyText(report, example.feature)
                << ", detail=" << example.detail
                << "\n";
        }
    }

    out << "\n";

    out << "Author-Facing Rendering Hints\n";
    out << "----------------------------\n";

    const std::vector<std::string> hints = AuthorRenderHints::buildHints(diagnostics);

    if (hints.empty())
    {
        out << "No author-facing hints.\n";
    }
    else
    {
        for (const std::string& hint : hints)
        {
            out << "- " << hint << "\n";
        }
    }

    out << "\n";

    out << "Notes\n";
    out << "-----\n";
    out << "- Diagnostics describe renderer behavior only.\n";
    out << "- Logical Style data stored in ScreenBuffer is not mutated by diagnostics or adaptation.\n";
    out << "- Tri-state logical reporting exists to distinguish absent fields from explicit enable/disable intent.\n";
    out << "- Host selection and renderer selection are reported separately on purpose.\n";
    out << "- VT enablement/availability and active renderer path are reported separately on purpose.\n";
    out << "- Output differences between authored style and visible result may be caused by downgrade, omission, approximation, or deferred emulation.\n";
    out << "- Color adaptation reasons are reported from ColorResolver decisions rather than recreated in the renderer.\n";
    out << "- Bright basic color capability describes palette/intensity color presentation, not bold or dim text semantics.\n";
    out << "- Author-facing hints are advisory only and do not change rendering behavior.\n";

    out << "VT Present Performance\n";
    out << "----------------------\n";

    const auto& vt = report.terminalPresentPerformance();

    out << "Present calls: " << vt.presentCallCount << "\n";
    out << "Skipped presents: " << vt.skippedPresentCount << "\n";
    out << "Full redraw presents: " << vt.fullRedrawCount << "\n";
    out << "Diff presents: " << vt.diffPresentCount << "\n";

    out << "Changed cells: " << vt.changedCellCount << "\n";
    out << "Dirty spans: " << vt.dirtySpanCount << "\n";

    out << "Cursor moves: " << vt.cursorMoveCount << "\n";
    out << "Runs: " << vt.emittedRunCount << "\n";

    out << "Text bytes: " << vt.emittedTextBytes << "\n";
    out << "SGR bytes: " << vt.emittedSgrBytes << "\n";
    out << "Control bytes: " << vt.emittedControlBytes << "\n";
    out << "Total bytes: " << vt.emittedTotalBytes << "\n";

    out << "Last present strategy: " << vt.lastPresentStrategy << "\n";
    out << "Last present reason: " << vt.lastPresentReason << "\n\n";

    return true;
}