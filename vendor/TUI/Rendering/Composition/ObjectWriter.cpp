#include "Rendering/Composition/ObjectWriter.h"

#include <array>

#include "Rendering/Composition/WritePolicyUtils.h"
#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Styles/StyleMerge.h"

/*
   Concrete Rule:
 
    Object glyph presets:
        - write glyph + style together
        - never write style alone

    Style presets:
        - write style alone intentionally
        - never imply glyph writes
*/

namespace
{
    struct LogicalCellRef
    {
        int x = 0;
        int y = 0;
        const ScreenCell* cell = nullptr;
    };

    const ScreenCell& resolveLogicalDestinationCell(const ScreenBuffer& target, int x, int y)
    {
        const ScreenCell& cell = target.getCell(x, y);

        if (cell.kind == CellKind::WideTrailing && target.inBounds(x - 1, y))
        {
            const ScreenCell& leading = target.getCell(x - 1, y);
            if (leading.kind == CellKind::Glyph && leading.width == CellWidth::Two)
            {
                return leading;
            }
        }

        return cell;
    }

    Style resolveLogicalDestinationStyle(const ScreenBuffer& target, int x, int y)
    {
        return resolveLogicalDestinationCell(target, x, y).style;
    }

    int collectLogicalDestinationCells(
        const ScreenBuffer& target,
        int x,
        int y,
        int width,
        std::array<LogicalCellRef, 2>& outCells)
    {
        int count = 0;

        for (int dx = 0; dx < width; ++dx)
        {
            const int currentX = x + dx;
            const ScreenCell& logicalCell = resolveLogicalDestinationCell(target, currentX, y);

            int logicalX = currentX;
            if (target.getCell(currentX, y).kind == CellKind::WideTrailing)
            {
                logicalX = currentX - 1;
            }

            bool alreadyAdded = false;
            for (int i = 0; i < count; ++i)
            {
                if (outCells[i].x == logicalX && outCells[i].y == y)
                {
                    alreadyAdded = true;
                    break;
                }
            }

            if (!alreadyAdded)
            {
                outCells[count].x = logicalX;
                outCells[count].y = y;
                outCells[count].cell = &logicalCell;
                ++count;
            }
        }

        return count;
    }

    bool overwriteRuleAllows(
        const ScreenBuffer& target,
        int x,
        int y,
        int width,
        Composition::OverwriteRule rule)
    {
        switch (rule)
        {
        case Composition::OverwriteRule::Never:
            return false;

        case Composition::OverwriteRule::Always:
            return true;

        case Composition::OverwriteRule::IfTargetEmpty:
        case Composition::OverwriteRule::IfTargetNonEmpty:
            break;
        }

        std::array<LogicalCellRef, 2> logicalCells{};
        const int logicalCellCount = collectLogicalDestinationCells(target, x, y, width, logicalCells);

        for (int i = 0; i < logicalCellCount; ++i)
        {
            const bool empty =
                logicalCells[i].cell == nullptr ||
                logicalCells[i].cell->kind == CellKind::Empty;

            if (rule == Composition::OverwriteRule::IfTargetEmpty && !empty)
            {
                return false;
            }

            if (rule == Composition::OverwriteRule::IfTargetNonEmpty && empty)
            {
                return false;
            }
        }

        return true;
    }

    bool depthPolicyAllows(
        const ScreenBuffer& target,
        int x,
        int y,
        int width,
        Composition::DepthPolicy policy)
    {
        (void)target;
        (void)x;
        (void)y;
        (void)width;

        switch (policy)
        {
        case Composition::DepthPolicy::Ignore:
        case Composition::DepthPolicy::Respect:
            return true;

        case Composition::DepthPolicy::BehindOnly:
            return false;

        default:
            return true;
        }
    }

    bool isValidSourceWideGlyph(const TextObject& source, int x, int y)
    {
        if (x + 1 >= source.getWidth())
        {
            return false;
        }

        const TextObjectCell* trailing = source.tryGetCell(x + 1, y);
        if (trailing == nullptr)
        {
            return false;
        }

        return trailing->kind == CellKind::WideTrailing;
    }

    std::optional<Style> resolveSourceStyle(
        const TextObjectCell& sourceCell,
        const std::optional<Style>& sourceStyleOverride)
    {
        if (sourceStyleOverride.has_value())
        {
            return sourceStyleOverride;
        }

        return sourceCell.style;
    }

    ScreenCell makeGlyphWriteCell(
        const TextObjectCell& sourceCell,
        const Style& style,
        Composition::GlyphPolicy glyphPolicy)
    {
        ScreenCell destinationCell;
        destinationCell.style = style;

        if (glyphPolicy == Composition::GlyphPolicy::SolidObject &&
            sourceCell.kind == CellKind::Empty)
        {
            destinationCell.glyph = U' ';
            destinationCell.cluster = std::u32string(1, U' ');
            destinationCell.kind = CellKind::Glyph;
            destinationCell.width = CellWidth::One;
            return destinationCell;
        }

        destinationCell.glyph = sourceCell.glyph;
        destinationCell.cluster = sourceCell.cluster;
        destinationCell.kind = sourceCell.kind;
        destinationCell.width = sourceCell.width;
        return destinationCell;
    }

