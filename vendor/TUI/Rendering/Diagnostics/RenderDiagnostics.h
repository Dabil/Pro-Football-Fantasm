#pragma once

#include <string>

#include "Rendering/Capabilities/CapabilityReport.h"

/*
    Purpose:

    RenderDiagnostics is the optional renderer-side diagnostics state holder.

    It does not control rendering.
    It only stores:
        - whether diagnostics are enabled
        - where diagnostics should be written
        - whether the file should be appended or replaced
        - the structured capability/adaptation report data

    Renderer code can populate this object during initialization and rendering,
    and a writer can later serialize it to a human-readable file.
*/

class RenderDiagnostics
{
public:
    RenderDiagnostics();

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setOutputPath(const std::string& outputPath);
    const std::string& outputPath() const;

    void setAppendMode(bool appendMode);
    bool appendMode() const;

    CapabilityReport& report();
    const CapabilityReport& report() const;

    void resetRuntimeData();
    bool hasRecordedData() const;

private:
    bool m_enabled = false;
    bool m_appendMode = false;
    std::string m_outputPath = "render_diagnostics_report.txt";
    CapabilityReport m_report;
};