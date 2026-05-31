#pragma once

#include <string>
#include <vector>

enum class RendererSelectionStage
{
    StartupConfiguration = 0,
    HostPreferenceEvaluation,
    RelaunchDecision,
    RendererPreferenceEvaluation,
    FinalSelection
};

struct RendererSelectionTraceEntry
{
    RendererSelectionStage stage = RendererSelectionStage::StartupConfiguration;
    std::string decision;
    std::string detail;
};

class RendererSelectionTrace
{
public:
    void clear();
    bool hasEntries() const;

    void addEntry(
        RendererSelectionStage stage,
        const std::string& decision,
        const std::string& detail);

    const std::vector<RendererSelectionTraceEntry>& entries() const;

    static const char* toString(RendererSelectionStage stage);

private:
    std::vector<RendererSelectionTraceEntry> m_entries;
};
