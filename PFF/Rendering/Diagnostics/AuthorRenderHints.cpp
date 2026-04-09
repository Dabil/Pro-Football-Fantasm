#include "Rendering/Diagnostics/AuthorRenderHints.h"

#include <sstream>

namespace
{
    std::size_t countOne(
        const CapabilityReport& report,
        StyleFeature feature,
        StyleAdaptationKind kind)
    {
        return report.getCount(feature, kind);
    }

    std::string makeCountSentence(
        const char* prefix,
        std::size_t count,
        const char* singular,
        const char* plural)
    {
        std::ostringstream stream;
        stream << prefix << count << ' ' << (count == 1 ? singular : plural) << '.';
        return stream.str();
    }

    void addCoreModelExplanation(std::vector<std::string>& hints)
    {
        hints.push_back(
            "Diagnostics separate authored logical style, backend capability, renderer policy, and final backend realization so authors can see where a visual difference was introduced.");
        hints.push_back(
            "Tri-state logical fields now matter in diagnostics: unspecified means the field was absent, explicitly enabled means the author requested it on, and explicitly disabled means the author intentionally forced it off.");
    }

    void addRuntimeEvidenceOverview(
        const CapabilityReport& report,
        std::vector<std::string>& hints)
    {
        if (!report.hasRuntimeData())
        {
            hints.push_back(
                "No runtime adaptation events were recorded during this diagnostics session, so the report mainly reflects static capability and policy state.");
            return;
        }

        std::size_t nonDirectEvents = 0;
        for (const StyleAdaptationCounter& counter : report.counters())
        {
            if (counter.kind != StyleAdaptationKind::Direct)
            {
                nonDirectEvents += counter.count;
            }
        }

        if (nonDirectEvents > 0)
        {
            hints.push_back(
                makeCountSentence(
                    "Runtime diagnostics recorded ",
                    nonDirectEvents,
                    "non-direct adaptation event",
                    "non-direct adaptation events"));
        }
        else
        {
            hints.push_back(
                "Runtime diagnostics recorded only direct realizations for the style features encountered during this session.");
        }
    }

    void addCapabilityAndPathHints(
        const CapabilityReport& report,
        std::vector<std::string>& hints)
    {
        const RendererCapabilities& capabilities = report.capabilities();
        const BackendStateSnapshot& backendState = report.backendState();

        if (capabilities.colorTier == ColorSupport::Basic16)
        {
            hints.push_back(
                "The active backend is operating in Basic16 color mode, so richer authored colors may still be downgraded before final presentation.");
        }
        else if (capabilities.colorTier == ColorSupport::Indexed256)
        {
            hints.push_back(
                "The active backend supports Indexed256 color, but Rgb24-authored values may still be approximated to indexed entries.");
        }

        if (!capabilities.virtualTerminalProcessing)
        {
            hints.push_back(
                "Virtual terminal processing is not active for this backend path, so richer terminal-style effects may still be handled conservatively.");
        }

        if (backendState.virtualTerminalProcessingActive &&
            !backendState.activeRenderPathUsesVirtualTerminalOutput)
        {
            hints.push_back(
                "VT processing was enabled successfully for the console session, but the active renderer path is still the conservative Win32 attribute path rather than a VT output path.");
        }
    }

    void addMaximalFeatureUseHints(
        const CapabilityReport& report,
        std::vector<std::string>& hints)
    {
        const std::size_t directDecorations =
            countOne(report, StyleFeature::Bold, StyleAdaptationKind::Direct) +
            countOne(report, StyleFeature::Dim, StyleAdaptationKind::Direct) +
            countOne(report, StyleFeature::Underline, StyleAdaptationKind::Direct) +
            countOne(report, StyleFeature::Reverse, StyleAdaptationKind::Direct) +
            countOne(report, StyleFeature::Invisible, StyleAdaptationKind::Direct) +
            countOne(report, StyleFeature::Strike, StyleAdaptationKind::Direct);

        const std::size_t approximatedDecorations =
            countOne(report, StyleFeature::Bold, StyleAdaptationKind::Approximated) +
            countOne(report, StyleFeature::Dim, StyleAdaptationKind::Approximated) +
            countOne(report, StyleFeature::Underline, StyleAdaptationKind::Approximated) +
            countOne(report, StyleFeature::Reverse, StyleAdaptationKind::Approximated) +
            countOne(report, StyleFeature::Invisible, StyleAdaptationKind::Approximated) +
            countOne(report, StyleFeature::Strike, StyleAdaptationKind::Approximated);

        if (directDecorations > 0)
        {
            hints.push_back(
                "Some authored style features were realized directly rather than conservatively omitted, which reflects the renderer's shift toward maximal safe feature use when the capability/policy pair allows it.");
        }

        if (approximatedDecorations > 0)
        {
            hints.push_back(
                "Approximation counts indicate features the renderer now tries to preserve visually instead of dropping entirely, while still marking the result as non-direct.");
        }
    }

    void addColorHints(
        const CapabilityReport& report,
        std::vector<std::string>& hints)
    {
        const std::size_t downgraded =
            countOne(report, StyleFeature::ForegroundColor, StyleAdaptationKind::Downgraded) +
            countOne(report, StyleFeature::BackgroundColor, StyleAdaptationKind::Downgraded);

        const std::size_t approximated =
            countOne(report, StyleFeature::ForegroundColor, StyleAdaptationKind::Approximated) +
            countOne(report, StyleFeature::BackgroundColor, StyleAdaptationKind::Approximated);

        const std::size_t logicalOnly =
            countOne(report, StyleFeature::ForegroundColor, StyleAdaptationKind::LogicalOnly) +
            countOne(report, StyleFeature::BackgroundColor, StyleAdaptationKind::LogicalOnly);

        if ((downgraded + approximated + logicalOnly) == 0)
        {
            return;
        }

        std::ostringstream summary;
        summary
            << "Observed color adaptation during rendering: "
            << downgraded << " downgraded, "
            << approximated << " approximated, "
            << logicalOnly << " logical-only color outcome(s).";
        hints.push_back(summary.str());

        hints.push_back(
            "Portable authoring suggestion: prefer semantic theme colors built from standard or bright basic colors where stable cross-backend appearance matters.");
    }

    void addBlinkHints(
        const CapabilityReport& report,
        std::vector<std::string>& hints)
    {
        const std::size_t omitted =
            countOne(report, StyleFeature::SlowBlink, StyleAdaptationKind::Omitted) +
            countOne(report, StyleFeature::FastBlink, StyleAdaptationKind::Omitted);

        const std::size_t emulated =
            countOne(report, StyleFeature::SlowBlink, StyleAdaptationKind::Emulated) +
            countOne(report, StyleFeature::FastBlink, StyleAdaptationKind::Emulated);

        if ((omitted + emulated) == 0)
        {
            return;
        }

        if (emulated > 0)
        {
            hints.push_back(
                "Blink was emulated for at least some rendered content, which means the visible result depends on renderer timing behavior rather than direct backend support.");
        }

        if (omitted > 0)
        {
            hints.push_back(
                "Some authored blink requests were omitted by policy because the current backend/capability combination could not present them safely.");
        }
    }
}

std::vector<std::string> AuthorRenderHints::buildHints(const RenderDiagnostics& diagnostics)
{
    std::vector<std::string> hints;

    const CapabilityReport& report = diagnostics.report();

    addCoreModelExplanation(hints);
    addRuntimeEvidenceOverview(report, hints);
    addCapabilityAndPathHints(report, hints);
    addMaximalFeatureUseHints(report, hints);
    addColorHints(report, hints);
    addBlinkHints(report, hints);

    return hints;
}