    void writeWithPreset(
        Composition::ObjectWriter& writer,
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        const Composition::WritePolicy& policy)
    {
        writer.writeObject(source, policy, sourceStyleOverride);
    }

    Style mergeObjectGlyphStyle(
        const Style& destinationStyle,
        const Style& sourceStyle)
    {
        Style result = StyleMerge::merge(
            destinationStyle,
            sourceStyle,
            StyleMergeMode::MergePreserveDestination);

        if (!sourceStyle.boldState().has_value())
        {
            result = result.withBold(false);
        }

        if (!sourceStyle.dimState().has_value())
        {
            result = result.withDim(false);
        }

        if (!sourceStyle.underlineState().has_value())
        {
            result = result.withUnderline(false);
        }

        if (!sourceStyle.slowBlinkState().has_value())
        {
            result = result.withSlowBlink(false);
        }

        if (!sourceStyle.fastBlinkState().has_value())
        {
            result = result.withFastBlink(false);
        }

        if (!sourceStyle.reverseState().has_value())
        {
            result = result.withReverse(false);
        }

        if (!sourceStyle.invisibleState().has_value())
        {
            result = result.withInvisible(false);
        }

        if (!sourceStyle.strikeState().has_value())
        {
            result = result.withStrike(false);
        }

        return result;
    }
}

namespace Composition
{
    ObjectWriter::ObjectWriter(ScreenBuffer& target, int offsetX, int offsetY)
        : m_target(target)
        , m_offsetX(offsetX)
        , m_offsetY(offsetY)
    {
    }

    void ObjectWriter::setOffset(int offsetX, int offsetY)
    {
        m_offsetX = offsetX;
        m_offsetY = offsetY;
    }

    int ObjectWriter::offsetX() const
    {
        return m_offsetX;
    }

    int ObjectWriter::offsetY() const
    {
        return m_offsetY;
    }

    void ObjectWriter::writeObject(const TextObject& source, const WritePolicy& policy)
    {
        writeObjectInternal(source, policy, std::nullopt);
    }

    void ObjectWriter::writeObject(
        const TextObject& source,
        const WritePolicy& policy,
        const std::optional<Style>& sourceStyleOverride)
    {
        writeObjectInternal(source, policy, sourceStyleOverride);
    }

    void ObjectWriter::writeObjectInternal(
        const TextObject& source,
        const WritePolicy& policy,
        const std::optional<Style>& sourceStyleOverride)
    {
        if (!source.isLoaded() || source.getWidth() <= 0 || source.getHeight() <= 0)
        {
            return;
        }

        if (!policy.canWriteAnything())
        {
            return;
        }

        for (int sourceY = 0; sourceY < source.getHeight(); ++sourceY)
        {
            for (int sourceX = 0; sourceX < source.getWidth(); ++sourceX)
            {
                const TextObjectCell* sourceCell = source.tryGetCell(sourceX, sourceY);
                if (sourceCell == nullptr)
                {
                    continue;
                }

                if (!WritePolicyUtils::isSourceLeadingCell(*sourceCell))
                {
                    continue;
                }

                if (!WritePolicyUtils::sourceMaskAllows(*sourceCell, policy.sourceMask))
                {
                    continue;
                }

                const int destinationX = m_offsetX + sourceX;
                const int destinationY = m_offsetY + sourceY;

                const bool sourceIsWideGlyph =
                    sourceCell->kind == CellKind::Glyph &&
                    sourceCell->width == CellWidth::Two;

                const int writeWidth = sourceIsWideGlyph ? 2 : 1;

                if (!m_target.inBounds(destinationX, destinationY) ||
                    !m_target.inBounds(destinationX + writeWidth - 1, destinationY))
                {
                    continue;
                }

                if (sourceIsWideGlyph && !isValidSourceWideGlyph(source, sourceX, sourceY))
                {
                    continue;
                }

                if (!depthPolicyAllows(m_target, destinationX, destinationY, writeWidth, policy.depthPolicy))
                {
                    continue;
                }

                const bool canWriteGlyph =
                    WritePolicyUtils::glyphPolicyAllows(*sourceCell, policy.glyphPolicy) &&
                    overwriteRuleAllows(
                        m_target,
                        destinationX,
                        destinationY,
                        writeWidth,
                        policy.glyphOverwriteRule);

                const std::optional<Style> resolvedSourceStyle =
                    resolveSourceStyle(*sourceCell, sourceStyleOverride);

                const bool isExplicitStyleOnlyPreset =
                    policy.glyphPolicy == Composition::GlyphPolicy::None &&
                    policy.stylePolicy == Composition::StylePolicy::Apply;

                const bool styleSourceCellAllowed =
                    isExplicitStyleOnlyPreset || canWriteGlyph;

                const bool canWriteStyle =
                    styleSourceCellAllowed &&
                    policy.stylePolicy == Composition::StylePolicy::Apply &&
                    resolvedSourceStyle.has_value() &&
                    overwriteRuleAllows(
                        m_target,
                        destinationX,
                        destinationY,
                        writeWidth,
                        policy.styleOverwriteRule);

                if (!canWriteGlyph && !canWriteStyle)
                {
                    continue;
                }

                if (canWriteGlyph)
                {
                    const Style writeStyle = canWriteStyle
                        ? mergeObjectGlyphStyle(
                            resolveLogicalDestinationStyle(m_target, destinationX, destinationY),
                            *resolvedSourceStyle)
                        : resolveLogicalDestinationStyle(m_target, destinationX, destinationY);

                    const ScreenCell destinationCell =
                        makeGlyphWriteCell(*sourceCell, writeStyle, policy.glyphPolicy);

                    m_target.setCell(destinationX, destinationY, destinationCell);
                    continue;
                }

                if (isExplicitStyleOnlyPreset && canWriteStyle)
                {
                    m_target.setCellStyle(
                        destinationX,
                        destinationY,
                        StyleMerge::merge(
                            resolveLogicalDestinationStyle(m_target, destinationX, destinationY),
                            *resolvedSourceStyle,
                            StyleMergeMode::MergePreserveDestination));
                }

                // Empty cells are transparent/no-op cells.
                // They must not mutate the destination through style-only writes.
                if (sourceCell->kind == CellKind::Empty)
                {
                    continue;
                }
            }
        }
    }

