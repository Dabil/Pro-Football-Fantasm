#include "Rendering/Objects/TextObjectFactory.h"
#include "Rendering/Objects/TextObjectBuilder.h"

#include <algorithm>
#include <optional>
#include <vector>

/*
Concrete Rule: When ObjectFactory method's should use TextObjectBuilder.h instead of TextObject.h

A new or revised ObjectFactory method should use TextObjectBuilder when any of these are true:
- it must place a wide glyph and also emit the required trailing cell correctly
- it must intentionally write non-default CellKind or CellWidth
- it must distinguish true empty cells from visible space glyph cells in a structural way
- it must apply style at different cells within the same returned object
- it must compose output procedurally at exact (x, y) positions rather than by assembling rows of text
- it needs to overlay or merge multiple generated pieces into one object at cell precision
*/

/*
* Concrete Rule: preserve the integrity of the motif, even if that means leaving 
* space or shortening the repeat
*/

/*
* Concrete Rule: All TextObjectFactory functions must return fully authored TextObjects. 
* This includes placing U' ' instead of leaving cells empty
*/

namespace
{
    std::u32string repeatGlyph(char32_t glyph, int count)
    {
        if (count <= 0)
        {
            return {};
        }

        return std::u32string(static_cast<std::size_t>(count), glyph);
    }

    TextObject buildObjectFromLines(
        const std::vector<std::u32string>& lines,
        const std::optional<Style>& style)
    {
        if (lines.empty())
        {
            return TextObject();
        }

        std::u32string combined;
        for (std::size_t i = 0; i < lines.size(); ++i)
        {
            if (i > 0)
            {
                combined.push_back(U'\n');
            }

            combined += lines[i];
        }

        if (style.has_value())
        {
            return TextObject::fromU32(combined, *style);
        }

        return TextObject::fromU32(combined);
    }

    std::vector<std::u32string> buildFilledRectLines(int width, int height, char32_t fillGlyph)
    {
        if (width <= 0 || height <= 0)
        {
            return {};
        }

        std::vector<std::u32string> lines;
        lines.reserve(static_cast<std::size_t>(height));

        const std::u32string row = repeatGlyph(fillGlyph, width);
        for (int y = 0; y < height; ++y)
        {
            lines.push_back(row);
        }

        return lines;
    }

    std::vector<std::u32string> buildHorizontalLineLines(int length, char32_t glyph)
    {
        if (length <= 0)
        {
            return {};
        }

        return { repeatGlyph(glyph, length) };
    }

    std::vector<std::u32string> buildVerticalLineLines(int length, char32_t glyph)
    {
        if (length <= 0)
        {
            return {};
        }

        std::vector<std::u32string> lines;
        lines.reserve(static_cast<std::size_t>(length));

        for (int i = 0; i < length; ++i)
        {
            lines.push_back(std::u32string(1, glyph));
        }

        return lines;
    }

    std::vector<std::u32string> buildFrameLines(
        int width,
        int height,
        const ObjectFactory::BorderGlyphs& border,
        std::optional<char32_t> fillGlyph)
    {
        if (width <= 0 || height <= 0)
        {
            return {};
        }

        std::vector<std::u32string> lines;
        lines.reserve(static_cast<std::size_t>(height));

        if (width == 1 && height == 1)
        {
            lines.push_back(std::u32string(1, border.topLeft));
            return lines;
        }

        if (height == 1)
        {
            std::u32string row;
            row.reserve(static_cast<std::size_t>(width));

            if (width == 1)
            {
                row.push_back(border.topLeft);
            }
            else
            {
                row.push_back(border.topLeft);

                for (int x = 1; x < width - 1; ++x)
                {
                    row.push_back(border.horizontal);
                }

                row.push_back(border.topRight);
            }

            lines.push_back(row);
            return lines;
        }

        if (width == 1)
        {
            lines.push_back(std::u32string(1, border.topLeft));

            for (int y = 1; y < height - 1; ++y)
            {
                lines.push_back(std::u32string(1, border.vertical));
            }

            lines.push_back(std::u32string(1, border.bottomLeft));
            return lines;
        }

        std::u32string top;
        top.reserve(static_cast<std::size_t>(width));
        top.push_back(border.topLeft);
        top += repeatGlyph(border.horizontal, width - 2);
        top.push_back(border.topRight);
        lines.push_back(top);

        const char32_t interiorGlyph = fillGlyph.value_or(U' ');
        for (int y = 1; y < height - 1; ++y)
        {
            std::u32string middle;
            middle.reserve(static_cast<std::size_t>(width));
            middle.push_back(border.vertical);
            middle += repeatGlyph(interiorGlyph, width - 2);
            middle.push_back(border.vertical);
            lines.push_back(middle);
        }

        std::u32string bottom;
        bottom.reserve(static_cast<std::size_t>(width));
        bottom.push_back(border.bottomLeft);
        bottom += repeatGlyph(border.horizontal, width - 2);
        bottom.push_back(border.bottomRight);
        lines.push_back(bottom);

        return lines;
    }

    struct UniformDividerLayout
    {
        int usedSize = 0;
        std::vector<int> offsets;
    };

    UniformDividerLayout computeUniformDividerLayout(int requestedOuterSize, int dividerCount)
    {
        UniformDividerLayout layout;

        if (requestedOuterSize <= 0)
        {
            return layout;
        }

        if (dividerCount <= 0)
        {
            layout.usedSize = requestedOuterSize;
            return layout;
        }

        const int segmentCount = dividerCount + 1;
        const int requestedInteriorSize = std::max(0, requestedOuterSize - 2);

        // Interior must contain:
        // - N uniform cell regions
        // - dividerCount one-cell divider lines
        const int availableCellSpace = requestedInteriorSize - dividerCount;
        if (availableCellSpace < segmentCount)
        {
            return layout;
        }

        const int cellSize = availableCellSpace / segmentCount;
        if (cellSize <= 0)
        {
            return layout;
        }

        layout.usedSize = 2 + (segmentCount * cellSize) + dividerCount;
        layout.offsets.reserve(static_cast<std::size_t>(dividerCount));

        int offset = cellSize;
        for (int i = 0; i < dividerCount; ++i)
        {
            layout.offsets.push_back(offset);
            offset += cellSize + 1;
        }

        return layout;
    }

    std::vector<int> normalizeExplicitOffsets(const std::vector<int>& offsets, int interiorSize)
    {
        std::vector<int> normalized;
        normalized.reserve(offsets.size());

        for (int offset : offsets)
        {
            if (offset > 0 && offset < interiorSize)
            {
                normalized.push_back(offset);
            }
        }

        std::sort(normalized.begin(), normalized.end());
        normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
        return normalized;
    }

    bool containsOffset(const std::vector<int>& offsets, int value)
    {
        return std::binary_search(offsets.begin(), offsets.end(), value);
    }

