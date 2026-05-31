#pragma once

namespace Composition
{
    /*
        SourceMask answers:

            "Which source cells are eligible to participate in this write pass?"

        Semantics:
        - AllCells:
            All source cells may participate.
        - GlyphCellsOnly:
            Only source cells that represent glyph-bearing content may participate.
            This is the common transparent-overlay style mask.
        - SpaceCellsOnly:
            Only source cells that represent space/empty cells may participate.
            Useful for future erase, punch-through, masking, or negative-space passes.
    */
    enum class SourceMask
    {
        AllCells,
        GlyphCellsOnly,
        SpaceCellsOnly
    };
}