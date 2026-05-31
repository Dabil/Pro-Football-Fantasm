#include "Rendering/FrameDiff.h"
#include "Rendering/ScreenBuffer.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace
{
    void mergeBackIfNeeded(std::vector<DirtySpan>& spans, const DirtySpan& next)
    {
        if (spans.empty())
        {
            spans.push_back(next);
            return;
        }

        DirtySpan& back = spans.back();

        if (back.y == next.y && next.xStart <= back.xEnd + 1)
        {
            back.xEnd = std::max(back.xEnd, next.xEnd);
            return;
        }

        spans.push_back(next);
    }
}

std::vector<DirtySpan> FrameDiff::diffRows(const ScreenBuffer& previous, const ScreenBuffer& current)
{
    if (previous.getWidth() != current.getWidth() ||
        previous.getHeight() != current.getHeight())
    {
        throw std::runtime_error("FrameDiff requires buffers of matching size.");
    }

    std::vector<DirtySpan> dirtySpans;

    const int width = current.getWidth();
    const int height = current.getHeight();

    for (int y = 0; y < height; ++y)
    {
        int x = 0;

        while (x < width)
        {
            while (x < width && previous.getCell(x, y) == current.getCell(x, y))
            {
                ++x;
            }

            if (x >= width)
            {
                break;
            }

            int spanStart = x;

            while (x < width && previous.getCell(x, y) != current.getCell(x, y))
            {
                ++x;
            }

            int spanEnd = x - 1;

            int expandedStart = spanStart;
            int expandedEnd = spanEnd;

            previous.expandSpanToGlyphBoundaries(y, expandedStart, expandedEnd);

            int currentStart = spanStart;
            int currentEnd = spanEnd;
            current.expandSpanToGlyphBoundaries(y, currentStart, currentEnd);

            expandedStart = std::min(expandedStart, currentStart);
            expandedEnd = std::max(expandedEnd, currentEnd);

            mergeBackIfNeeded(dirtySpans, DirtySpan{ y, expandedStart, expandedEnd });
        }
    }

    return dirtySpans;
}