    std::vector<std::u32string> buildGridLines(
        int columns,
        int rows,
        int cellWidth,
        int cellHeight,
        const ObjectFactory::LineGlyphs& glyphs)
    {
        if (columns <= 0 || rows <= 0 || cellWidth <= 0 || cellHeight <= 0)
        {
            return {};
        }

        const int gridWidth = columns * cellWidth + (columns + 1);
        const int gridHeight = rows * cellHeight + (rows + 1);

        std::vector<std::u32string> lines(
            static_cast<std::size_t>(gridHeight),
            std::u32string(static_cast<std::size_t>(gridWidth), U' '));

        auto isVerticalBoundary = [cellWidth](int x) -> bool
            {
                return x % (cellWidth + 1) == 0;
            };

        auto isHorizontalBoundary = [cellHeight](int y) -> bool
            {
                return y % (cellHeight + 1) == 0;
            };

        for (int y = 0; y < gridHeight; ++y)
        {
            for (int x = 0; x < gridWidth; ++x)
            {
                const bool onVertical = isVerticalBoundary(x);
                const bool onHorizontal = isHorizontalBoundary(y);

                char32_t glyph = U' ';

                if (onVertical && onHorizontal)
                {
                    const bool top = (y == 0);
                    const bool bottom = (y == gridHeight - 1);
                    const bool left = (x == 0);
                    const bool right = (x == gridWidth - 1);

                    if (top && left)
                    {
                        glyph = glyphs.topLeft;
                    }
                    else if (top && right)
                    {
                        glyph = glyphs.topRight;
                    }
                    else if (bottom && left)
                    {
                        glyph = glyphs.bottomLeft;
                    }
                    else if (bottom && right)
                    {
                        glyph = glyphs.bottomRight;
                    }
                    else if (top)
                    {
                        glyph = glyphs.teeUp;
                    }
                    else if (bottom)
                    {
                        glyph = glyphs.teeDown;
                    }
                    else if (left)
                    {
                        glyph = glyphs.teeLeft;
                    }
                    else if (right)
                    {
                        glyph = glyphs.teeRight;
                    }
                    else
                    {
                        glyph = glyphs.cross;
                    }
                }
                else if (onHorizontal)
                {
                    glyph = glyphs.horizontal;
                }
                else if (onVertical)
                {
                    glyph = glyphs.vertical;
                }

                lines[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = glyph;
            }
        }

        return lines;
    }

    std::vector<std::u32string> buildDividerBoxLinesFromOffsets(
        int width,
        int height,
        const std::vector<int>& columnOffsets,
        const std::vector<int>& rowOffsets,
        char32_t fillGlyph,
        const ObjectFactory::LineGlyphs& glyphs)
    {
        if (width <= 0 || height <= 0)
        {
            return {};
        }

        std::vector<std::u32string> lines(
            static_cast<std::size_t>(height),
            std::u32string(static_cast<std::size_t>(width), fillGlyph));

        if (width == 1 && height == 1)
        {
            lines[0][0] = glyphs.topLeft;
            return lines;
        }

        const int interiorWidth = std::max(0, width - 2);
        const int interiorHeight = std::max(0, height - 2);

        const std::vector<int> verticalPositions = normalizeExplicitOffsets(columnOffsets, interiorWidth);
        const std::vector<int> horizontalPositions = normalizeExplicitOffsets(rowOffsets, interiorHeight);

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const bool top = (y == 0);
                const bool bottom = (y == height - 1);
                const bool left = (x == 0);
                const bool right = (x == width - 1);

                const bool onOuterVertical = left || right;
                const bool onOuterHorizontal = top || bottom;

                const int interiorX = x - 1;
                const int interiorY = y - 1;

                const bool onInnerVertical =
                    !onOuterVertical &&
                    !onOuterHorizontal &&
                    containsOffset(verticalPositions, interiorX);

                const bool onInnerHorizontal =
                    !onOuterVertical &&
                    !onOuterHorizontal &&
                    containsOffset(horizontalPositions, interiorY);

                const bool topJoin = top && containsOffset(verticalPositions, interiorX);
                const bool bottomJoin = bottom && containsOffset(verticalPositions, interiorX);
                const bool leftJoin = left && containsOffset(horizontalPositions, interiorY);
                const bool rightJoin = right && containsOffset(horizontalPositions, interiorY);

                char32_t glyph = lines[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];

                if (top && left)
                {
                    glyph = glyphs.topLeft;
                }
                else if (top && right)
                {
                    glyph = glyphs.topRight;
                }
                else if (bottom && left)
                {
                    glyph = glyphs.bottomLeft;
                }
                else if (bottom && right)
                {
                    glyph = glyphs.bottomRight;
                }
                else if (topJoin)
                {
                    glyph = glyphs.teeUp;
                }
                else if (bottomJoin)
                {
                    glyph = glyphs.teeDown;
                }
                else if (leftJoin)
                {
                    glyph = glyphs.teeLeft;
                }
                else if (rightJoin)
                {
                    glyph = glyphs.teeRight;
                }
                else if (top || bottom)
                {
                    glyph = glyphs.horizontal;
                }
                else if (left || right)
                {
                    glyph = glyphs.vertical;
                }

                if (onInnerHorizontal)
                {
                    glyph = glyphs.horizontal;
                }

                if (onInnerVertical)
                {
                    glyph = glyphs.vertical;
                }

                if (onInnerHorizontal && onInnerVertical)
                {
                    glyph = glyphs.cross;
                }

                lines[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = glyph;
            }
        }

        return lines;
    }
    int maxPatternWidth(const PatternTile& tile)
    {
        int width = 0;

        for (const std::u32string& row : tile.rows)
        {
            width = std::max(width, static_cast<int>(row.size()));
        }

        return width;
    }

    PatternTile normalizePatternTile(const PatternTile& tile)
    {
        if (tile.rows.empty())
        {
            return {};
        }

        const int tileWidth = maxPatternWidth(tile);
        if (tileWidth <= 0)
        {
            return {};
        }

        PatternTile normalized;
        normalized.capMode = tile.capMode;
        normalized.rows.reserve(tile.rows.size());

        for (const std::u32string& row : tile.rows)
        {
            std::u32string padded = row;
            if (static_cast<int>(padded.size()) < tileWidth)
            {
                padded.append(
                    static_cast<std::size_t>(tileWidth - static_cast<int>(padded.size())),
                    U' ');
            }

            normalized.rows.push_back(std::move(padded));
        }

        return normalized;
    }

    TextObjectBuilder makeAuthoredRectBuilder(
        int width,
        int height,
        const std::optional<Style>& style)
    {
        TextObjectBuilder builder(width, height);
        builder.fillAuthoredSpace(style);
        return builder;
    }

    void writePatternRowToBuilder(
        TextObjectBuilder& builder,
        int objectWidth,
        int objectHeight,
        int startX,
        int destY,
        int regionWidth,
        const std::u32string& sourceRow,
        int tileWidth,
        const std::optional<Style>& style)
    {
        if (regionWidth <= 0 || tileWidth <= 0)
        {
            return;
        }

        if (destY < 0 || destY >= objectHeight)
        {
            return;
        }

        for (int x = 0; x < regionWidth; ++x)
        {
            const int destX = startX + x;
            if (destX < 0 || destX >= objectWidth)
            {
                continue;
            }

            const char32_t glyph = sourceRow[static_cast<std::size_t>(x % tileWidth)];

            if (style.has_value())
            {
                builder.setGlyph(destX, destY, glyph, *style);
            }
            else
            {
                builder.setGlyph(destX, destY, glyph);
            }
        }
    }

