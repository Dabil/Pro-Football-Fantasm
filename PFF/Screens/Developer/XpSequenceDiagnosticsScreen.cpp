#include "Screens/Developer/XpSequenceDiagnosticsScreen.h"

#include <algorithm>
#include <string>

#include "Core/Rect.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/Surface.h"

namespace
{
    constexpr int MinimumWidth = 120;
    constexpr int MinimumHeight = 36;

    std::string clipText(const std::string& text, const int width)
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
        const int x,
        const int y,
        const int width,
        const std::string& text,
        const Style& style)
    {
        if (width <= 0)
        {
            return;
        }

        buffer.writeString(x, y, clipText(text, width), style);
    }

    void drawPanel(
        ScreenBuffer& buffer,
        const int x,
        const int y,
        const int width,
        const int height,
        const std::string& title)
    {
        if (width < 4 || height < 3)
        {
            return;
        }

        buffer.drawFrame(
            Rect{ Point{ x, y }, Size{ width, height } },
            Themes::Frame,
            U'┌', U'┐', U'└', U'┘', U'─', U'│');

        writeClipped(
            buffer,
            x + 2,
            y,
            std::max(0, width - 4),
            " " + title + " ",
            Themes::SectionHeader);
    }
}

XpSequenceDiagnosticsScreen::XpSequenceDiagnosticsScreen(
    const XpSequenceInspection::InspectionReport& report)
    : m_report(report)
{
}

void XpSequenceDiagnosticsScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
}

void XpSequenceDiagnosticsScreen::update(const double deltaTime)
{
    m_elapsedSeconds += deltaTime;
}

void XpSequenceDiagnosticsScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < MinimumWidth || screenHeight < MinimumHeight)
    {
        buffer.writeString(2, 2, "XpSequenceDiagnosticsScreen needs at least 120x36.", Themes::Warning);
        return;
    }

    buffer.fillRect(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        Themes::AccentSurface,
        U'╔', U'╗', U'╚', U'╝', U'═', U'║');

    buffer.writeString(3, 0, "[ XP Sequence Diagnostics ]", Themes::PanelTitle);
    buffer.writeString(screenWidth - 40, 0, "[ manifest-first inspection ]", Themes::Info);

    const int halfWidth = screenWidth / 2;
    drawSummaryPanel(buffer, 1, 2, halfWidth - 1, 10);
    drawManifestPanel(buffer, halfWidth, 2, screenWidth - halfWidth - 1, 10);
    drawPlaybackPanel(buffer, 1, 12, halfWidth - 1, 7);
    drawDiagnosticsPanel(buffer, halfWidth, 12, screenWidth - halfWidth - 1, 7);
    drawFramesPanel(buffer, 1, 19, halfWidth - 1, screenHeight - 20);
    drawRetainedDocumentPanel(buffer, halfWidth, 19, screenWidth - halfWidth - 1, screenHeight - 20);
}

void XpSequenceDiagnosticsScreen::drawSummaryPanel(
    ScreenBuffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height) const
{
    drawPanel(buffer, x, y, width, height, "Sequence Summary");

    writeClipped(buffer, x + 2, y + 1, width - 4, XpSequenceInspection::formatInspectionSummary(m_report), Themes::Text);
    writeClipped(buffer, x + 2, y + 3, width - 4, "Valid: " + std::string(m_report.sequenceValid ? "true" : "false"), Themes::Text);
    writeClipped(buffer, x + 2, y + 4, width - 4, "Frame count: " + std::to_string(m_report.frameCount), Themes::Text);
    writeClipped(buffer, x + 2, y + 5, width - 4, "Unique indices: " + std::string(m_report.hasUniqueFrameIndices ? "true" : "false"), Themes::Text);
    writeClipped(buffer, x + 2, y + 6, width - 4, "Contiguous from zero: " + std::string(m_report.hasContiguousFrameIndicesStartingAtZero ? "true" : "false"), Themes::Text);
    writeClipped(buffer, x + 2, y + 7, width - 4, "Stored in frame-index order: " + std::string(m_report.framesStoredInFrameIndexOrder ? "true" : "false"), Themes::Text);
    writeClipped(buffer, x + 2, y + 8, width - 4, XpSequenceInspection::formatOverrideSummary(m_report), Themes::Info);
}

void XpSequenceDiagnosticsScreen::drawManifestPanel(
    ScreenBuffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height) const
{
    drawPanel(buffer, x, y, width, height, "Manifest Fields / Sources");

    writeClipped(buffer, x + 2, y + 1, width - 4, XpSequenceInspection::formatManifestFieldSummary(m_report), Themes::Text);
    writeClipped(buffer, x + 2, y + 3, width - 4, XpSequenceInspection::formatSourceResolutionSummary(m_report), Themes::Info);
    writeClipped(buffer, x + 2, y + 5, width - 4, "Manifest path: " + m_report.manifestPath, Themes::Text);
    writeClipped(buffer, x + 2, y + 6, width - 4, "Manifest dir:  " + m_report.manifestDirectory, Themes::Text);
    writeClipped(
        buffer,
        x + 2,
        y + 7,
        width - 4,
        "Load diagnostics: " + std::to_string(m_report.loadDiagnostics.size()),
        Themes::Text);
    writeClipped(
        buffer,
        x + 2,
        y + 8,
        width - 4,
        "Validation diagnostics: " + std::to_string(m_report.validationDiagnostics.size()),
        Themes::Text);
}

