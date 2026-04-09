#include "Rendering/Diagnostics/RenderDiagnostics.h"

RenderDiagnostics::RenderDiagnostics() = default;

void RenderDiagnostics::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool RenderDiagnostics::isEnabled() const
{
    return m_enabled;
}

void RenderDiagnostics::setOutputPath(const std::string& outputPath)
{
    if (!outputPath.empty())
    {
        m_outputPath = outputPath;
    }
}

const std::string& RenderDiagnostics::outputPath() const
{
    return m_outputPath;
}

void RenderDiagnostics::setAppendMode(bool appendMode)
{
    m_appendMode = appendMode;
}

bool RenderDiagnostics::appendMode() const
{
    return m_appendMode;
}

CapabilityReport& RenderDiagnostics::report()
{
    return m_report;
}

const CapabilityReport& RenderDiagnostics::report() const
{
    return m_report;
}

void RenderDiagnostics::resetRuntimeData()
{
    m_report.clearRuntimeData();
}

bool RenderDiagnostics::hasRecordedData() const
{
    return m_report.hasRuntimeData() || m_report.rendererSelectionTrace().hasEntries();
}
