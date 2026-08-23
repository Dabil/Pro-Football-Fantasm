#pragma once

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Surface.h"
#include "UI/Base/Viewport.h"

namespace UI::Scrolling
{
    struct VerticalScrollbarGlyphs
    {
        char32_t track = U'│';
        char32_t thumb = U'█';
    };

    struct VerticalScrollbarStyle
    {
        Style trackStyle;
        Style thumbStyle;
        VerticalScrollbarGlyphs glyphs;
    };

    bool shouldShowVerticalScrollbar(const Viewport& viewport);

    Rect verticalScrollbarBoundsForContentBounds(
        const Rect& contentBounds,
        bool reserveColumn);

    Rect viewportBoundsForContentBounds(
        const Rect& contentBounds,
        bool verticalScrollbarVisible,
        bool reserveColumn);

    void drawVerticalScrollbar(
        Surface& surface,
        const Rect& scrollbarBounds,
        const Viewport& viewport,
        const VerticalScrollbarStyle& style);
}