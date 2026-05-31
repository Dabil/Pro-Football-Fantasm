#include "Rendering/TerminalPresentPolicy.h"

namespace
{
    constexpr double kHighCoverageThreshold = 0.60;
    constexpr double kModerateCoverageThreshold = 0.35;
}

TerminalPresentDecision TerminalPresentPolicy::decide(const TerminalPresentMetrics& metrics) const
{
    TerminalPresentDecision decision;
    decision.minimizeCursorMovement = true;

    if (metrics.firstPresent)
    {
        decision.strategy = TerminalPresentStrategy::FullRedraw;
        decision.clearScreenFirst = true;
        decision.reason = "First present requires a full redraw.";
        return decision;
    }

    if (metrics.sizeMismatch)
    {
        decision.strategy = TerminalPresentStrategy::FullRedraw;
        decision.clearScreenFirst = true;
        decision.reason = "Frame size changed, so the terminal surface must be redrawn.";
        return decision;
    }

    if (metrics.blinkForcedFullPresent)
    {
        decision.strategy = TerminalPresentStrategy::FullRedraw;
        decision.clearScreenFirst = true;
        decision.reason = "Blink emulation visibility changed and requires a full present.";
        return decision;
    }

    if (metrics.changedCells == 0 || metrics.dirtySpanCount == 0)
    {
        decision.strategy = TerminalPresentStrategy::DiffBased;
        decision.clearScreenFirst = false;
        decision.reason = "No changed cells were detected.";
        return decision;
    }

    if (shouldPreferFullRedrawForCoverage(metrics))
    {
        decision.strategy = TerminalPresentStrategy::FullRedraw;
        decision.clearScreenFirst = true;
        decision.reason = "Changed-cell coverage is high enough that a full redraw is cheaper and simpler.";
        return decision;
    }

    if (shouldPreferFullRedrawForFragmentation(metrics))
    {
        decision.strategy = TerminalPresentStrategy::FullRedraw;
        decision.clearScreenFirst = true;
        decision.reason = "Dirty regions are fragmented enough that diff presentation would be cursor-heavy.";
        return decision;
    }

    decision.strategy = TerminalPresentStrategy::DiffBased;
    decision.clearScreenFirst = false;
    decision.reason = "Dirty-region coverage is low enough to benefit from diff-based presentation.";
    return decision;
}

std::size_t TerminalPresentPolicy::estimateFullRedrawCells(const TerminalPresentMetrics& metrics)
{
    return metrics.totalCells;
}

std::size_t TerminalPresentPolicy::estimateDiffCells(const TerminalPresentMetrics& metrics)
{
    return metrics.changedCells;
}

const char* TerminalPresentPolicy::toString(TerminalPresentStrategy strategy)
{
    switch (strategy)
    {
    case TerminalPresentStrategy::FullRedraw:
        return "FullRedraw";

    case TerminalPresentStrategy::DiffBased:
        return "DiffBased";

    default:
        return "Unknown";
    }
}

bool TerminalPresentPolicy::shouldPreferFullRedrawForCoverage(const TerminalPresentMetrics& metrics) const
{
    if (metrics.totalCells == 0)
    {
        return false;
    }

    const double coverage =
        static_cast<double>(metrics.changedCells) /
        static_cast<double>(metrics.totalCells);

    return coverage >= kHighCoverageThreshold;
}

bool TerminalPresentPolicy::shouldPreferFullRedrawForFragmentation(const TerminalPresentMetrics& metrics) const
{
    if (metrics.totalCells == 0 || metrics.frameHeight <= 0)
    {
        return false;
    }

    const double coverage =
        static_cast<double>(metrics.changedCells) /
        static_cast<double>(metrics.totalCells);

    if (coverage < kModerateCoverageThreshold)
    {
        return false;
    }

    return metrics.dirtySpanCount > static_cast<std::size_t>(metrics.frameHeight);
}
