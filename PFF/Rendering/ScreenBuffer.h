#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Rect.h"
#include "Rendering/ScreenCell.h"
#include "Rendering/Styles/Style.h"

class ScreenBuffer
{
public:
    ScreenBuffer();
    ScreenBuffer(int width, int height);

    void resize(int width, int height);
    void clear(const Style& style = Style());

    int getWidth() const;
    int getHeight() const;

    bool inBounds(int x, int y) const;

    const ScreenCell& getCell(int x, int y) const;
    const ScreenCell& getCell(int x, int y);

    const ScreenCell* tryGetRowData(int y) const;
    void expandSpanToGlyphBoundaries(int y, int& xStart, int& xEnd) const;

    void setCell(int x, int y, const ScreenCell& cell);
    void setCellStyle(int x, int y, const Style& style);

    void writeCodePoint(int x, int y, char32_t glyph, const Style& style);
    void writeCodePoint(int x, int y, char32_t glyph, const std::optional<Style>& styleOverride);

    void writeText(int x, int y, std::u32string_view text, const Style& style);
    void writeText(int x, int y, std::u32string_view text, const std::optional<Style>& styleOverride);

    void fillRect(const Rect& rect, char32_t glyph, const Style& style);
    void fillRect(const Rect& rect, char32_t glyph, const std::optional<Style>& styleOverride);

    void drawFrame(
        const Rect& rect,
        const Style& style,
        char32_t topLeft = U'+',
        char32_t topRight = U'+',
        char32_t bottomLeft = U'+',
        char32_t bottomRight = U'+',
        char32_t horizontal = U'-',
        char32_t vertical = U'|');

    void drawFrame(
        const Rect& rect,
        const std::optional<Style>& styleOverride,
        char32_t topLeft = U'+',
        char32_t topRight = U'+',
        char32_t bottomLeft = U'+',
        char32_t bottomRight = U'+',
        char32_t horizontal = U'-',
        char32_t vertical = U'|');

    std::u32string renderToU32String() const;
    std::string renderToUtf8String() const;

    void writeChar(int x, int y, char32_t glyph, const Style& style);
    void writeChar(int x, int y, char32_t glyph, const std::optional<Style>& styleOverride);

    void writeUtf8Char(int x, int y, std::string_view utf8Glyph, const Style& style);
    void writeUtf8Char(int x, int y, std::string_view utf8Glyph, const std::optional<Style>& styleOverride);

    void writeChar(int x, int y, char glyph, const Style& style);
    void writeChar(int x, int y, char glyph, const std::optional<Style>& styleOverride);

    void writeString(int x, int y, const std::string& text, const Style& style);
    void writeString(int x, int y, const std::string& text, const std::optional<Style>& styleOverride);

    std::string renderToString() const;

private:
    int index(int x, int y) const;

    void clearCell(int x, int y);
    void clearOccupiedTrail(int x, int y);

    void writeSingleWidthCodePoint(int x, int y, char32_t glyph, const Style& style);
    void writeSingleWidthCodePoint(int x, int y, char32_t glyph, const std::optional<Style>& styleOverride);

    void writeDoubleWidthCodePoint(int x, int y, char32_t glyph, const Style& style);
    void writeDoubleWidthCodePoint(int x, int y, char32_t glyph, const std::optional<Style>& styleOverride);

    Style resolveWriteStyle(int x, int y, const std::optional<Style>& styleOverride) const;
    const ScreenCell& getStyleSourceCell(int x, int y) const;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<ScreenCell> m_cells;
};