    void writePatternFillRegion(
        TextObjectBuilder& builder,
        int objectWidth,
        int objectHeight,
        int startX,
        int startY,
        int regionWidth,
        int regionHeight,
        const PatternTile& tile,
        const std::optional<Style>& style)
    {
        if (regionWidth <= 0 || regionHeight <= 0)
        {
            return;
        }

        const PatternTile normalizedTile = normalizePatternTile(tile);
        if (normalizedTile.rows.empty())
        {
            return;
        }

        const int tileHeight = static_cast<int>(normalizedTile.rows.size());
        const int tileWidth = static_cast<int>(normalizedTile.rows.front().size());

        if (tileHeight <= 0 || tileWidth <= 0)
        {
            return;
        }

        int repeatStart = 0;
        int repeatEndExclusive = tileHeight;

        switch (normalizedTile.capMode)
        {
        case PatternCapMode::None:
            repeatStart = 0;
            repeatEndExclusive = tileHeight;
            break;

        case PatternCapMode::Top:
            repeatStart = 1;
            repeatEndExclusive = tileHeight;
            break;

        case PatternCapMode::Bottom:
            repeatStart = 0;
            repeatEndExclusive = tileHeight - 1;
            break;

        case PatternCapMode::TopAndBottom:
            repeatStart = 1;
            repeatEndExclusive = tileHeight - 1;
            break;
        }

        const bool hasTopCap =
            normalizedTile.capMode == PatternCapMode::Top ||
            normalizedTile.capMode == PatternCapMode::TopAndBottom;

        const bool hasBottomCap =
            normalizedTile.capMode == PatternCapMode::Bottom ||
            normalizedTile.capMode == PatternCapMode::TopAndBottom;

        const int repeatRowCount = std::max(0, repeatEndExclusive - repeatStart);

        int localY = 0;

        if (hasTopCap&& localY < regionHeight)
        {
            writePatternRowToBuilder(
                builder,
                objectWidth,
                objectHeight,
                startX,
                startY + localY,
                regionWidth,
                normalizedTile.rows.front(),
                tileWidth,
                style);

            ++localY;
        }

        const int lastLocalRow = regionHeight - 1;
        const bool reserveBottomRow = hasBottomCap && regionHeight > 0;
        const int repeatableEndY = reserveBottomRow ? lastLocalRow : regionHeight;

        if (repeatRowCount > 0)
        {
            while (localY < repeatableEndY)
            {
                const int repeatIndex = (localY - (hasTopCap ? 1 : 0)) % repeatRowCount;
                const int sourceIndex = repeatStart + repeatIndex;

                writePatternRowToBuilder(
                    builder,
                    objectWidth,
                    objectHeight,
                    startX,
                    startY + localY,
                    regionWidth,
                    normalizedTile.rows[static_cast<std::size_t>(sourceIndex)],
                    tileWidth,
                    style);

                ++localY;
            }
        }
        else
        {
            while (localY < repeatableEndY)
            {
                const std::u32string& fallbackRow =
                    hasTopCap ? normalizedTile.rows.front() : normalizedTile.rows.back();

                writePatternRowToBuilder(
                    builder,
                    objectWidth,
                    objectHeight,
                    startX,
                    startY + localY,
                    regionWidth,
                    fallbackRow,
                    tileWidth,
                    style);

                ++localY;
            }
        }

        if (hasBottomCap && regionHeight > 0)
        {
            writePatternRowToBuilder(
                builder,
                objectWidth,
                objectHeight,
                startX,
                startY + lastLocalRow,
                regionWidth,
                normalizedTile.rows.back(),
                tileWidth,
                style);
        }
    }

    TextObject buildPatternFillObject(
        int width,
        int height,
        const PatternTile& tile,
        const std::optional<Style>& style)
    {
        if (width <= 0 || height <= 0)
        {
            return TextObject();
        }

        const PatternTile normalizedTile = normalizePatternTile(tile);
        if (normalizedTile.rows.empty())
        {
            return TextObject();
        }

        TextObjectBuilder builder = makeAuthoredRectBuilder(width, height, style);

        writePatternFillRegion(
            builder,
            width,
            height,
            0,
            0,
            width,
            height,
            normalizedTile,
            style);

        return builder.build();
    }

    int maxRowWidth(const std::vector<std::u32string>& rows)
    {
        int width = 0;

        for (const std::u32string& row : rows)
        {
            width = std::max(width, static_cast<int>(row.size()));
        }

        return width;
    }

    std::vector<std::u32string> normalizeRows(
        const std::vector<std::u32string>& rows,
        int targetHeight)
    {
        std::vector<std::u32string> normalized;

        if (targetHeight <= 0)
        {
            return normalized;
        }

        const int segmentWidth = maxRowWidth(rows);
        normalized.reserve(static_cast<std::size_t>(targetHeight));

        for (int y = 0; y < targetHeight; ++y)
        {
            std::u32string row;

            if (y < static_cast<int>(rows.size()))
            {
                row = rows[static_cast<std::size_t>(y)];
            }

            if (static_cast<int>(row.size()) < segmentWidth)
            {
                row.append(
                    static_cast<std::size_t>(segmentWidth - static_cast<int>(row.size())),
                    U' ');
            }

            normalized.push_back(std::move(row));
        }

        return normalized;
    }

    struct NormalizedHorizontalLinePattern
    {
        std::vector<std::u32string> beginRows;
        std::vector<std::u32string> repeatRows;
        std::vector<std::u32string> endRows;

        int height = 0;
        int beginWidth = 0;
        int repeatWidth = 0;
        int endWidth = 0;
    };

    NormalizedHorizontalLinePattern normalizeHorizontalLinePattern(
        const HorizontalLinePattern& pattern)
    {
        NormalizedHorizontalLinePattern normalized;

        const int beginHeight = static_cast<int>(pattern.beginRows.size());
        const int repeatHeight = static_cast<int>(pattern.repeatRows.size());
        const int endHeight = static_cast<int>(pattern.endRows.size());

        normalized.height = std::max({ beginHeight, repeatHeight, endHeight });
        if (normalized.height <= 0)
        {
            return normalized;
        }

        normalized.beginRows = normalizeRows(pattern.beginRows, normalized.height);
        normalized.repeatRows = normalizeRows(pattern.repeatRows, normalized.height);
        normalized.endRows = normalizeRows(pattern.endRows, normalized.height);

        normalized.beginWidth = maxRowWidth(normalized.beginRows);
        normalized.repeatWidth = maxRowWidth(normalized.repeatRows);
        normalized.endWidth = maxRowWidth(normalized.endRows);

        return normalized;
    }

    void writeRowsToBuilder(
        TextObjectBuilder& builder,
        int objectWidth,
        int objectHeight,
        int startX,
        int startY,
        const std::vector<std::u32string>& rows,
        const std::optional<Style>& style)
    {
        for (int rowIndex = 0; rowIndex < static_cast<int>(rows.size()); ++rowIndex)
        {
            const int destY = startY + rowIndex;
            if (destY < 0 || destY >= objectHeight)
            {
                continue;
            }

            const std::u32string& row = rows[static_cast<std::size_t>(rowIndex)];

            for (int x = 0; x < static_cast<int>(row.size()); ++x)
            {
                const int destX = startX + x;
                if (destX < 0 || destX >= objectWidth)
                {
                    continue;
                }

                const char32_t glyph = row[static_cast<std::size_t>(x)];

                if (style.has_value())
                {
                    builder.setGlyph(destX, destY, glyph, *style);
                }
                else
                {
                    builder.setGlyph(destX, destY, glyph);
                }
            }
        }
    }

    struct NormalizedVerticalLinePattern
    {
        std::vector<std::u32string> topRows;
        std::vector<std::u32string> repeatRows;
        std::vector<std::u32string> bottomRows;

        int width = 0;
        int topHeight = 0;
        int repeatHeight = 0;
        int bottomHeight = 0;
    };

    std::vector<std::u32string> normalizeRowsToWidth(
        const std::vector<std::u32string>& rows,
        int targetWidth)
    {
        std::vector<std::u32string> normalized;
        normalized.reserve(rows.size());

        for (const std::u32string& row : rows)
        {
            std::u32string padded = row;
            if (static_cast<int>(padded.size()) < targetWidth)
            {
                padded.append(
                    static_cast<std::size_t>(targetWidth - static_cast<int>(padded.size())),
                    U' ');
            }

            normalized.push_back(std::move(padded));
        }

        return normalized;
    }

