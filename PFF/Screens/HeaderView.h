// Draws a FIGlet header within the supplied bounds.
#pragma once

#include "Rendering/Composition/PageComposer.h"
#include <string_view>

namespace HeaderView
{
inline constexpr int Height = 12;

void draw(Composition::PageComposer &page, const Rect &header, const TextObject &banner,
          std::string_view title);
}
