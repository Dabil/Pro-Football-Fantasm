#include "Rendering/Objects/ObjectFactory.h"

#include <algorithm>
#include <optional>
#include <vector>

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

    std::vector<int> computeDividerPositions(int interiorSize, int dividerCount)
    {
        std::vector<int> positions;

        if (interiorSize <= 0 || dividerCount <= 0)
        {
            return positions;
        }

        positions.reserve(static_cast<std::size_t>(dividerCount));

        const int segmentCount = dividerCount + 1;
        for (int i = 1; i <= dividerCount; ++i)
        {
            const int pos = (i * interiorSize) / segmentCount;

            if (pos > 0 && pos < interiorSize)
            {
                if (positions.empty() || positions.back() != pos)
                {
                    positions.push_back(pos);
                }
            }
        }

        return positions;
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
}

namespace ObjectFactory
{
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

    TextObject makeFigletText(std::u32string_view text)
    {
        return makeText(text);
    }

    TextObject makeFigletText(std::u32string_view text, const Style& style)
    {
        return makeText(text, style);
    }

    TextObject makeFigletTextUtf8(std::string_view text)
    {
        return makeTextUtf8(text);
    }

    TextObject makeFigletTextUtf8(std::string_view text, const Style& style)
    {
        return makeTextUtf8(text, style);
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
        const int interiorWidth = std::max(0, width - 2);
        const int interiorHeight = std::max(0, height - 2);

        return buildObjectFromLines(
            buildDividerBoxLinesFromOffsets(
                width,
                height,
                computeDividerPositions(interiorWidth, verticalDividers),
                computeDividerPositions(interiorHeight, horizontalDividers),
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
        const int interiorWidth = std::max(0, width - 2);
        const int interiorHeight = std::max(0, height - 2);

        return buildObjectFromLines(
            buildDividerBoxLinesFromOffsets(
                width,
                height,
                computeDividerPositions(interiorWidth, verticalDividers),
                computeDividerPositions(interiorHeight, horizontalDividers),
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
}