#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Rect.h"
#include "Rendering/ScreenCell.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Text/TextCluster.h"

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
    const ScreenCell* tryGetCell(int x, int y) const;

    const ScreenCell& getLogicalCell(int x, int y) const;
    char32_t getDisplayGlyph(int x, int y) const;
    Style getDisplayStyle(int x, int y) const;
    std::u32string getDisplayCluster(int x, int y) const;

    const ScreenCell* tryGetRowData(int y) const;
    void expandSpanToGlyphBoundaries(int y, int& xStart, int& xEnd) const;

    /*
        Immediate mode drawing methods, use these when:
        - Text that will frequently change
    */

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

    void writeCluster(int x, int y, const TextCluster& cluster, const std::optional<Style>& styleOverride);
    void writeSingleWidthCluster(int x, int y, const std::u32string& clusterText, const std::optional<Style>& styleOverride);
    void writeDoubleWidthCluster(int x, int y, const std::u32string& clusterText, const std::optional<Style>& styleOverride);
    void appendZeroWidthClusterToPreviousVisibleCell(int x, int y, const std::u32string& clusterText);

    Style resolveWriteStyle(int x, int y, const std::optional<Style>& styleOverride) const;
    const ScreenCell& getLogicalCellInternal(int x, int y) const;

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<ScreenCell> m_cells;
};