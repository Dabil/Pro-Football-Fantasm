#pragma once

#include <vector>

class ScreenBuffer;

struct DirtySpan
{
    int y = 0;
    int xStart = 0;
    int xEnd = 0;
};

class FrameDiff
{
public:
    static std::vector<DirtySpan> diffRows(const ScreenBuffer& previous, const ScreenBuffer& current);
};