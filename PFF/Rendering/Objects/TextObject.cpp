#include "Rendering/Objects/TextObject.h"

#include <fstream>
#include <iterator>
#include <stdexcept>

#include "Rendering/Objects/TextObjectExporter.h"
#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"

namespace
{
    int cellWidthToInt(CellWidth width)
    {
        switch (width)
        {
        case CellWidth::Zero:
            return 0;

        case CellWidth::Two:
            return 2;

        case CellWidth::One:
        default:
            return 1;
        }
    }

    Style resolveDrawStyle(
        const ScreenBuffer& target,
        int x,
        int y,
        const TextObjectCell& sourceCell,
        const std::optional<Style>& overrideStyle)
    {
        if (overrideStyle.has_value())
        {
            return *overrideStyle;
        }

        if (sourceCell.style.has_value())
        {
            return *sourceCell.style;
        }

        if (target.inBounds(x, y))
        {
            return target.getCell(x, y).style;
        }

        return Style{};
    }
}

TextObject::TextObject() = default;

TextObject TextObject::fromUtf8(std::string_view utf8Text)
{
    return fromU32(UnicodeConversion::utf8ToU32(utf8Text));
}

TextObject TextObject::fromUtf8(std::string_view utf8Text, const Style& style)
{
    return fromU32(UnicodeConversion::utf8ToU32(utf8Text), style);
}

TextObject TextObject::fromU32(std::u32string_view text)
{
    return buildFromLines(splitLines(text), std::nullopt);
}

TextObject TextObject::fromU32(std::u32string_view text, const Style& style)
{
    return buildFromLines(splitLines(text), std::optional<Style>(style));
}

TextObject TextObject::loadFromFile(const std::string& filePath)
{
    TextObject object;
    object.loadUtf8File(filePath);
    return object;
}

TextObject TextObject::loadFromFile(const std::string& filePath, const Style& style)
{
    TextObject object;
    object.loadUtf8File(filePath, style);
    return object;
}

bool TextObject::loadUtf8File(const std::string& filePath)
{
    clear();

    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        return false;
    }

    const std::string bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    *this = fromUtf8(std::string_view(bytes.data(), bytes.size()));
    m_loaded = true;
    return true;
}

bool TextObject::loadUtf8File(const std::string& filePath, const Style& style)
{
    clear();

    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        return false;
    }

    const std::string bytes{
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };

    *this = fromUtf8(std::string_view(bytes.data(), bytes.size()), style);
    m_loaded = true;
    return true;
}

TextObjectExporter::SaveResult TextObject::saveToFile(const std::string& filePath) const
{
    return TextObjectExporter::saveToFile(*this, filePath);
}

TextObjectExporter::SaveResult TextObject::saveToFile(
    const std::string& filePath,
    const TextObjectExporter::SaveOptions& options) const
{
    return TextObjectExporter::saveToFile(*this, filePath, options);
}

bool TextObject::trySaveToFile(const std::string& filePath) const
{
    return TextObjectExporter::trySaveToFile(*this, filePath);
}

bool TextObject::trySaveToFile(
    const std::string& filePath,
    const TextObjectExporter::SaveOptions& options) const
{
    return TextObjectExporter::trySaveToFile(*this, filePath, options);
}

void TextObject::clear()
{
    m_width = 0;
    m_height = 0;
    m_loaded = false;
    m_hasAnyCellStyles = false;
    m_cells.clear();
}

bool TextObject::isLoaded() const
{
    return m_loaded;
}

int TextObject::getWidth() const
{
    return m_width;
}

int TextObject::getHeight() const
{
    return m_height;
}

bool TextObject::isEmpty() const
{
    return m_cells.empty();
}

bool TextObject::hasAnyCellStyles() const
{
    return m_hasAnyCellStyles;
}

bool TextObject::preservesPerCellStyle() const
{
    return m_hasAnyCellStyles;
}

const TextObjectCell& TextObject::getCell(int x, int y) const
{
    const TextObjectCell* cell = tryGetCell(x, y);
    if (cell == nullptr)
    {
        throw std::out_of_range("TextObject::getCell out of bounds.");
    }

    return *cell;
}

const TextObjectCell* TextObject::tryGetCell(int x, int y) const
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
    {
        return nullptr;
    }

    return &m_cells[static_cast<std::size_t>(index(x, y))];
}

void TextObject::draw(ScreenBuffer& target, int x, int y) const
{
    draw(target, x, y, std::nullopt);
}

