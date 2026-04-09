#include "Screens/RendererDiagnosticsScreen.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <string>

#include "Core/Rect.h"
#include "Rendering/Capabilities/CapabilityReport.h"
#include "Rendering/Diagnostics/RenderDiagnostics.h"
#include "Rendering/Diagnostics/RendererSelectionTrace.h"
#include "Rendering/IRenderer.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/Surface.h"

namespace
{
    constexpr int MinimumWidth = 120;
    constexpr int MinimumHeight = 34;

    std::string clipText(const std::string& text, int width)
    {
        if (width <= 0)
        {
            return {};
        }

        if (static_cast<int>(text.size()) <= width)
        {
            return text;
        }

        if (width <= 3)
        {
            return text.substr(0, static_cast<std::size_t>(width));
        }

        return text.substr(0, static_cast<std::size_t>(width - 3)) + "...";
    }

    void writeClipped(
        ScreenBuffer& buffer,
        int x,
        int y,
        int width,
        const std::string& text,
        const Style& style)
    {
        if (width <= 0)
        {
            return;
        }

        buffer.writeString(x, y, clipText(text, width), style);
    }

    void drawPanel(ScreenBuffer& buffer, int x, int y, int width, int height, const std::string& title)
    {
        if (width < 4 || height < 3)
        {
            return;
        }

        buffer.drawFrame(
            Rect{ Point{ x, y }, Size{ width, height } },
            Themes::Frame,
            U'┌', U'┐', U'└', U'┘', U'─', U'│');

        writeClipped(buffer, x + 2, y, std::max(0, width - 4), " " + title + " ", Themes::SectionHeader);
    }

    const char* shortFeatureName(StyleFeature feature)
    {
        switch (feature)
        {
        case StyleFeature::ForegroundColor: return "FG";
        case StyleFeature::BackgroundColor: return "BG";
        case StyleFeature::Bold:            return "Bold";
        case StyleFeature::Dim:             return "Dim";
        case StyleFeature::Underline:       return "Under";
        case StyleFeature::SlowBlink:       return "SBlink";
        case StyleFeature::FastBlink:       return "FBlink";
        case StyleFeature::Reverse:         return "Rev";
        case StyleFeature::Invisible:       return "Inv";
        case StyleFeature::Strike:          return "Strike";
        default:                            return "?";
        }
    }
}

RendererDiagnosticsScreen::RendererDiagnosticsScreen(const IRenderer* renderer)
    : m_renderer(renderer)
{
}

void RendererDiagnosticsScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
}

void RendererDiagnosticsScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;
}

void RendererDiagnosticsScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < MinimumWidth || screenHeight < MinimumHeight)
    {
        buffer.writeString(2, 2, "RendererDiagnosticsScreen needs at least 120x34.", Themes::Warning);
        return;
    }

    const RenderDiagnostics* diagnostics = (m_renderer != nullptr)
        ? m_renderer->renderDiagnostics()
        : nullptr;

    if (diagnostics == nullptr)
    {
        buffer.writeString(2, 2, "No renderer diagnostics are available.", Themes::Warning);
        return;
    }

    const CapabilityReport& report = diagnostics->report();

    buffer.fillRect(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        Themes::AccentSurface,
        U'╔', U'╗', U'╚', U'╝', U'═', U'║');

    buffer.writeString(3, 0, "[ Renderer Diagnostics Validation ]", Themes::PanelTitle);
    buffer.writeString(screenWidth - 37, 0, "[ Live counters + adaptation data ]", Themes::Info);

    const int halfWidth = screenWidth / 2;

    drawBackendState(buffer, report, 1, 2, halfWidth - 1, 11);
    drawPerformance(buffer, report, halfWidth, 2, screenWidth - halfWidth - 1, 11);
    drawAdaptationTable(buffer, report, 1, 13, screenWidth - 2, 11);
    drawExamples(buffer, report, 1, 24, screenWidth - 2, screenHeight - 25);
}

void RendererDiagnosticsScreen::drawBackendState(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Startup / Backend State");

    const BackendStateSnapshot& backend = report.backendState();

    writeClipped(buffer, x + 2, y + 1, width - 4, "Configured host:     " + backend.configuredHostPreference, Themes::Text);
    writeClipped(buffer, x + 2, y + 2, width - 4, "Configured renderer: " + backend.configuredRendererPreference, Themes::Text);
    writeClipped(buffer, x + 2, y + 3, width - 4, "Requested host:      " + backend.requestedHostKind, Themes::Text);
    writeClipped(buffer, x + 2, y + 4, width - 4, "Actual host:         " + backend.actualHostKind, Themes::Text);
    writeClipped(buffer, x + 2, y + 5, width - 4, "Requested renderer:  " + backend.requestedRendererIdentity, Themes::Text);
    writeClipped(buffer, x + 2, y + 6, width - 4, "Active renderer:     " + backend.rendererIdentity, Themes::Text);
    writeClipped(buffer, x + 2, y + 7, width - 4, "Render path:         " + backend.activeRenderPath, Themes::Text);
    writeClipped(buffer, x + 2, y + 8, width - 4, "VT enable attempt:   " + std::string(backend.virtualTerminalEnableAttempted ? "true" : "false"), Themes::Text);
    writeClipped(buffer, x + 2, y + 9, width - 4, "VT active output:    " + std::string(backend.activeRenderPathUsesVirtualTerminalOutput ? "true" : "false"), Themes::Text);
}