    NormalizedVerticalLinePattern normalizeVerticalLinePattern(
        const VerticalLinePattern& pattern)
    {
        NormalizedVerticalLinePattern normalized;

        normalized.width = std::max({
            maxRowWidth(pattern.topRows),
            maxRowWidth(pattern.repeatRows),
            maxRowWidth(pattern.bottomRows)
            });

        if (normalized.width <= 0)
        {
            return normalized;
        }

        normalized.topRows = normalizeRowsToWidth(pattern.topRows, normalized.width);
        normalized.repeatRows = normalizeRowsToWidth(pattern.repeatRows, normalized.width);
        normalized.bottomRows = normalizeRowsToWidth(pattern.bottomRows, normalized.width);

        normalized.topHeight = static_cast<int>(normalized.topRows.size());
        normalized.repeatHeight = static_cast<int>(normalized.repeatRows.size());
        normalized.bottomHeight = static_cast<int>(normalized.bottomRows.size());

        return normalized;
    }

    struct NormalizedFramePattern
    {
        std::vector<std::u32string> topLeftRows;
        std::vector<std::u32string> topRows;
        std::vector<std::u32string> topRightRows;

        std::vector<std::u32string> leftRows;
        std::vector<std::u32string> rightRows;

        std::vector<std::u32string> bottomLeftRows;
        std::vector<std::u32string> bottomRows;
        std::vector<std::u32string> bottomRightRows;

        int topLeftWidth = 0;
        int topWidth = 0;
        int topRightWidth = 0;

        int leftWidth = 0;
        int rightWidth = 0;

        int bottomLeftWidth = 0;
        int bottomWidth = 0;
        int bottomRightWidth = 0;

        int topHeight = 0;
        int middleHeight = 0;
        int bottomHeight = 0;
    };

    std::vector<std::u32string> normalizeRowsToHeightAndWidth(
        const std::vector<std::u32string>& rows,
        int targetHeight,
        int targetWidth)
    {
        std::vector<std::u32string> normalized;
        normalized.reserve(static_cast<std::size_t>(targetHeight));

        for (int y = 0; y < targetHeight; ++y)
        {
            std::u32string row;
            if (y < static_cast<int>(rows.size()))
            {
                row = rows[static_cast<std::size_t>(y)];
            }

            if (static_cast<int>(row.size()) < targetWidth)
            {
                row.append(
                    static_cast<std::size_t>(targetWidth - static_cast<int>(row.size())),
                    U' ');
            }

            normalized.push_back(std::move(row));
        }

        return normalized;
    }

    NormalizedFramePattern normalizeFramePattern(const FramePattern& pattern)
    {
        NormalizedFramePattern normalized;

        normalized.topLeftWidth = maxRowWidth(pattern.topLeftRows);
        normalized.topWidth = maxRowWidth(pattern.topRows);
        normalized.topRightWidth = maxRowWidth(pattern.topRightRows);

        normalized.leftWidth = maxRowWidth(pattern.leftRows);
        normalized.rightWidth = maxRowWidth(pattern.rightRows);

        normalized.bottomLeftWidth = maxRowWidth(pattern.bottomLeftRows);
        normalized.bottomWidth = maxRowWidth(pattern.bottomRows);
        normalized.bottomRightWidth = maxRowWidth(pattern.bottomRightRows);

        normalized.topHeight = std::max({
            static_cast<int>(pattern.topLeftRows.size()),
            static_cast<int>(pattern.topRows.size()),
            static_cast<int>(pattern.topRightRows.size())
            });

        normalized.middleHeight = std::max(
            static_cast<int>(pattern.leftRows.size()),
            static_cast<int>(pattern.rightRows.size()));

        normalized.bottomHeight = std::max({
            static_cast<int>(pattern.bottomLeftRows.size()),
            static_cast<int>(pattern.bottomRows.size()),
            static_cast<int>(pattern.bottomRightRows.size())
            });

        normalized.topLeftRows =
            normalizeRowsToHeightAndWidth(pattern.topLeftRows, normalized.topHeight, normalized.topLeftWidth);
        normalized.topRows =
            normalizeRowsToHeightAndWidth(pattern.topRows, normalized.topHeight, normalized.topWidth);
        normalized.topRightRows =
            normalizeRowsToHeightAndWidth(pattern.topRightRows, normalized.topHeight, normalized.topRightWidth);

        normalized.leftRows =
            normalizeRowsToWidth(pattern.leftRows, normalized.leftWidth);
        normalized.rightRows =
            normalizeRowsToWidth(pattern.rightRows, normalized.rightWidth);

        normalized.bottomLeftRows =
            normalizeRowsToHeightAndWidth(pattern.bottomLeftRows, normalized.bottomHeight, normalized.bottomLeftWidth);
        normalized.bottomRows =
            normalizeRowsToHeightAndWidth(pattern.bottomRows, normalized.bottomHeight, normalized.bottomWidth);
        normalized.bottomRightRows =
            normalizeRowsToHeightAndWidth(pattern.bottomRightRows, normalized.bottomHeight, normalized.bottomRightWidth);

        return normalized;
    }

    struct HorizontalAppendLayout
    {
        int beginX = 0;
        int repeatStartX = 0;
        int repeatCount = 0;
        int endX = -1;
        bool hasEnd = false;
        int usedWidth = 0;
    };

    HorizontalAppendLayout computeHorizontalAppendLayout(
        int targetWidth,
        int beginWidth,
        int repeatWidth,
        int endWidth)
    {
        HorizontalAppendLayout layout;

        if (targetWidth <= 0)
        {
            return layout;
        }

        int x = 0;

        if (beginWidth > 0)
        {
            x += beginWidth;
        }

        layout.repeatStartX = x;

        int reservedEndWidth = 0;
        if (endWidth > 0 && x + endWidth <= targetWidth)
        {
            reservedEndWidth = endWidth;
        }

        const int repeatLimit = targetWidth - reservedEndWidth;

        if (repeatWidth > 0)
        {
            while (x + repeatWidth <= repeatLimit)
            {
                ++layout.repeatCount;
                x += repeatWidth;
            }
        }

        if (endWidth > 0 && x + endWidth <= targetWidth)
        {
            layout.hasEnd = true;
            layout.endX = x;
            layout.usedWidth = x + endWidth;
        }
        else
        {
            layout.usedWidth = x;
        }

        return layout;
    }

    /*
 Horizontal pattern line behavior:

 - BEGIN is written once at the left.
 - REPEAT tiles in full blocks only.
 - END is written only if it fully fits.
 - The repeat region may truncate to preserve a complete END.
 - Partial END rendering is intentionally not supported.
*/

    TextObject buildHorizontalPatternLineObject(
        int width,
        const HorizontalLinePattern& pattern,
        const std::optional<Style>& style)
    {
        if (width <= 0)
        {
            return TextObject();
        }

        const NormalizedHorizontalLinePattern normalized =
            normalizeHorizontalLinePattern(pattern);

        if (normalized.height <= 0)
        {
            return TextObject();
        }

        if (normalized.beginWidth <= 0 &&
            normalized.repeatWidth <= 0 &&
            normalized.endWidth <= 0)
        {
            return TextObject();
        }

        const HorizontalAppendLayout layout = computeHorizontalAppendLayout(
            width,
            normalized.beginWidth,
            normalized.repeatWidth,
            normalized.endWidth);

        const int objectWidth = std::min(width, layout.usedWidth);
        if (objectWidth <= 0)
        {
            return TextObject();
        }

        TextObjectBuilder builder =
            makeAuthoredRectBuilder(objectWidth, normalized.height, style);

        if (normalized.beginWidth > 0)
        {
            writeRowsToBuilder(
                builder,
                objectWidth,
                normalized.height,
                0,
                0,
                normalized.beginRows,
                style);
        }

        if (normalized.repeatWidth > 0)
        {
            int x = layout.repeatStartX;

            for (int i = 0; i < layout.repeatCount; ++i)
            {
                writeRowsToBuilder(
                    builder,
                    objectWidth,
                    normalized.height,
                    x,
                    0,
                    normalized.repeatRows,
                    style);

                x += normalized.repeatWidth;
            }
        }

        if (layout.hasEnd && normalized.endWidth > 0)
        {
            writeRowsToBuilder(
                builder,
                objectWidth,
                normalized.height,
                layout.endX,
                0,
                normalized.endRows,
                style);
        }

        return builder.build();
    }

