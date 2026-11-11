#pragma once

#include "Core/Rect.h"
#include <algorithm>

namespace PffLeagueFooter
{
inline constexpr int Height = 5;

inline Rect textBounds(const Rect &footer)
{
    return Rect{Point{footer.position.x + 2, footer.position.y + 1},
                Size{std::max(0, footer.size.width - 42), std::min(2, std::max(0, footer.size.height - 1))}};
}

inline Rect returnBounds(const Rect &footer)
{
    constexpr int rightInset = 10;
    const Size button{std::clamp(footer.size.width - rightInset, 0, 28),
                      std::clamp(footer.size.height, 0, 3)};
    return Rect{Point{footer.position.x + std::max(0, footer.size.width - button.width - rightInset),
                      footer.position.y + (footer.size.height - button.height) / 2},
                button};
}
} // namespace PffLeagueFooter
