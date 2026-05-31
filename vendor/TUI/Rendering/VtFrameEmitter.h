#pragma once

#include <cstddef>
#include <string>

#include "Rendering/SgrEmitter.h"
#include "Rendering/VtRun.h"

struct VtFrameEmitterStats
{
    std::size_t cursorMoveCount = 0;
    std::size_t runCount = 0;
    std::size_t emittedTextBytes = 0;
    std::size_t emittedSgrBytes = 0;
    std::size_t emittedControlBytes = 0;
    std::size_t emittedTotalBytes = 0;
};

class VtFrameEmitter
{
public:
    explicit VtFrameEmitter(SgrEmitter& sgrEmitter);

    void beginFrame(bool clearScreenFirst);
    void appendRun(const VtRun& run);
    std::string finishFrame(bool resetStyleAtEnd = false);
    const VtFrameEmitterStats& stats() const;

private:
    void appendCursorMove(int x, int y);

private:
    SgrEmitter& m_sgrEmitter;
    std::string m_buffer;

    int m_cursorX = 0;
    int m_cursorY = 0;
    bool m_cursorKnown = false;
    VtFrameEmitterStats m_stats{};
};