void RendererDiagnosticsScreen::drawPerformance(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Present Performance");

    const TerminalPresentPerformanceSnapshot& perf = report.terminalPresentPerformance();

    writeClipped(buffer, x + 2, y + 1, width - 4, "Present calls:        " + std::to_string(perf.presentCallCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 2, width - 4, "Skipped presents:     " + std::to_string(perf.skippedPresentCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 3, width - 4, "Full redraw count:    " + std::to_string(perf.fullRedrawCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 4, width - 4, "Diff present count:   " + std::to_string(perf.diffPresentCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 5, width - 4, "Forced full redraws:  " + std::to_string(perf.forcedFullRedrawCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 6, width - 4, "Changed cells:        " + std::to_string(perf.changedCellCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 7, width - 4, "Dirty spans:          " + std::to_string(perf.dirtySpanCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 8, width - 4, "Cursor moves / runs:  " + std::to_string(perf.cursorMoveCount) + " / " + std::to_string(perf.emittedRunCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 9, width - 4, "Last strategy:        " + perf.lastPresentStrategy + " (" + perf.lastPresentReason + ")", Themes::Text);
}

void RendererDiagnosticsScreen::drawAdaptationTable(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Feature Adaptation Counters");

    writeClipped(buffer, x + 2, y + 1, width - 4, "Feature     Direct  Down  Approx  Emul  Omit  Logic", Themes::Focused);

    const std::array<StyleFeature, 10> features =
    { {
        StyleFeature::ForegroundColor,
        StyleFeature::BackgroundColor,
        StyleFeature::Bold,
        StyleFeature::Dim,
        StyleFeature::Underline,
        StyleFeature::SlowBlink,
        StyleFeature::FastBlink,
        StyleFeature::Reverse,
        StyleFeature::Invisible,
        StyleFeature::Strike
    } };

    int rowY = y + 3;
    for (StyleFeature feature : features)
    {
        if (rowY >= y + height - 1)
        {
            break;
        }

        std::ostringstream line;
        std::string name = shortFeatureName(feature);
        while (name.size() < 11)
        {
            name.push_back(' ');
        }

        line
            << name
            << report.getCount(feature, StyleAdaptationKind::Direct) << "       "
            << report.getCount(feature, StyleAdaptationKind::Downgraded) << "     "
            << report.getCount(feature, StyleAdaptationKind::Approximated) << "       "
            << report.getCount(feature, StyleAdaptationKind::Emulated) << "     "
            << report.getCount(feature, StyleAdaptationKind::Omitted) << "     "
            << report.getCount(feature, StyleAdaptationKind::LogicalOnly);

        writeClipped(buffer, x + 2, rowY, width - 4, line.str(), Themes::Text);
        ++rowY;
    }
}

void RendererDiagnosticsScreen::drawExamples(
    ScreenBuffer& buffer,
    const CapabilityReport& report,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Trace / Examples");

    int row = y + 1;

    writeClipped(buffer, x + 2, row++, width - 4, "Renderer selection trace", Themes::SectionHeader);

    const RendererSelectionTrace& trace = report.rendererSelectionTrace();
    if (trace.hasEntries())
    {
        const std::vector<RendererSelectionTraceEntry>& entries = trace.entries();
        const int maxTraceRows = std::min(static_cast<int>(entries.size()), 4);

        for (int i = 0; i < maxTraceRows && row < y + height - 1; ++i)
        {
            const RendererSelectionTraceEntry& entry = entries[static_cast<std::size_t>(i)];

            writeClipped(
                buffer,
                x + 2,
                row++,
                width - 4,
                std::string("[") + RendererSelectionTrace::toString(entry.stage) + "] " +
                entry.decision + " :: " + entry.detail,
                Themes::Text);
        }
    }
    else
    {
        writeClipped(buffer, x + 2, row++, width - 4, "No trace entries recorded.", Themes::MutedText);
    }

    if (row < y + height - 1)
    {
        ++row;
    }

    writeClipped(buffer, x + 2, row++, width - 4, "Recent color adaptation examples", Themes::SectionHeader);

    const std::vector<ColorAdaptationExample>& colorExamples = report.colorAdaptationExamples();
    if (!colorExamples.empty())
    {
        const int maxColorRows = std::min(static_cast<int>(colorExamples.size()), std::max(0, (y + height - 1) - row));

        for (int i = 0; i < maxColorRows; ++i)
        {
            const ColorAdaptationExample& example = colorExamples[static_cast<std::size_t>(i)];

            std::string resolvedText = example.resolvedColor.has_value()
                ? CapabilityReport::formatColor(*example.resolvedColor)
                : std::string("None");

            const std::string line =
                std::string(shortFeatureName(example.feature)) +
                " kind=" + CapabilityReport::toString(example.kind) +
                " tier=" + CapabilityReport::toString(example.supportedTier) +
                " reason=" + CapabilityReport::toString(example.reason) +
                " resolved=" + resolvedText;

            writeClipped(buffer, x + 2, row++, width - 4, line, Themes::Text);

            if (row >= y + height - 1)
            {
                break;
            }
        }
    }
    else
    {
        writeClipped(buffer, x + 2, row++, width - 4, "No color adaptation examples recorded yet.", Themes::MutedText);
    }
}