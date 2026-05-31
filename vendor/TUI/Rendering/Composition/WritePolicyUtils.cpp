#include "Rendering/Composition/WritePolicyUtils.h"

namespace Composition::WritePolicyUtils
{
    bool isSourceLeadingCell(const TextObjectCell& cell)
    {
        return cell.kind == CellKind::Glyph || cell.kind == CellKind::Empty;
    }

    bool sourceMaskAllows(const TextObjectCell& cell, SourceMask mask)
    {
        switch (mask)
        {
        case SourceMask::GlyphCellsOnly:
            return cell.kind == CellKind::Glyph;

        case SourceMask::SpaceCellsOnly:
            return cell.kind == CellKind::Empty;

        case SourceMask::AllCells:
        default:
            return cell.kind == CellKind::Glyph || cell.kind == CellKind::Empty;
        }
    }

    bool glyphPolicyAllows(const TextObjectCell& cell, GlyphPolicy policy)
    {
        switch (policy)
        {
        case GlyphPolicy::None:
            return false;

        case GlyphPolicy::VisibleObject:
            return cell.kind == CellKind::Glyph && cell.glyph != U' ';

        case GlyphPolicy::AuthoredObject:
            return cell.kind == CellKind::Glyph;

        case GlyphPolicy::SolidObject:
            return cell.kind == CellKind::Glyph || cell.kind == CellKind::Empty;

        default:
            return false;
        }
    }

    bool sourceCellCanWriteGlyph(const TextObjectCell& cell, const WritePolicy& policy)
    {
        return glyphPolicyAllows(cell, policy.glyphPolicy) &&
            policy.glyphOverwriteRule != OverwriteRule::Never;
    }

    bool sourceCellParticipates(const TextObjectCell& cell, const WritePolicy& policy)
    {
        if (!isSourceLeadingCell(cell))
        {
            return false;
        }

        if (!sourceMaskAllows(cell, policy.sourceMask))
        {
            return false;
        }

        if (sourceCellCanWriteGlyph(cell, policy))
        {
            return true;
        }

        return policy.stylePolicy == StylePolicy::Apply;
    }

    TextObjectCell resolveGlyphWriteCell(const TextObjectCell& sourceCell, const WritePolicy& policy)
    {
        if (policy.glyphPolicy == GlyphPolicy::SolidObject &&
            sourceCell.kind == CellKind::Empty)
        {
            TextObjectCell authoredSpace;
            authoredSpace.glyph = U' ';
            authoredSpace.kind = CellKind::Glyph;
            authoredSpace.width = CellWidth::One;
            authoredSpace.style = sourceCell.style;
            return authoredSpace;
        }

        return sourceCell;
    }

    bool isBuilderCompatible(const WritePolicy& policy)
    {
        if (policy.depthPolicy == DepthPolicy::BehindOnly)
        {
            return false;
        }

        if (policy.glyphOverwriteRule != OverwriteRule::Always)
        {
            return false;
        }

        if (policy.styleOverwriteRule != OverwriteRule::Always &&
            policy.styleOverwriteRule != OverwriteRule::Never)
        {
            return false;
        }

        return policy.glyphPolicy != GlyphPolicy::None;
    }
}