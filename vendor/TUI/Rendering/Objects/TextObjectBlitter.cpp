#include "Rendering/Objects/TextObjectBlitter.h"

#include <cassert>

#include "Rendering/Composition/ObjectWriter.h"
#include "Rendering/Composition/WritePolicyUtils.h"

namespace
{
    bool isVisibleGlyph(const TextObjectCell& cell)
    {
        return cell.kind == CellKind::Glyph && cell.glyph != U' ';
    }

    bool isAuthoredGlyph(const TextObjectCell& cell)
    {
        return cell.kind == CellKind::Glyph;
    }

    bool isValidWideLead(const TextObject& source, int x, int y, const TextObjectCell& cell)
    {
        if (cell.kind != CellKind::Glyph || cell.width != CellWidth::Two)
        {
            return false;
        }

        const TextObjectCell* trailing = source.tryGetCell(x + 1, y);
        return trailing != nullptr && trailing->kind == CellKind::WideTrailing;
    }

    bool shouldSkipCell(
        const TextObject& source,
        int sourceX,
        int sourceY,
        const TextObjectCell& cell,
        const Composition::WritePolicy& policy)
    {
        if (cell.kind == CellKind::WideTrailing)
        {
            return true;
        }

        if (cell.kind == CellKind::Glyph && cell.width == CellWidth::Two)
        {
            return !isValidWideLead(source, sourceX, sourceY, cell);
        }

        switch (policy.glyphPolicy)
        {
        case Composition::GlyphPolicy::None:
            return true;

        case Composition::GlyphPolicy::VisibleObject:
            return !isVisibleGlyph(cell);

        case Composition::GlyphPolicy::AuthoredObject:
            return !isAuthoredGlyph(cell);

        case Composition::GlyphPolicy::SolidObject:
            return cell.kind != CellKind::Glyph && cell.kind != CellKind::Empty;

        default:
            return true;
        }
    }

    TextObjectCell resolveBuilderWriteCell(
        const TextObjectCell& sourceCell,
        const Composition::WritePolicy& policy)
    {
        if (policy.glyphPolicy == Composition::GlyphPolicy::SolidObject &&
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

    std::optional<Style> resolveBuilderStyle(
        const TextObjectCell& sourceCell,
        const TextObjectBlitter::BlitOptions& options)
    {
        if (options.overrideStyle.has_value())
        {
            return options.overrideStyle;
        }

        return sourceCell.style;
    }
}

namespace TextObjectBlitter
{
    void blitToBuilder(
        TextObjectBuilder& builder,
        const TextObject& source,
        int offsetX,
        int offsetY,
        const BlitOptions& options)
    {
        if (!source.isLoaded() || source.getWidth() <= 0 || source.getHeight() <= 0)
        {
            return;
        }

        assert(
            Composition::WritePolicyUtils::isBuilderCompatible(options.writePolicy) &&
            "TextObjectBlitter::blitToBuilder only supports glyph-writing overwrite policies.");

        if (!Composition::WritePolicyUtils::isBuilderCompatible(options.writePolicy))
        {
            return;
        }

        for (int y = 0; y < source.getHeight(); ++y)
        {
            for (int x = 0; x < source.getWidth(); ++x)
            {
                const TextObjectCell* cell = source.tryGetCell(x, y);
                if (cell == nullptr)
                {
                    continue;
                }

                if (shouldSkipCell(source, x, y, *cell, options.writePolicy))
                {
                    continue;
                }

                if (!Composition::WritePolicyUtils::sourceMaskAllows(*cell, options.writePolicy.sourceMask))
                {
                    continue;
                }

                const int destX = offsetX + x;
                const int destY = offsetY + y;

                if (!builder.inBounds(destX, destY))
                {
                    continue;
                }

                const std::optional<Style> styleToApply = resolveBuilderStyle(*cell, options);

                switch (cell->kind)
                {
                case CellKind::Glyph:
                    if (cell->width == CellWidth::Two)
                    {
                        builder.setWideGlyph(destX, destY, cell->glyph, styleToApply);
                    }
                    else
                    {
                        builder.setCell(
                            destX,
                            destY,
                            cell->glyph,
                            CellKind::Glyph,
                            CellWidth::One,
                            styleToApply);
                    }
                    break;

                case CellKind::Empty:
                    if (options.writePolicy.glyphPolicy == Composition::GlyphPolicy::SolidObject)
                    {
                        builder.setAuthoredSpace(destX, destY, styleToApply);
                    }
                    else
                    {
                        builder.setTransparent(destX, destY, styleToApply);
                    }
                    break;

                case CellKind::WideTrailing:
                case CellKind::CombiningContinuation:
                    if (!options.skipStructuralContinuationCells)
                    {
                        builder.setCell(
                            destX,
                            destY,
                            cell->glyph,
                            cell->kind,
                            cell->width,
                            styleToApply);
                    }
                    break;

                default:
                    break;
                }
            }
        }
    }

    void blitToScreenBuffer(
        ScreenBuffer& target,
        const TextObject& source,
        int offsetX,
        int offsetY,
        const BlitOptions& options)
    {
        Composition::ObjectWriter writer(target, offsetX, offsetY);
        writer.writeObject(source, options.writePolicy, options.overrideStyle);
    }
}