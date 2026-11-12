// Sets up the screen and allocates bounds for a header, body, and footer.
#pragma once

#include "Rendering/Composition/PageComposer.h"

namespace FrameView
{
struct Regions
{
    Rect screen;
    Rect header;
    Rect body;
    Rect footer;
};

Regions begin(Composition::PageComposer &page, int headerHeight, int footerHeight, const Style &background);
}
