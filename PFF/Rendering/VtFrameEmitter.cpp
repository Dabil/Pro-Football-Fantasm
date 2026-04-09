#include "Rendering/VtFrameEmitter.h"

#include <utility>

VtFrameEmitter::VtFrameEmitter(SgrEmitter& sgrEmitter)
    : m_sgrEmitter(sgrEmitter)
{
}

void VtFrameEmitter::beginFrame(bool clearScreenFirst)
{
    m_buffer.clear();
    m_stats = VtFrameEmitterStats{};

    if (clearScreenFirst)
    {
        const std::size_t before = m_buffer.size();
        m_buffer += "\x1b[2J\x1b[H";
        m_stats.emittedControlBytes += (m_buffer.size() - before);
        m_cursorX = 0;
        m_cursorY = 0;
        m_cursorKnown = true;
        m_sgrEmitter.reset();
    }
    else
    {
        m_cursorKnown = false;
    }
}

void VtFrameEmitter::appendRun(const VtRun& run)
{
    if (run.empty())
    {
        return;
    }

    if (!m_cursorKnown || m_cursorX != run.xStart || m_cursorY != run.y)
    {
        appendCursorMove(run.xStart, run.y);
    }

    {
        const std::size_t before = m_buffer.size();
        m_sgrEmitter.appendTransitionTo(m_buffer, run.presentedStyle);
        m_stats.emittedSgrBytes += (m_buffer.size() - before);
    }
    m_buffer += run.utf8Text;
    m_stats.runCount += 1;
    m_stats.emittedTextBytes += run.utf8Text.size();

    m_cursorX = run.xStart + run.cellWidth;
    m_cursorY = run.y;
    m_cursorKnown = true;
}

std::string VtFrameEmitter::finishFrame(bool resetStyleAtEnd)
{
    if (resetStyleAtEnd)
    {
        const std::size_t before = m_buffer.size();
        m_sgrEmitter.appendReset(m_buffer);
        m_stats.emittedSgrBytes += (m_buffer.size() - before);
    }

    m_stats.emittedTotalBytes = m_buffer.size();
    return std::move(m_buffer);
}

const VtFrameEmitterStats& VtFrameEmitter::stats() const
{
    return m_stats;
}

void VtFrameEmitter::appendCursorMove(int x, int y)
{
    const std::size_t before = m_buffer.size();
    m_buffer += "\x1b[";
    m_buffer += std::to_string(y + 1);
    m_buffer += ';';
    m_buffer += std::to_string(x + 1);
    m_buffer += 'H';
    m_stats.cursorMoveCount += 1;
    m_stats.emittedControlBytes += (m_buffer.size() - before);
}