    struct VerticalAppendLayout
    {
        int topY = 0;
        int repeatStartY = 0;
        int repeatCount = 0;
        int bottomY = -1;
        bool hasBottom = false;
        int usedHeight = 0;
    };

    VerticalAppendLayout computeVerticalAppendLayout(
        int targetHeight,
        int topHeight,
        int repeatHeight,
        int bottomHeight)
    {
        VerticalAppendLayout layout;

        if (targetHeight <= 0)
        {
            return layout;
        }

        int y = 0;

        if (topHeight > 0)
        {
            y += topHeight;
        }

        layout.repeatStartY = y;

        int reservedBottomHeight = 0;
        if (bottomHeight > 0 && y + bottomHeight <= targetHeight)
        {
            reservedBottomHeight = bottomHeight;
        }

        const int repeatLimit = targetHeight - reservedBottomHeight;

        if (repeatHeight > 0)
        {
            while (y + repeatHeight <= repeatLimit)
            {
                ++layout.repeatCount;
                y += repeatHeight;
            }
        }

        if (bottomHeight > 0 && y + bottomHeight <= targetHeight)
        {
            layout.hasBottom = true;
            layout.bottomY = y;
            layout.usedHeight = y + bottomHeight;
        }
        else
        {
            layout.usedHeight = y;
        }

        return layout;
    }

    TextObject buildVerticalPatternLineObject(
        int height,
        const VerticalLinePattern& pattern,
        const std::optional<Style>& style)
    {
        if (height <= 0)
        {
            return TextObject();
        }

        const NormalizedVerticalLinePattern normalized =
            normalizeVerticalLinePattern(pattern);

        if (normalized.width <= 0)
        {
            return TextObject();
        }

        if (normalized.topHeight <= 0 &&
            normalized.repeatHeight <= 0 &&
            normalized.bottomHeight <= 0)
        {
            return TextObject();
        }

        const VerticalAppendLayout layout = computeVerticalAppendLayout(
            height,
            normalized.topHeight,
            normalized.repeatHeight,
            normalized.bottomHeight);

        const int objectHeight = std::min(height, layout.usedHeight);
        if (objectHeight <= 0)
        {
            return TextObject();
        }

        TextObjectBuilder builder =
            makeAuthoredRectBuilder(normalized.width, objectHeight, style);

        if (normalized.topHeight > 0)
        {
            writeRowsToBuilder(
                builder,
                normalized.width,
                objectHeight,
                0,
                0,
                normalized.topRows,
                style);
        }

        if (normalized.repeatHeight > 0)
        {
            int y = layout.repeatStartY;

            for (int i = 0; i < layout.repeatCount; ++i)
            {
                writeRowsToBuilder(
                    builder,
                    normalized.width,
                    objectHeight,
                    0,
                    y,
                    normalized.repeatRows,
                    style);

                y += normalized.repeatHeight;
            }
        }

        if (layout.hasBottom && normalized.bottomHeight > 0)
        {
            writeRowsToBuilder(
                builder,
                normalized.width,
                objectHeight,
                0,
                layout.bottomY,
                normalized.bottomRows,
                style);
        }

        return builder.build();
    }



    TextObject buildPatternFrameObject(
        int width,
        int height,
        const FramePattern& pattern,
        const std::optional<Style>& style)
    {
        if (width <= 0 || height <= 0)
        {
            return TextObject();
        }

        const NormalizedFramePattern normalized = normalizeFramePattern(pattern);

        // Compute polished top-edge layout first.
        const HorizontalAppendLayout topLayout = computeHorizontalAppendLayout(
            width,
            normalized.topLeftWidth,
            normalized.topWidth,
            normalized.topRightWidth);

        int frameUsedWidth = topLayout.usedWidth;

        // If the top produced nothing, fall back to bottom widths or side widths.
        if (frameUsedWidth <= 0)
        {
            const HorizontalAppendLayout bottomFallback = computeHorizontalAppendLayout(
                width,
                normalized.bottomLeftWidth,
                normalized.bottomWidth,
                normalized.bottomRightWidth);

            frameUsedWidth = bottomFallback.usedWidth;
        }

        if (frameUsedWidth <= 0)
        {
            frameUsedWidth = std::max({
                normalized.topLeftWidth,
                normalized.topRightWidth,
                normalized.leftWidth,
                normalized.rightWidth,
                normalized.bottomLeftWidth,
                normalized.bottomRightWidth
                });
        }

        frameUsedWidth = std::min(frameUsedWidth, width);

        // Compute polished vertical layout.
        const VerticalAppendLayout verticalLayout = computeVerticalAppendLayout(
            height,
            normalized.topHeight,
            normalized.middleHeight,
            normalized.bottomHeight);

        int frameUsedHeight = verticalLayout.usedHeight;
        if (frameUsedHeight <= 0)
        {
            frameUsedHeight = std::max(normalized.topHeight, normalized.bottomHeight);
        }

        frameUsedHeight = std::min(frameUsedHeight, height);

        if (frameUsedWidth <= 0 || frameUsedHeight <= 0)
        {
            return TextObject();
        }

        TextObjectBuilder builder =
            makeAuthoredRectBuilder(frameUsedWidth, frameUsedHeight, style);

        // Recompute bottom layout against the actual used frame width so it stays polished too.
        const HorizontalAppendLayout bottomLayout = computeHorizontalAppendLayout(
            frameUsedWidth,
            normalized.bottomLeftWidth,
            normalized.bottomWidth,
            normalized.bottomRightWidth);

        // Top corners
        if (!normalized.topLeftRows.empty())
        {
            writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, 0, 0, normalized.topLeftRows, style);
        }

        if (topLayout.hasEnd && !normalized.topRightRows.empty())
        {
            writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, topLayout.endX, 0, normalized.topRightRows, style);
        }

        // Top horizontal repeat
        if (normalized.topWidth > 0 && normalized.topHeight > 0)
        {
            int x = topLayout.repeatStartX;
            for (int i = 0; i < topLayout.repeatCount; ++i)
            {
                writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, x, 0, normalized.topRows, style);
                x += normalized.topWidth;
            }
        }

        // Bottom corners
        if (!normalized.bottomLeftRows.empty() && verticalLayout.hasBottom)
        {
            writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, 0, verticalLayout.bottomY, normalized.bottomLeftRows, style);
        }

        if (bottomLayout.hasEnd && verticalLayout.hasBottom && !normalized.bottomRightRows.empty())
        {
            writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, bottomLayout.endX, verticalLayout.bottomY, normalized.bottomRightRows, style);
        }

        // Bottom horizontal repeat
        if (normalized.bottomWidth > 0 && normalized.bottomHeight > 0 && verticalLayout.hasBottom)
        {
            int x = bottomLayout.repeatStartX;
            for (int i = 0; i < bottomLayout.repeatCount; ++i)
            {
                writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, x, verticalLayout.bottomY, normalized.bottomRows, style);
                x += normalized.bottomWidth;
            }
        }

        // Left vertical repeat
        if (normalized.leftWidth > 0 && normalized.middleHeight > 0)
        {
            int y = verticalLayout.repeatStartY;
            for (int i = 0; i < verticalLayout.repeatCount; ++i)
            {
                writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, 0, y, normalized.leftRows, style);
                y += normalized.middleHeight;
            }
        }

        // Right vertical repeat, aligned to the polished frame width instead of requested width.
        if (normalized.rightWidth > 0 && normalized.middleHeight > 0)
        {
            const int rightX = std::max(0, frameUsedWidth - normalized.rightWidth);

            int y = verticalLayout.repeatStartY;
            for (int i = 0; i < verticalLayout.repeatCount; ++i)
            {
                writeRowsToBuilder(builder, frameUsedWidth, frameUsedHeight, rightX, y, normalized.rightRows, style);
                y += normalized.middleHeight;
            }
        }

        return builder.build();
    }
}