void XpSequenceDiagnosticsScreen::drawPlaybackPanel(
    ScreenBuffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height) const
{
    drawPanel(buffer, x, y, width, height, "Future Playback Hooks");

    writeClipped(buffer, x + 2, y + 1, width - 4, XpSequenceInspection::formatPlaybackHookSummary(m_report), Themes::Text);
    writeClipped(buffer, x + 2, y + 3, width - 4, "This metadata is inspection-only and does not schedule, interpolate, or render frames.", Themes::Info);
    writeClipped(buffer, x + 2, y + 4, width - 4, "Use it later to seed animation state while preserving XpSequence -> XpFrame -> TextObject.", Themes::Info);
}

void XpSequenceDiagnosticsScreen::drawDiagnosticsPanel(
    ScreenBuffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height) const
{
    drawPanel(buffer, x, y, width, height, "Diagnostic Snapshot");

    int rowY = y + 1;
    const int maxRowY = y + height - 2;

    for (const XpSequenceLoader::Diagnostic& diagnostic : m_report.loadDiagnostics)
    {
        if (rowY > maxRowY)
        {
            break;
        }

        writeClipped(
            buffer,
            x + 2,
            rowY,
            width - 4,
            std::string(diagnostic.isError ? "ERR " : "WRN ") + diagnostic.message,
            diagnostic.isError ? Themes::Warning : Themes::Info);
        ++rowY;
    }

    for (const XpSequenceValidation::Diagnostic& diagnostic : m_report.validationDiagnostics)
    {
        if (rowY > maxRowY)
        {
            break;
        }

        writeClipped(
            buffer,
            x + 2,
            rowY,
            width - 4,
            std::string(diagnostic.isError ? "ERR " : "WRN ") + diagnostic.message,
            diagnostic.isError ? Themes::Warning : Themes::Info);
        ++rowY;
    }

    if (rowY == y + 1)
    {
        writeClipped(buffer, x + 2, rowY, width - 4, "No loader or validation diagnostics were supplied.", Themes::Text);
    }
}

void XpSequenceDiagnosticsScreen::drawFramesPanel(
    ScreenBuffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height) const
{
    drawPanel(buffer, x, y, width, height, "Frames / Resolved Overrides");

    int rowY = y + 1;
    const int maxRowY = y + height - 2;

    for (int ordinal = 0; ordinal < static_cast<int>(m_report.frames.size()); ++ordinal)
    {
        if (rowY > maxRowY)
        {
            break;
        }

        const XpSequenceInspection::FrameInspection& frame = m_report.frames[ordinal];
        writeClipped(
            buffer,
            x + 2,
            rowY,
            width - 4,
            XpSequenceInspection::formatFrameSummary(m_report, ordinal),
            Themes::Text);
        ++rowY;

        if (rowY > maxRowY)
        {
            break;
        }

        writeClipped(
            buffer,
            x + 4,
            rowY,
            width - 6,
            "resolvedVisibleLayers=" + std::to_string(frame.resolvedVisibleLayerCount)
            + ", resolvedHiddenLayers=" + std::to_string(frame.resolvedHiddenLayerCount)
            + ", hasOverrides=" + std::string(frame.hasAnyRetainedOverride() ? "true" : "false"),
            Themes::Info);
        ++rowY;
    }

    if (m_report.frames.empty())
    {
        writeClipped(buffer, x + 2, rowY, width - 4, "No retained frames are available for inspection.", Themes::Warning);
    }
}

void XpSequenceDiagnosticsScreen::drawRetainedDocumentPanel(
    ScreenBuffer& buffer,
    const int x,
    const int y,
    const int width,
    const int height) const
{
    drawPanel(buffer, x, y, width, height, "Retained XP Details (Initial Frame)");

    int rowY = y + 1;
    const int maxRowY = y + height - 2;

    if (m_report.frames.empty())
    {
        writeClipped(buffer, x + 2, rowY, width - 4, "No retained frame details are available.", Themes::Warning);
        return;
    }

    const XpSequenceInspection::FrameInspection* selectedFrame = nullptr;
    for (const XpSequenceInspection::FrameInspection& frame : m_report.frames)
    {
        if (frame.frameIndex == m_report.playbackMetadata.initialFrameIndex)
        {
            selectedFrame = &frame;
            break;
        }
    }

    if (selectedFrame == nullptr)
    {
        selectedFrame = &m_report.frames.front();
    }

    writeClipped(
        buffer,
        x + 2,
        rowY,
        width - 4,
        "Frame " + std::to_string(selectedFrame->ordinal)
        + " / frameIndex=" + std::to_string(selectedFrame->frameIndex),
        Themes::SectionHeader);
    ++rowY;

    if (rowY <= maxRowY)
    {
        writeClipped(buffer, x + 2, rowY, width - 4, selectedFrame->documentSummary, Themes::Text);
        ++rowY;
    }

    if (rowY <= maxRowY)
    {
        writeClipped(buffer, x + 2, rowY, width - 4, selectedFrame->visibilityCompositeSummary, Themes::Info);
        ++rowY;
    }

    if (rowY <= maxRowY && selectedFrame->hasSourcePath)
    {
        writeClipped(
            buffer,
            x + 2,
            rowY,
            width - 4,
            "Source: " + selectedFrame->sourcePath + " -> " + selectedFrame->resolvedSourcePath,
            Themes::Text);
        ++rowY;
    }

    for (const std::string& layerDetail : selectedFrame->layerDetails)
    {
        if (rowY > maxRowY)
        {
            break;
        }

        writeClipped(buffer, x + 2, rowY, width - 4, layerDetail, Themes::Text);
        ++rowY;
    }
}
