#include "Rendering/Objects/TextObjectBuilder.h"

#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"

/*
TODO: Next clean-up

Current: Added explicit authroed/transparent vocabulary while preserving old calls:

New:
setAuthoredSpace(...)      // writes CellKind::Glyph + U' '
setTransparent(...)        // writes CellKind::Empty
fillAuthoredSpace(...)     // fills entire surface with authored spaces
fillTransparent(...)       // fills entire surface with transparent Empty cells

Removed:
setEmpty(...) and replaced with setTransparent(...) at all call-sites

Existing Compatibility:

fill(...)                  // remains available as the low-level generic fill
reset(...)                 // still defaults to transparent cells

Next clean-up check to see if the compatibility layer should be enhanced
*/

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
    fillTransparent();
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
    cell.cluster.clear();
    if (kind == CellKind::Glyph)
    {
        cell.cluster.push_back(cell.glyph);
    }
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
    glyph = UnicodeConversion::sanitizeCodePoint(glyph);

    const CellWidth measuredWidth = UnicodeWidth::measureCodePointWidth(glyph);

    if (measuredWidth == CellWidth::Zero)
    {
        return false;
    }

    if (measuredWidth == CellWidth::Two)
    {
        return setWideGlyph(x, y, glyph, style);
    }

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

    glyph = UnicodeConversion::sanitizeCodePoint(glyph);

    if (!setCell(x, y, glyph, CellKind::Glyph, CellWidth::Two, style))
    {
        return false;
    }

    return setCell(x + 1, y, U' ', CellKind::WideTrailing, CellWidth::Zero, style);
}

bool TextObjectBuilder::setAuthoredSpace(
    int x,
    int y,
    const std::optional<Style>& style)
{
    return setCell(x, y, U' ', CellKind::Glyph, CellWidth::One, style);
}

bool TextObjectBuilder::setTransparent(
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

void TextObjectBuilder::fillAuthoredSpace(const std::optional<Style>& style)
{
    fill(U' ', CellKind::Glyph, CellWidth::One, style);
}

void TextObjectBuilder::fillTransparent(const std::optional<Style>& style)
{
    fill(U' ', CellKind::Empty, CellWidth::One, style);
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
