#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Text/TextTypes.h"

namespace TextObjectExporter
{
    struct SaveOptions;
    struct SaveResult;
}

class TextObjectBuilder;

struct TextObjectCell
{
    char32_t glyph = U' ';
    CellKind kind = CellKind::Empty;
    CellWidth width = CellWidth::One;
    std::optional<Style> style;

    bool isEmpty() const
    {
        return kind == CellKind::Empty;
    }

    bool isGlyph() const
    {
        return kind == CellKind::Glyph;
    }

    bool isWideTrailing() const
    {
        return kind == CellKind::WideTrailing;
    }

    bool isCombiningContinuation() const
    {
        return kind == CellKind::CombiningContinuation;
    }

    bool hasStyle() const
    {
        return style.has_value();
    }
};

class TextObject
{
    friend class TextObjectBuilder;

public:
    TextObject();

    static TextObject fromUtf8(std::string_view utf8Text);
    static TextObject fromUtf8(std::string_view utf8Text, const Style& style);

    static TextObject fromU32(std::u32string_view text);
    static TextObject fromU32(std::u32string_view text, const Style& style);

    static TextObject loadFromFile(const std::string& filePath);
    static TextObject loadFromFile(const std::string& filePath, const Style& style);

    bool loadUtf8File(const std::string& filePath);
    bool loadUtf8File(const std::string& filePath, const Style& style);

    TextObjectExporter::SaveResult saveToFile(const std::string& filePath) const;
    TextObjectExporter::SaveResult saveToFile(
        const std::string& filePath,
        const TextObjectExporter::SaveOptions& options) const;

    bool trySaveToFile(const std::string& filePath) const;
    bool trySaveToFile(
        const std::string& filePath,
        const TextObjectExporter::SaveOptions& options) const;

    void clear();

    bool isLoaded() const;
    int getWidth() const;
    int getHeight() const;
    bool isEmpty() const;

    bool hasAnyCellStyles() const;
    bool preservesPerCellStyle() const;

    const TextObjectCell& getCell(int x, int y) const;
    const TextObjectCell* tryGetCell(int x, int y) const;

    void draw(ScreenBuffer& target, int x, int y) const;
    void draw(ScreenBuffer& target, int x, int y, const Style& overrideStyle) const;
    void draw(ScreenBuffer& target, int x, int y, const std::optional<Style>& overrideStyle) const;

private:
    static TextObject buildFromLines(
        const std::vector<std::u32string>& lines,
        const std::optional<Style>& defaultStyle);

    static std::vector<std::u32string> splitLines(std::u32string_view text);
    static int measureLineWidth(std::u32string_view line);

    static void appendLineCells(
        std::vector<TextObjectCell>& outCells,
        std::u32string_view line,
        int paddedWidth,
        const std::optional<Style>& defaultStyle);

    int index(int x, int y) const;

private:
    int m_width = 0;
    int m_height = 0;
    bool m_loaded = false;
    bool m_hasAnyCellStyles = false;
    std::vector<TextObjectCell> m_cells;
};