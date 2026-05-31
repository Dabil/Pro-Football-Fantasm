#pragma once

#include <string>

#include "Core/Rect.h"

class Window;

namespace UI
{
    struct DockTarget
    {
        Window* window = nullptr;
        std::string contentId;
        Rect bounds{};
        int zOrder = 0;

        bool isValid() const
        {
            return window != nullptr
                && !contentId.empty()
                && bounds.size.width > 0
                && bounds.size.height > 0;
        }
    };
}