namespace ObjectFactory
{
    /*
        Things you need to know to properly create these text patterns

        1) Backslashes need to be escaped to print properly
        so if you want to print a backslash you have to "\\" escape it
        this leads to a readability problem. And worse, since we aren't
        printing through cout, the escaped backslash gets drawn as
        \\ breaking the effect altogether.

        2) You can use this pattern to avoid the escaped backslash:
            R"( ... )"

            like in UR"( / __ \ \__)"

        3) You can also use a custom pattern such as:
            UR"PATTERN( ... )PATTERN"
*/

    BorderGlyphs asciiBorder()
    {
        return BorderGlyphs{};
    }

    BorderGlyphs singleLineBorder()
    {
        BorderGlyphs glyphs;
        glyphs.topLeft = U'┌';
        glyphs.topRight = U'┐';
        glyphs.bottomLeft = U'└';
        glyphs.bottomRight = U'┘';
        glyphs.horizontal = U'─';
        glyphs.vertical = U'│';
        return glyphs;
    }

    BorderGlyphs doubleLineBorder()
    {
        BorderGlyphs glyphs;
        glyphs.topLeft = U'╔';
        glyphs.topRight = U'╗';
        glyphs.bottomLeft = U'╚';
        glyphs.bottomRight = U'╝';
        glyphs.horizontal = U'═';
        glyphs.vertical = U'║';
        return glyphs;
    }

    BorderGlyphs roundedBorder()
    {
        BorderGlyphs glyphs;
        glyphs.topLeft = U'╭';
        glyphs.topRight = U'╮';
        glyphs.bottomLeft = U'╰';
        glyphs.bottomRight = U'╯';
        glyphs.horizontal = U'─';
        glyphs.vertical = U'│';
        return glyphs;
    }

    LineGlyphs asciiLineGlyphs()
    {
        return LineGlyphs{};
    }

    LineGlyphs singleLineGlyphs()
    {
        LineGlyphs glyphs;
        glyphs.horizontal = U'─';
        glyphs.vertical = U'│';

        glyphs.topLeft = U'┌';
        glyphs.topRight = U'┐';
        glyphs.bottomLeft = U'└';
        glyphs.bottomRight = U'┘';

        glyphs.teeUp = U'┬';
        glyphs.teeDown = U'┴';
        glyphs.teeLeft = U'├';
        glyphs.teeRight = U'┤';
        glyphs.cross = U'┼';
        return glyphs;
    }

    LineGlyphs doubleLineGlyphs()
    {
        LineGlyphs glyphs;
        glyphs.horizontal = U'═';
        glyphs.vertical = U'║';

        glyphs.topLeft = U'╔';
        glyphs.topRight = U'╗';
        glyphs.bottomLeft = U'╚';
        glyphs.bottomRight = U'╝';

        glyphs.teeUp = U'╦';
        glyphs.teeDown = U'╩';
        glyphs.teeLeft = U'╠';
        glyphs.teeRight = U'╣';
        glyphs.cross = U'╬';
        return glyphs;
    }

    LineGlyphs roundedLineGlyphs()
    {
        LineGlyphs glyphs;
        glyphs.horizontal = U'─';
        glyphs.vertical = U'│';

        glyphs.topLeft = U'╭';
        glyphs.topRight = U'╮';
        glyphs.bottomLeft = U'╰';
        glyphs.bottomRight = U'╯';

        glyphs.teeUp = U'┬';
        glyphs.teeDown = U'┴';
        glyphs.teeLeft = U'├';
        glyphs.teeRight = U'┤';
        glyphs.cross = U'┼';
        return glyphs;
    }

    PatternTile brickPattern()
    {
        return PatternTile
        {
            {
            U"_|___",
            U"___|_"
            },
            PatternCapMode::None
        };
    }

    PatternTile bubblesPattern()
    {
        return PatternTile
        {
            {
            UR"( / __ \ \__/)",
            UR"(/ /  \ \____)",
            UR"(\ \__/ / __ )",
            UR"( \____/ /  \)"
            },
            PatternCapMode::None
        };
    }

    PatternTile crossStitchPattern()
    {
        return PatternTile
        {
            {
            UR"(<>================================<>)",
            UR"(||\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/||)",
            UR"(||<> <> <> <> <> <> <> <> <> <> <>||)",
            UR"(||/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\||)",
            UR"(||================================||)",
            UR"(||<> <> <> <> <> <> <> <> <> <> <>||)",
            UR"(||================================||)",
            UR"(||/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\||)",
            UR"(||<> <> <> <> <> <> <> <> <> <> <>||)",
            UR"(||\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/\/||)",
            UR"(<>================================<>)"
            },
            PatternCapMode::None
        };
    }

    PatternTile crossedPattern()
    {
        return PatternTile
        {
            {
            U"_|_     ",
            U" |      ",
            U"    _|_ ",
            U"     |  "
            },
            PatternCapMode::None
        };
    }

    PatternTile embroideryPattern()
    {
        return PatternTile
        {
            {
            U" /\\ ",
            U" )( ",
            U"(  )",
            U" \\/"
            },
            PatternCapMode::TopAndBottom
        };
    }

    PatternTile embroideryTile()
    {
        return PatternTile
        {
            {
            U".----------------------------.",
            U"|\\  /\\  /\\  /\\  /\\  /\\  /\\  /|",
            U"| )(  )(  )(  )(  )(  )(  )( |",
            U"|(  )(  )(  )(  )(  )(  )(  )|",
            U"| )(  )(  )(  )(  )(  )(  )( |",
            U"|(  )(  )(  )(  )(  )(  )(  )|",
            U"| )(  )(  )(  )(  )(  )(  )( |",
            U"|(  )(  )(  )(  )(  )(  )(  )|",
            U"| )(  )(  )(  )(  )(  )(  )( |",
            U"|(  )(  )(  )(  )(  )(  )(  )|",
            U"| )(  )(  )(  )(  )(  )(  )( |",
            U"|/  \\/  \\/  \\/  \\/  \\/  \\/  \\|",
            U"'----------------------------'"
            },
            PatternCapMode::None
        };
    }

    PatternTile fencePattern()
    {
        return PatternTile
        {
            {
            UR"(   /  \ )",
            UR"(__/    \)",
            UR"(  \    /)",
            UR"(   \__/ )"
            },
            PatternCapMode::None
        };
    }

    PatternTile honeyCombPattern()
    {
        return PatternTile
        {
            {
            UR"(/      \____)",
            UR"(\      /    )",
            UR"( \____/     )",
            UR"( /    \     )"
            },
            PatternCapMode::None
        };
    }

    PatternTile houndsToothPattern()
    {
        return PatternTile
        {
            {
            U"|__|   __",
            U" __|__|  ",
            U"|   __|__"
            },
            PatternCapMode::None
        };
    }

