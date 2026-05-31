#pragma once

namespace Composition
{
    /*
        GlyphPolicy answers:

            "Which source cells are allowed to write glyph content?"

        Semantics:
        - None:
            Never write glyph content.

        - VisibleObject:
            Write only visible non-space Glyph cells.
            Skips authored spaces and Empty cells.

        - AuthoredObject:
            Write all authored Glyph cells, including U' '.
            Skips Empty cells.

        - SolidObject:
            Write the full source object footprint.
            Glyph cells write normally.
            Empty cells participate as clearing authored spaces.
    */

    enum class GlyphPolicy
    {
        None,
        VisibleObject,
        AuthoredObject,
        SolidObject
    };
}