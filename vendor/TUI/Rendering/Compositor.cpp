#include "Rendering/Compositor.h"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "Rendering/Composition/GlyphPolicy.h"
#include "Rendering/Composition/OverwriteRule.h"
#include "Rendering/Composition/SourceMask.h"
#include "Rendering/Composition/StylePolicy.h"
#include "Rendering/Composition/WritePresets.h"

namespace
{
    struct LayerSortEntry
    {
        const LayerInstance* layer = nullptr;
        std::size_t originalIndex = 0;
    };

    bool isSourceLeadingCell(const ScreenCell& cell)
    {
        return cell.kind == CellKind::Glyph || cell.kind == CellKind::Empty;
    }

    bool sourceMaskAllows(const ScreenCell& cell, Composition::SourceMask mask)
    {
        switch (mask)
        {
        case Composition::SourceMask::GlyphCellsOnly:
            return cell.kind == CellKind::Glyph;

        case Composition::SourceMask::SpaceCellsOnly:
            return cell.kind == CellKind::Empty;

        case Composition::SourceMask::AllCells:
        default:
            return cell.kind == CellKind::Glyph || cell.kind == CellKind::Empty;
        }
    }

    bool glyphPolicyAllows(const ScreenCell& cell, Composition::GlyphPolicy policy)
    {
        switch (policy)
        {
        case Composition::GlyphPolicy::None:
            return false;

        case Composition::GlyphPolicy::VisibleObject:
            return cell.kind == CellKind::Glyph && cell.glyph != U' ';

        case Composition::GlyphPolicy::AuthoredObject:
            return cell.kind == CellKind::Glyph;

        case Composition::GlyphPolicy::SolidObject:
            return cell.kind == CellKind::Glyph || cell.kind == CellKind::Empty;

        default:
            return false;
        }
    }

    bool overwriteRuleAllows(
        const ScreenBuffer& destination,
        int x,
        int y,
        Composition::OverwriteRule rule)
    {
        switch (rule)
        {
        case Composition::OverwriteRule::Never:
            return false;

        case Composition::OverwriteRule::Always:
            return true;

        case Composition::OverwriteRule::IfTargetEmpty:
            return destination.getLogicalCell(x, y).kind == CellKind::Empty;

        case Composition::OverwriteRule::IfTargetNonEmpty:
            return destination.getLogicalCell(x, y).kind != CellKind::Empty;

        default:
            return true;
        }
    }

    ScreenCell makeGlyphWriteCell(
        const ScreenCell& sourceCell,
        Composition::GlyphPolicy glyphPolicy)
    {
        if (glyphPolicy == Composition::GlyphPolicy::SolidObject &&
            sourceCell.kind == CellKind::Empty)
        {
            ScreenCell authoredSpace;
            authoredSpace.glyph = U' ';
            authoredSpace.cluster = std::u32string(1, U' ');
            authoredSpace.style = sourceCell.style;
            authoredSpace.kind = CellKind::Glyph;
            authoredSpace.width = CellWidth::One;
            authoredSpace.metadata = sourceCell.metadata;
            return authoredSpace;
        }

        return sourceCell;
    }

    bool isValidWideSourceCell(const ScreenBuffer& source, int x, int y)
    {
        if (!source.inBounds(x + 1, y))
        {
            return false;
        }

        return source.getCell(x + 1, y).kind == CellKind::WideTrailing;
    }
}

void Compositor::compose(
    const std::vector<LayerInstance>& layers,
    Surface& destination)
{
    compose(layers, destination.buffer());
}

void Compositor::compose(
    const std::vector<LayerInstance>& layers,
    ScreenBuffer& destination)
{
    compose(layers, destination, Composition::WritePresets::authoredObject());
}

void Compositor::compose(
    const std::vector<LayerInstance>& layers,
    Surface& destination,
    const Composition::WritePolicy& writePolicy)
{
    compose(layers, destination.buffer(), writePolicy);
}