    PatternTile ninjaPattern()
    {
        return PatternTile
        {
            {
            U" |___  |",
            U"    _|_|",
            U"_  | |__",
            U"_|_|    "
            },
            PatternCapMode::None
        };
    }

    PatternTile puzzlePattern()
    {
        return PatternTile
        {
            {
            U" _|    ",
            U"(_   _ ",
            U" |__( )"
            },
            PatternCapMode::None
        };
    }

    HorizontalLinePattern catFaceLinePattern()
    {
        return HorizontalLinePattern
        {
            {},
            {
                U"=^..^=    "
            },
            {
                U"=^..^="
            }
        };
    }

    HorizontalLinePattern pennantLinePattern()
    {
        return HorizontalLinePattern
        {
            {},
            {
                U"     .-.",
                U"`._.'   "
            },
            {}
        };
    }

    HorizontalLinePattern sparkChainLinePattern()
    {
        return HorizontalLinePattern
        {
            {
                U"."
            },
            {
                UR"(+"+.)"
            },
            {}
        };
    }

    HorizontalLinePattern orbChainLinePattern()
    {
        return HorizontalLinePattern
        {
            {
                UR"( /)",
                U"O ",
                UR"( \)"
            },
            {
                UR"(\ /)",
                UR"( \ )",
                UR"(/ \)"
            },
            {
                UR"(\ )",
                U" O",
                UR"(/ )"
            }
        };
    }

    HorizontalLinePattern rightAroundPattern()
    {
        return HorizontalLinePattern
        {
            {},
            {
                UR"(  .--.    )",
                UR"(:::::.\:::)",
                UR"('      `--)"
            },
            {}
        };
    }

    HorizontalLinePattern dnaPattern()
    {
        return HorizontalLinePattern
        {
            {},
            {
                UR"( ,-"-.   ,-"-.)",
                UR"(X | | \ / | | )",
                UR"( \| | |X| | |/)",
                UR"(  `-!-' `-!-" )"
            },
            {
                UR"( )",
                UR"(X)",
                UR"( )",
                UR"( )"
            }
        };
    }

    HorizontalLinePattern puzzleLinePattern()
    {
        return HorizontalLinePattern
        {   {
                U"     _ ",
                U"   _( )",
                U" _|    ",
                U"(_   _ ",
                U" |__( )"
            },
            {
                U"     _ ",
                U"__ _( )",
                U" _|    ",
                U"(_   _ ",
                U"_|__( )"
            },
            {
                U"     _    ",
                U"__ _( )__ ",
                U" _|     _|",
                U"(_   _ (_ ",
                U"_|__( )_| "
            }
        };
    }


    HorizontalLinePattern scrollSemetricPattern()
    {
        return HorizontalLinePattern
        {
            {
                UR"(  .------)",
                UR"( /  .-.  )",
                UR"(|  /   \ )",
                UR"(| |\_.  |)",
                UR"(|\|  | /|)",
                UR"(| `---' |)",
                UR"(|       |)",
                UR"(|       |)",
                UR"(\       |)",
                UR"( \     / )",
                UR"(  `---'  )"            },
            {
                U"-",
                U" ",
                U" ",
                U" ",
                U" ",
                U" ",
                U" ",
                U"-"
            },
            {
                UR"(------.  )",
                UR"(  .-.  \ )",
                UR"( /   \  |)",
                UR"(|    /| |)",
                UR"(|\  | |/|)",
                UR"(| `---' |)",
                UR"(|       |)",
                UR"(|       |)",
                UR"(|       /)",
                UR"( \     / )",
                UR"(  `---'  )"            }
        };
    }

    VerticalLinePattern heartChainVerticalPattern()
    {
        return VerticalLinePattern
        {
            {
                UR"PATTERN( .-"-,-"-. )PATTERN",
                UR"PATTERN((         ))PATTERN",
                UR"PATTERN( `.     .' )PATTERN",
                UR"PATTERN(   `._.'   )PATTERN",
                UR"PATTERN(   ( (     )PATTERN"
            },
            {
                UR"PATTERN(    ) )    )PATTERN",
                UR"PATTERN( .-"-,-"-. )PATTERN",
                UR"PATTERN((         ))PATTERN",
                UR"PATTERN( `.     .' )PATTERN",
                UR"PATTERN(   `._.'   )PATTERN",
                UR"PATTERN(   ( (     )PATTERN"
            },
            {
                UR"PATTERN(    ) )    )PATTERN",
                UR"PATTERN( .-"-,-"-. )PATTERN",
                UR"PATTERN((         ))PATTERN",
                UR"PATTERN( `.     .' )PATTERN",
                UR"PATTERN(   `._.'   )PATTERN"
            }
        };
    }

    // In order for the FramePatterns to be built correctly
    // The corner pieces must be the same width as the column pieces
    // and the same height as the row pieces

    FramePattern heartFramePattern()
    {
        return FramePattern
        {
            {
                UR"PATTERN(           )PATTERN",
                UR"PATTERN(       _.-.)PATTERN",
                UR"PATTERN(     .'_.-.)PATTERN",
                UR"PATTERN(    ( (    )PATTERN"
            },
            {
                UR"PATTERN(     .-"-,-"-.     )PATTERN",
                UR"PATTERN(.-._(         ).-._)PATTERN",
                UR"PATTERN(.-._.`.     .'_.-._)PATTERN",
                UR"PATTERN(       `._.'       )PATTERN"
            },
            {
                UR"PATTERN(           )PATTERN",
                UR"PATTERN(.-..       )PATTERN",
                UR"PATTERN(.-..`.     )PATTERN",
                UR"PATTERN(    ) )    )PATTERN"
            },

            {
                UR"PATTERN(     ) )   )PATTERN",
                UR"PATTERN( .-"-,-"-. )PATTERN",
                UR"PATTERN((         ))PATTERN",
                UR"PATTERN( `.     .' )PATTERN",
                UR"PATTERN(   `._.'   )PATTERN",
                UR"PATTERN(    ( (    )PATTERN"
            },
            {
                UR"PATTERN(   ( (     )PATTERN",
                UR"PATTERN( .-"-,-"-. )PATTERN",
                UR"PATTERN((         ))PATTERN",
                UR"PATTERN( `.     .' )PATTERN",
                UR"PATTERN(   `._.'   )PATTERN",
                UR"PATTERN(    ) )    )PATTERN"
            },

            {
                UR"PATTERN(    ) )    )PATTERN",
                UR"PATTERN(   ( (_.-..)PATTERN",
                UR"PATTERN(    `._.-..)PATTERN",
                UR"PATTERN(           )PATTERN"
            },
            {
                UR"PATTERN(     .-"-,-"-.     )PATTERN",
                UR"PATTERN(.-._(         ).-._)PATTERN",
                UR"PATTERN(.-._.`.     .'_.-._)PATTERN",
                UR"PATTERN(       `._.'       )PATTERN"
            },
            {
                UR"PATTERN(   ( (     )PATTERN",
                UR"PATTERN(.-._) )    )PATTERN",
                UR"PATTERN(.-._,'     )PATTERN",
                UR"PATTERN(           )PATTERN"
            }
        };
    }

    TextObject makeText(std::u32string_view text)
    {
        return TextObject::fromU32(text);
    }

    TextObject makeText(std::u32string_view text, const Style& style)
    {
        return TextObject::fromU32(text, style);
    }

    TextObject makeTextUtf8(std::string_view text)
    {
        return TextObject::fromUtf8(text);
    }

    TextObject makeTextUtf8(std::string_view text, const Style& style)
    {
        return TextObject::fromUtf8(text, style);
    }

    TextObject makeFilledRect(int width, int height, char32_t fillGlyph)
    {
        return buildObjectFromLines(buildFilledRectLines(width, height, fillGlyph), std::nullopt);
    }

    TextObject makeFilledRect(int width, int height, char32_t fillGlyph, const Style& style)
    {
        return buildObjectFromLines(buildFilledRectLines(width, height, fillGlyph), style);
    }

    TextObject makeHorizontalLine(int length, char32_t glyph)
    {
        return buildObjectFromLines(buildHorizontalLineLines(length, glyph), std::nullopt);
    }

    TextObject makeHorizontalLine(int length, char32_t glyph, const Style& style)
    {
        return buildObjectFromLines(buildHorizontalLineLines(length, glyph), style);
    }

    TextObject makeVerticalLine(int length, char32_t glyph)
    {
        return buildObjectFromLines(buildVerticalLineLines(length, glyph), std::nullopt);
    }

    TextObject makeVerticalLine(int length, char32_t glyph, const Style& style)
    {
        return buildObjectFromLines(buildVerticalLineLines(length, glyph), style);
    }

    TextObject makeFrame(int width, int height, const BorderGlyphs& border)
    {
        return buildObjectFromLines(buildFrameLines(width, height, border, std::nullopt), std::nullopt);
    }

    TextObject makeFrame(int width, int height, const Style& style, const BorderGlyphs& border)
    {
        return buildObjectFromLines(buildFrameLines(width, height, border, std::nullopt), style);
    }

    TextObject makeBorderedBox(int width, int height, char32_t fillGlyph, const BorderGlyphs& border)
    {
        return buildObjectFromLines(buildFrameLines(width, height, border, fillGlyph), std::nullopt);
    }

    TextObject makeBorderedBox(
        int width,
        int height,
        char32_t fillGlyph,
        const Style& style,
        const BorderGlyphs& border)
    {
        return buildObjectFromLines(buildFrameLines(width, height, border, fillGlyph), style);
    }

    TextObject makeLineRow(int length, const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(buildHorizontalLineLines(length, glyphs.horizontal), std::nullopt);
    }

    TextObject makeLineRow(int length, const Style& style, const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(buildHorizontalLineLines(length, glyphs.horizontal), style);
    }

    TextObject makeLineColumn(int length, const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(buildVerticalLineLines(length, glyphs.vertical), std::nullopt);
    }

    TextObject makeLineColumn(int length, const Style& style, const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(buildVerticalLineLines(length, glyphs.vertical), style);
    }

    TextObject makeJunction(char32_t glyph)
    {
        return TextObject::fromU32(std::u32string(1, glyph));
    }

    TextObject makeJunction(char32_t glyph, const Style& style)
    {
        return TextObject::fromU32(std::u32string(1, glyph), style);
    }

    TextObject makeCross(const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.cross);
    }

    TextObject makeCross(const Style& style, const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.cross, style);
    }

    TextObject makeTeeUp(const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeUp);
    }

    TextObject makeTeeUp(const Style& style, const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeUp, style);
    }

    TextObject makeTeeDown(const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeDown);
    }

    TextObject makeTeeDown(const Style& style, const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeDown, style);
    }

    TextObject makeTeeLeft(const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeLeft);
    }

    TextObject makeTeeLeft(const Style& style, const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeLeft, style);
    }

    TextObject makeTeeRight(const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeRight);
    }

    TextObject makeTeeRight(const Style& style, const LineGlyphs& glyphs)
    {
        return makeJunction(glyphs.teeRight, style);
    }

    TextObject makeGrid(
        int columns,
        int rows,
        int cellWidth,
        int cellHeight,
        const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(
            buildGridLines(columns, rows, cellWidth, cellHeight, glyphs),
            std::nullopt);
    }

    TextObject makeGrid(
        int columns,
        int rows,
        int cellWidth,
        int cellHeight,
        const Style& style,
        const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(
            buildGridLines(columns, rows, cellWidth, cellHeight, glyphs),
            style);
    }

    TextObject makeDividerBox(
        int width,
        int height,
        int verticalDividers,
        int horizontalDividers,
        char32_t fillGlyph,
        const LineGlyphs& glyphs)
    {
        const UniformDividerLayout horizontalLayout =
            computeUniformDividerLayout(width, verticalDividers);

        const UniformDividerLayout verticalLayout =
            computeUniformDividerLayout(height, horizontalDividers);

        return buildObjectFromLines(
            buildDividerBoxLinesFromOffsets(
                horizontalLayout.usedSize,
                verticalLayout.usedSize,
                horizontalLayout.offsets,
                verticalLayout.offsets,
                fillGlyph,
                glyphs),
            std::nullopt);
    }

    TextObject makeDividerBox(
        int width,
        int height,
        int verticalDividers,
        int horizontalDividers,
        char32_t fillGlyph,
        const Style& style,
        const LineGlyphs& glyphs)
    {
        const UniformDividerLayout horizontalLayout =
            computeUniformDividerLayout(width, verticalDividers);

        const UniformDividerLayout verticalLayout =
            computeUniformDividerLayout(height, horizontalDividers);

        return buildObjectFromLines(
            buildDividerBoxLinesFromOffsets(
                horizontalLayout.usedSize,
                verticalLayout.usedSize,
                horizontalLayout.offsets,
                verticalLayout.offsets,
                fillGlyph,
                glyphs),
            style);
    }

    TextObject makeDividerBox(
        int width,
        int height,
        const std::vector<int>& columnOffsets,
        const std::vector<int>& rowOffsets,
        char32_t fillGlyph,
        const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(
            buildDividerBoxLinesFromOffsets(
                width,
                height,
                columnOffsets,
                rowOffsets,
                fillGlyph,
                glyphs),
            std::nullopt);
    }

    TextObject makeDividerBox(
        int width,
        int height,
        const std::vector<int>& columnOffsets,
        const std::vector<int>& rowOffsets,
        char32_t fillGlyph,
        const Style& style,
        const LineGlyphs& glyphs)
    {
        return buildObjectFromLines(
            buildDividerBoxLinesFromOffsets(
                width,
                height,
                columnOffsets,
                rowOffsets,
                fillGlyph,
                glyphs),
            style);
    }

    TextObject makePatternFill(
        int width,
        int height,
        const PatternTile& tile)
    {
        return buildPatternFillObject(width, height, tile, std::nullopt);
    }

    TextObject makePatternFill(
        int width,
        int height,
        const PatternTile& tile,
        const Style& style)
    {
        return buildPatternFillObject(width, height, tile, style);
    }

    TextObject makeHorizontalPatternLine(
        int width,
        const HorizontalLinePattern& pattern)
    {
        return buildHorizontalPatternLineObject(width, pattern, std::nullopt);
    }

    TextObject makeHorizontalPatternLine(
        int width,
        const HorizontalLinePattern& pattern,
        const Style& style)
    {
        return buildHorizontalPatternLineObject(width, pattern, style);
    }

    TextObject makeVerticalPatternLine(
        int height,
        const VerticalLinePattern& pattern)
    {
        return buildVerticalPatternLineObject(height, pattern, std::nullopt);
    }

    TextObject makeVerticalPatternLine(
        int height,
        const VerticalLinePattern& pattern,
        const Style& style)
    {
        return buildVerticalPatternLineObject(height, pattern, style);
    }


    TextObject makePatternFrame(
        int width,
        int height,
        const FramePattern& pattern)
    {
        return buildPatternFrameObject(width, height, pattern, std::nullopt);
    }

    TextObject makePatternFrame(
        int width,
        int height,
        const FramePattern& pattern,
        const Style& style)
    {
        return buildPatternFrameObject(width, height, pattern, style);
    }
}