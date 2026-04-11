#pragma once

#include "Rendering/Objects/XpSequenceInspection.h"
#include "Screens/Screen.h"

class ScreenBuffer;
class Surface;

class XpSequenceDiagnosticsScreen : public Screen
{
public:
    explicit XpSequenceDiagnosticsScreen(
        const XpSequenceInspection::InspectionReport& report);

    ~XpSequenceDiagnosticsScreen() override = default;

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    void drawSummaryPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawManifestPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawPlaybackPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawDiagnosticsPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawFramesPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawRetainedDocumentPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;

private:
    XpSequenceInspection::InspectionReport m_report;
    double m_elapsedSeconds = 0.0;
};
