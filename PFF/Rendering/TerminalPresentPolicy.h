#pragma once

#include <cstddef>
#include <string>

class ScreenBuffer;

enum class TerminalPresentStrategy
{
    FullRedraw,
    DiffBased
};

struct TerminalPresentMetrics
{
    int frameWidth = 0;
    int frameHeight = 0;

    std::size_t totalCells = 0;
    std::size_t changedCells = 0;
    std::size_t dirtySpanCount = 0;
    std::size_t dirtyRowCount = 0;

    bool firstPresent = false;
    bool sizeMismatch = false;
    bool blinkForcedFullPresent = false;
};

struct TerminalPresentDecision
{
    TerminalPresentStrategy strategy = TerminalPresentStrategy::FullRedraw;
    bool clearScreenFirst = false;
    bool minimizeCursorMovement = true;
    std::string reason;
};

class TerminalPresentPolicy
{
public:
    TerminalPresentDecision decide(const TerminalPresentMetrics& metrics) const;

    static std::size_t estimateFullRedrawCells(const TerminalPresentMetrics& metrics);
    static std::size_t estimateDiffCells(const TerminalPresentMetrics& metrics);

    static const char* toString(TerminalPresentStrategy strategy);

private:
    bool shouldPreferFullRedrawForCoverage(const TerminalPresentMetrics& metrics) const;
    bool shouldPreferFullRedrawForFragmentation(const TerminalPresentMetrics& metrics) const;
};
