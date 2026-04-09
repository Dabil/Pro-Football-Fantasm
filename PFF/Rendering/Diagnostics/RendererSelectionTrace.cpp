#include "Rendering/Diagnostics/RendererSelectionTrace.h"

void RendererSelectionTrace::clear()
{
    m_entries.clear();
}

bool RendererSelectionTrace::hasEntries() const
{
    return !m_entries.empty();
}

void RendererSelectionTrace::addEntry(
    RendererSelectionStage stage,
    const std::string& decision,
    const std::string& detail)
{
    RendererSelectionTraceEntry entry;
    entry.stage = stage;
    entry.decision = decision;
    entry.detail = detail;
    m_entries.push_back(entry);
}

const std::vector<RendererSelectionTraceEntry>& RendererSelectionTrace::entries() const
{
    return m_entries;
}

const char* RendererSelectionTrace::toString(RendererSelectionStage stage)
{
    switch (stage)
    {
    case RendererSelectionStage::StartupConfiguration:
        return "StartupConfiguration";

    case RendererSelectionStage::HostPreferenceEvaluation:
        return "HostPreferenceEvaluation";

    case RendererSelectionStage::RelaunchDecision:
        return "RelaunchDecision";

    case RendererSelectionStage::RendererPreferenceEvaluation:
        return "RendererPreferenceEvaluation";

    case RendererSelectionStage::FinalSelection:
        return "FinalSelection";

    default:
        return "Unknown";
    }
}