void TextObject::draw(ScreenBuffer& target, int x, int y, const Style& overrideStyle) const
{
    draw(target, x, y, std::optional<Style>(overrideStyle));
}

void TextObject::draw(ScreenBuffer& target, int x, int y, const std::optional<Style>& overrideStyle) const
{
    if (!m_loaded || m_width <= 0 || m_height <= 0)
    {
        return;
    }

    for (int row = 0; row < m_height; ++row)
    {
        for (int col = 0; col < m_width; ++col)
        {
            const int targetX = x + col;
            const int targetY = y + row;

            if (!target.inBounds(targetX, targetY))
            {
                continue;
            }

            const TextObjectCell& sourceCell = m_cells[static_cast<std::size_t>(index(col, row))];

            if (sourceCell.kind == CellKind::Empty)
            {
                continue;
            }

            if (sourceCell.kind == CellKind::WideTrailing)
            {
                continue;
            }

            if (sourceCell.kind == CellKind::CombiningContinuation)
            {
                continue;
            }

            const Style style = resolveDrawStyle(target, targetX, targetY, sourceCell, overrideStyle);
            target.writeCodePoint(targetX, targetY, sourceCell.glyph, style);
        }
    }
}

TextObject TextObject::buildFromLines(
    const std::vector<std::u32string>& lines,
    const std::optional<Style>& defaultStyle)
{
    TextObject object;

    object.m_height = static_cast<int>(lines.size());
    object.m_width = 0;

    for (const std::u32string& line : lines)
    {
        object.m_width = std::max(object.m_width, measureLineWidth(line));
    }

    object.m_cells.reserve(static_cast<std::size_t>(object.m_width * object.m_height));

    for (const std::u32string& line : lines)
    {
        appendLineCells(object.m_cells, line, object.m_width, defaultStyle);
    }

    object.m_hasAnyCellStyles = defaultStyle.has_value();
    object.m_loaded = true;
    return object;
}

std::vector<std::u32string> TextObject::splitLines(std::u32string_view text)
{
    std::vector<std::u32string> lines;
    std::u32string current;

    for (char32_t ch : text)
    {
        if (ch == U'\r')
        {
            continue;
        }

        if (ch == U'\n')
        {
            lines.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    lines.push_back(current);
    return lines;
}

int TextObject::measureLineWidth(std::u32string_view line)
{
    int width = 0;

    for (char32_t glyph : line)
    {
        glyph = UnicodeConversion::sanitizeCodePoint(glyph);

        if (glyph == U'\r' || glyph == U'\n')
        {
            continue;
        }

        const CellWidth measuredWidth = UnicodeWidth::measureCodePointWidth(glyph);
        width += cellWidthToInt(measuredWidth);
    }

    return width;
}

void TextObject::appendLineCells(
    std::vector<TextObjectCell>& outCells,
    std::u32string_view line,
    int paddedWidth,
    const std::optional<Style>& defaultStyle)
{
    int writtenWidth = 0;

    for (char32_t glyph : line)
    {
        glyph = UnicodeConversion::sanitizeCodePoint(glyph);

        if (glyph == U'\r' || glyph == U'\n')
        {
            continue;
        }

        const CellWidth measuredWidth = UnicodeWidth::measureCodePointWidth(glyph);

        if (measuredWidth == CellWidth::Zero)
        {
            continue;
        }

        if (measuredWidth == CellWidth::Two)
        {
            TextObjectCell leading;
            leading.glyph = glyph;
            leading.kind = CellKind::Glyph;
            leading.width = CellWidth::Two;
            leading.style = defaultStyle;
            outCells.push_back(leading);

            TextObjectCell trailing;
            trailing.glyph = U' ';
            trailing.kind = CellKind::WideTrailing;
            trailing.width = CellWidth::Zero;
            trailing.style = defaultStyle;
            outCells.push_back(trailing);

            writtenWidth += 2;
            continue;
        }

        TextObjectCell single;
        single.glyph = glyph;
        single.kind = CellKind::Glyph;
        single.width = CellWidth::One;
        single.style = defaultStyle;
        outCells.push_back(single);

        writtenWidth += 1;
    }

    while (writtenWidth < paddedWidth)
    {
        TextObjectCell empty;
        empty.glyph = U' ';
        empty.kind = CellKind::Empty;
        empty.width = CellWidth::One;
        empty.style.reset();
        outCells.push_back(empty);

        ++writtenWidth;
    }
}

int TextObject::index(int x, int y) const
{
    return y * m_width + x;
}