    void ObjectWriter::writeVisibleOnly(
        const TextObject& source,
        DepthPolicy depthPolicy)
    {
        writeVisibleOnly(source, std::nullopt, depthPolicy);
    }

    void ObjectWriter::writeVisibleOnly(
        const TextObject& source,
        const Style& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeVisibleOnly(source, std::optional<Style>(sourceStyleOverride), depthPolicy);
    }

    void ObjectWriter::writeVisibleOnly(
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeWithPreset(*this, source, sourceStyleOverride, WritePresets::visibleObject(depthPolicy));
    }

    void ObjectWriter::writeAuthoredOnly(
        const TextObject& source,
        DepthPolicy depthPolicy)
    {
        writeAuthoredOnly(source, std::nullopt, depthPolicy);
    }

    void ObjectWriter::writeAuthoredOnly(
        const TextObject& source,
        const Style& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeAuthoredOnly(source, std::optional<Style>(sourceStyleOverride), depthPolicy);
    }

    void ObjectWriter::writeAuthoredOnly(
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeWithPreset(*this, source, sourceStyleOverride, WritePresets::authoredObject(depthPolicy));
    }

    void ObjectWriter::writeSolidObject(
        const TextObject& source,
        DepthPolicy depthPolicy)
    {
        writeSolidObject(source, std::nullopt, depthPolicy);
    }

    void ObjectWriter::writeSolidObject(
        const TextObject& source,
        const Style& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeSolidObject(source, std::optional<Style>(sourceStyleOverride), depthPolicy);
    }

    void ObjectWriter::writeSolidObject(
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeWithPreset(*this, source, sourceStyleOverride, WritePresets::solidObject(depthPolicy));
    }

    void ObjectWriter::writeGlyphsOnly(
        const TextObject& source,
        DepthPolicy depthPolicy)
    {
        writeGlyphsOnly(source, std::nullopt, depthPolicy);
    }

    void ObjectWriter::writeGlyphsOnly(
        const TextObject& source,
        const Style& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeGlyphsOnly(source, std::optional<Style>(sourceStyleOverride), depthPolicy);
    }

    void ObjectWriter::writeGlyphsOnly(
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeWithPreset(*this, source, sourceStyleOverride, WritePresets::glyphsOnly(depthPolicy));
    }

    void ObjectWriter::writeStyleMask(
        const TextObject& source,
        DepthPolicy depthPolicy)
    {
        writeStyleMask(source, std::nullopt, depthPolicy);
    }

    void ObjectWriter::writeStyleMask(
        const TextObject& source,
        const Style& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeStyleMask(source, std::optional<Style>(sourceStyleOverride), depthPolicy);
    }

    void ObjectWriter::writeStyleMask(
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeWithPreset(*this, source, sourceStyleOverride, WritePresets::styleMask(depthPolicy));
    }

    void ObjectWriter::writeStyleBlock(
        const TextObject& source,
        DepthPolicy depthPolicy)
    {
        writeStyleBlock(source, std::nullopt, depthPolicy);
    }

    void ObjectWriter::writeStyleBlock(
        const TextObject& source,
        const Style& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeStyleBlock(source, std::optional<Style>(sourceStyleOverride), depthPolicy);
    }

    void ObjectWriter::writeStyleBlock(
        const TextObject& source,
        const std::optional<Style>& sourceStyleOverride,
        DepthPolicy depthPolicy)
    {
        writeWithPreset(*this, source, sourceStyleOverride, WritePresets::styleBlock(depthPolicy));
    }
}