void Compositor::compose(
    const std::vector<LayerInstance>& layers,
    ScreenBuffer& destination,
    const Composition::WritePolicy& writePolicy)
{
    if (!writePolicy.canWriteAnything())
    {
        return;
    }

    std::vector<LayerSortEntry> sortedLayers;
    sortedLayers.reserve(layers.size());

    for (std::size_t index = 0; index < layers.size(); ++index)
    {
        const LayerInstance& layer = layers[index];

        if (!layer.isVisible() || layer.isEmpty())
        {
            continue;
        }

        sortedLayers.push_back({ &layer, index });
    }

    std::stable_sort(
        sortedLayers.begin(),
        sortedLayers.end(),
        [](const LayerSortEntry& lhs, const LayerSortEntry& rhs)
        {
            if (lhs.layer->zOrder() != rhs.layer->zOrder())
            {
                return lhs.layer->zOrder() < rhs.layer->zOrder();
            }

            return lhs.originalIndex < rhs.originalIndex;
        });

    for (const LayerSortEntry& entry : sortedLayers)
    {
        composeLayer(*entry.layer, destination, writePolicy);
    }
}

void Compositor::composeLayer(
    const LayerInstance& layer,
    ScreenBuffer& destination,
    const Composition::WritePolicy& writePolicy)
{
    const ScreenBuffer& source = layer.buffer();

    const int sourceWidth = source.getWidth();
    const int sourceHeight = source.getHeight();

    if (sourceWidth <= 0 || sourceHeight <= 0)
    {
        return;
    }

    const int destinationWidth = destination.getWidth();
    const int destinationHeight = destination.getHeight();

    if (destinationWidth <= 0 || destinationHeight <= 0)
    {
        return;
    }

    const bool isExplicitStyleOnlyPreset =
        writePolicy.glyphPolicy == Composition::GlyphPolicy::None &&
        writePolicy.stylePolicy == Composition::StylePolicy::Apply;

    for (int sourceY = 0; sourceY < sourceHeight; ++sourceY)
    {
        const int destinationY = layer.y() + sourceY;

        if (destinationY < 0 || destinationY >= destinationHeight)
        {
            continue;
        }

        for (int sourceX = 0; sourceX < sourceWidth; ++sourceX)
        {
            const ScreenCell& sourceCell = source.getCell(sourceX, sourceY);

            if (!isSourceLeadingCell(sourceCell))
            {
                continue;
            }

            if (!sourceMaskAllows(sourceCell, writePolicy.sourceMask))
            {
                continue;
            }

            const int destinationX = layer.x() + sourceX;

            if (destinationX < 0 || destinationX >= destinationWidth)
            {
                continue;
            }

            const bool sourceIsWideGlyph =
                sourceCell.kind == CellKind::Glyph &&
                sourceCell.width == CellWidth::Two;

            if (sourceIsWideGlyph)
            {
                if (!isValidWideSourceCell(source, sourceX, sourceY))
                {
                    continue;
                }

                if (!destination.inBounds(destinationX + 1, destinationY))
                {
                    continue;
                }
            }

            const bool canWriteGlyph =
                glyphPolicyAllows(sourceCell, writePolicy.glyphPolicy) &&
                overwriteRuleAllows(
                    destination,
                    destinationX,
                    destinationY,
                    writePolicy.glyphOverwriteRule);

            const bool styleSourceCellAllowed =
                isExplicitStyleOnlyPreset || canWriteGlyph;

            const bool canWriteStyle =
                styleSourceCellAllowed &&
                writePolicy.stylePolicy == Composition::StylePolicy::Apply &&
                overwriteRuleAllows(
                    destination,
                    destinationX,
                    destinationY,
                    writePolicy.styleOverwriteRule);

            if (!canWriteGlyph && !canWriteStyle)
            {
                continue;
            }

            if (canWriteGlyph)
            {
                ScreenCell destinationCell =
                    makeGlyphWriteCell(sourceCell, writePolicy.glyphPolicy);

                if (!canWriteStyle)
                {
                    destinationCell.style =
                        destination.getLogicalCell(destinationX, destinationY).style;
                }

                destination.setCell(destinationX, destinationY, destinationCell);
                continue;
            }

            if (isExplicitStyleOnlyPreset && canWriteStyle)
            {
                destination.setCellStyle(destinationX, destinationY, sourceCell.style);
            }
        }
    }
}