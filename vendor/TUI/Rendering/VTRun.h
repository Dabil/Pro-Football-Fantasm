#pragma once

#include <string>

#include "Rendering/Styles/Style.h"

struct VtRun
{
    int y = 0;
    int xStart = 0;
    int cellWidth = 0;

    Style presentedStyle{};

    std::string utf8Text;

    bool empty() const
    {
        return utf8Text.empty() || cellWidth <= 0;
    }
};