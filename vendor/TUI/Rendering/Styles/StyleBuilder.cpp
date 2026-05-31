#include "Rendering/Styles/StyleBuilder.h"

#include "Rendering/Styles/StyleMerge.h"

/*
Composition behavior with this design is simple and predictable:

left side is the base style
right side overlays onto it
foreground/background from the right replace the left if present
boolean style fields from the right replace the left only when the
right side explicitly authored that field
this is still purely logical style composition, not renderer capability mapping

This makes it a good fit for themes, page composition, and any future semantic style namespace layer.
*/

namespace style
{
    Style Fg(const Color& color)
    {
        return Style().withForeground(color);
    }

    Style Fg(const ThemeColor& color)
    {
        return Style().withForeground(color);
    }

    Style Bg(const Color& color)
    {
        return Style().withBackground(color);
    }

    Style Bg(const ThemeColor& color)
    {
        return Style().withBackground(color);
    }
}

Style operator+(const Style& left, const Style& right)
{
    return StyleMerge::merge(left, right, StyleMergeMode::MergePreserveDestination);
}