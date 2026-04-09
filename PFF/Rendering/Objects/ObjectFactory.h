#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace ObjectFactory
{
    struct BorderGlyphs
    {
        char32_t topLeft = U'+';
        char32_t topRight = U'+';
        char32_t bottomLeft = U'+';
        char32_t bottomRight = U'+';
        char32_t horizontal = U'-';
        char32_t vertical = U'|';
    };

    struct LineGlyphs
    {
        char32_t horizontal = U'-';
        char32_t vertical = U'|';

        char32_t topLeft = U'+';
        char32_t topRight = U'+';
        char32_t bottomLeft = U'+';
        char32_t bottomRight = U'+';

        char32_t teeUp = U'+';       // joins left/right/up
        char32_t teeDown = U'+';     // joins left/right/down
        char32_t teeLeft = U'+';     // joins up/down/left
        char32_t teeRight = U'+';    // joins up/down/right
        char32_t cross = U'+';
    };

    BorderGlyphs asciiBorder();
    BorderGlyphs singleLineBorder();
    BorderGlyphs doubleLineBorder();
    BorderGlyphs roundedBorder();

    LineGlyphs asciiLineGlyphs();
    LineGlyphs singleLineGlyphs();
    LineGlyphs doubleLineGlyphs();
    LineGlyphs roundedLineGlyphs();

    TextObject makeText(std::u32string_view text);
    TextObject makeText(std::u32string_view text, const Style& style);

    TextObject makeTextUtf8(std::string_view text);
    TextObject makeTextUtf8(std::string_view text, const Style& style);

    TextObject makeFigletText(std::u32string_view text);
    TextObject makeFigletText(std::u32string_view text, const Style& style);

    TextObject makeFigletTextUtf8(std::string_view text);
    TextObject makeFigletTextUtf8(std::string_view text, const Style& style);

    TextObject makeFilledRect(int width, int height, char32_t fillGlyph = U' ');
    TextObject makeFilledRect(int width, int height, char32_t fillGlyph, const Style& style);

    TextObject makeHorizontalLine(int length, char32_t glyph = U'-');
    TextObject makeHorizontalLine(int length, char32_t glyph, const Style& style);

    TextObject makeVerticalLine(int length, char32_t glyph = U'|');
    TextObject makeVerticalLine(int length, char32_t glyph, const Style& style);

    TextObject makeFrame(
        int width,
        int height,
        const BorderGlyphs& border = asciiBorder());

    TextObject makeFrame(
        int width,
        int height,
        const Style& style,
        const BorderGlyphs& border = asciiBorder());

    TextObject makeBorderedBox(
        int width,
        int height,
        char32_t fillGlyph = U' ',
        const BorderGlyphs& border = asciiBorder());

    TextObject makeBorderedBox(
        int width,
        int height,
        char32_t fillGlyph,
        const Style& style,
        const BorderGlyphs& border = asciiBorder());

    // Line helpers for future layout/composition work.
    TextObject makeLineRow(int length, const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeLineRow(int length, const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeLineColumn(int length, const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeLineColumn(int length, const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeJunction(char32_t glyph);
    TextObject makeJunction(char32_t glyph, const Style& style);

    TextObject makeCross(const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeCross(const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeTeeUp(const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeTeeUp(const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeTeeDown(const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeTeeDown(const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeTeeLeft(const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeTeeLeft(const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeTeeRight(const LineGlyphs& glyphs = asciiLineGlyphs());
    TextObject makeTeeRight(const Style& style, const LineGlyphs& glyphs = asciiLineGlyphs());

    // Creates a rectangular grid shell using line/intersection glyphs.
    // rows and columns represent cell counts, not raw character dimensions.
    TextObject makeGrid(
        int columns,
        int rows,
        int cellWidth,
        int cellHeight,
        const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeGrid(
        int columns,
        int rows,
        int cellWidth,
        int cellHeight,
        const Style& style,
        const LineGlyphs& glyphs = asciiLineGlyphs());

    // Creates a framed box with evenly distributed interior divider lines.
    // verticalDividers and horizontalDividers are counts of interior separator lines.
    TextObject makeDividerBox(
        int width,
        int height,
        int verticalDividers,
        int horizontalDividers,
        char32_t fillGlyph = U' ',
        const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeDividerBox(
        int width,
        int height,
        int verticalDividers,
        int horizontalDividers,
        char32_t fillGlyph,
        const Style& style,
        const LineGlyphs& glyphs = asciiLineGlyphs());

    // Creates a framed box with explicit interior divider positions.
    // columnOffsets and rowOffsets are interior offsets measured from the
    // inside-left and inside-top of the box. Valid interior offsets are:
    //   x: 1..(width - 3)
    //   y: 1..(height - 3)
    TextObject makeDividerBox(
        int width,
        int height,
        const std::vector<int>& columnOffsets,
        const std::vector<int>& rowOffsets,
        char32_t fillGlyph = U' ',
        const LineGlyphs& glyphs = asciiLineGlyphs());

    TextObject makeDividerBox(
        int width,
        int height,
        const std::vector<int>& columnOffsets,
        const std::vector<int>& rowOffsets,
        char32_t fillGlyph,
        const Style& style,
        const LineGlyphs& glyphs = asciiLineGlyphs());
}