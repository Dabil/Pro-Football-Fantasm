#include "Rendering/Objects/TextObjectBuilder.h"

#include "Utilities/Unicode/UnicodeConversion.h"

TextObjectBuilder::TextObjectBuilder(int width, int height)
{
    reset(width, height);
}

void TextObjectBuilder::reset(int width, int height)
{
    m_object.clear();

    m_object.m_width = (width > 0) ? width : 0;
    m_object.m_height = (height > 0) ? height : 0;
    m_object.m_loaded = (m_object.m_width > 0 && m_object.m_height > 0);
    m_object.m_hasAnyCellStyles = false;

    if (!m_object.m_loaded)
    {
        return;
    }

    m_object.m_cells.resize(static_cast<std::size_t>(m_object.m_width * m_object.m_height));
    fill(U' ', CellKind::Empty, CellWidth::One, std::nullopt);
}

void TextObjectBuilder::clear()
{
    m_object.clear();
}

bool TextObjectBuilder::isInitialized() const
{
    return m_object.isLoaded() &&
        m_object.getWidth() > 0 &&
        m_object.getHeight() > 0;
}

int TextObjectBuilder::getWidth() const
{
    return m_object.getWidth();
}

int TextObjectBuilder::getHeight() const
{
    return m_object.getHeight();
}

bool TextObjectBuilder::inBounds(int x, int y) const
{
    return x >= 0 &&
        x < m_object.m_width &&
        y >= 0 &&
        y < m_object.m_height;
}

bool TextObjectBuilder::setCell(
    int x,
    int y,
    char32_t glyph,
    CellKind kind,
    CellWidth width,
    const std::optional<Style>& style)
{
    if (!inBounds(x, y))
    {
        return false;
    }

    TextObjectCell& cell = m_object.m_cells[static_cast<std::size_t>(index(x, y))];
    cell.glyph = UnicodeConversion::sanitizeCodePoint(glyph);
    cell.kind = kind;
    cell.width = width;
    cell.style = style;

    if (style.has_value())
    {
        m_object.m_hasAnyCellStyles = true;
    }

    return true;
}

bool TextObjectBuilder::setGlyph(
    int x,
    int y,
    char32_t glyph,
    const std::optional<Style>& style)
{
    return setCell(x, y, glyph, CellKind::Glyph, CellWidth::One, style);
}

bool TextObjectBuilder::setWideGlyph(
    int x,
    int y,
    char32_t glyph,
    const std::optional<Style>& style)
{
    if (!inBounds(x, y) || !inBounds(x + 1, y))
    {
        return false;
    }

    if (!setCell(x, y, glyph, CellKind::Glyph, CellWidth::Two, style))
    {
        return false;
    }

    return setCell(x + 1, y, U' ', CellKind::WideTrailing, CellWidth::Zero, style);
}

bool TextObjectBuilder::setEmpty(
    int x,
    int y,
    const std::optional<Style>& style)
{
    return setCell(x, y, U' ', CellKind::Empty, CellWidth::One, style);
}

void TextObjectBuilder::fill(
    char32_t glyph,
    CellKind kind,
    CellWidth width,
    const std::optional<Style>& style)
{
    if (!isInitialized())
    {
        return;
    }

    for (int y = 0; y < m_object.m_height; ++y)
    {
        for (int x = 0; x < m_object.m_width; ++x)
        {
            setCell(x, y, glyph, kind, width, style);
        }
    }
}

TextObject TextObjectBuilder::build() const
{
    return m_object;
}

TextObject TextObjectBuilder::buildAndReset()
{
    TextObject result = m_object;
    clear();
    return result;
}

int TextObjectBuilder::index(int x, int y) const
{
    return y * m_object.m_width + x;
}