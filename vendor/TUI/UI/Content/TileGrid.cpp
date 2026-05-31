#include "UI/Content/TileGrid.h"

#include <algorithm>
#include <stdexcept>

TileGrid::TileGrid()
    : m_size{ 0, 0 }
{
}

TileGrid::TileGrid(int width, int height)
{
    resize(width, height);
}

TileGrid::TileGrid(const Size& size)
{
    resize(size);
}

void TileGrid::resize(int width, int height)
{
    width = std::max(0, width);
    height = std::max(0, height);

    m_size.width = width;
    m_size.height = height;

    m_cells.resize(static_cast<std::size_t>(width * height));
}

void TileGrid::resize(const Size& size)
{
    resize(size.width, size.height);
}

void TileGrid::clear()
{
    m_size.width = 0;
    m_size.height = 0;

    m_cells.clear();
}

void TileGrid::fill(const Cell& value)
{
    std::fill(m_cells.begin(), m_cells.end(), value);
}

bool TileGrid::empty() const
{
    return m_cells.empty();
}

int TileGrid::width() const
{
    return m_size.width;
}

int TileGrid::height() const
{
    return m_size.height;
}

Size TileGrid::size() const
{
    return m_size;
}

bool TileGrid::contains(int x, int y) const
{
    return x >= 0
        && y >= 0
        && x < m_size.width
        && y < m_size.height;
}

std::size_t TileGrid::index(int x, int y) const
{
    return static_cast<std::size_t>((y * m_size.width) + x);
}

TileGrid::Cell* TileGrid::tryGet(int x, int y)
{
    if (!contains(x, y))
    {
        return nullptr;
    }

    return &m_cells[index(x, y)];
}

const TileGrid::Cell* TileGrid::tryGet(int x, int y) const
{
    if (!contains(x, y))
    {
        return nullptr;
    }

    return &m_cells[index(x, y)];
}

TileGrid::Cell& TileGrid::at(int x, int y)
{
    if (!contains(x, y))
    {
        throw std::out_of_range("TileGrid::at() coordinates out of range");
    }

    return m_cells[index(x, y)];
}

const TileGrid::Cell& TileGrid::at(int x, int y) const
{
    if (!contains(x, y))
    {
        throw std::out_of_range("TileGrid::at() coordinates out of range");
    }

    return m_cells[index(x, y)];
}

void TileGrid::set(int x, int y, const Cell& value)
{
    if (!contains(x, y))
    {
        return;
    }

    m_cells[index(x, y)] = value;
}

const std::vector<TileGrid::Cell>& TileGrid::cells() const
{
    return m_cells;
}

std::vector<TileGrid::Cell>& TileGrid::cells()
{
    return m_cells;
}

TileGrid TileGrid::fromTextRows(
    const std::vector<std::string>& rows,
    const Cell& defaultCell)
{
    TileGrid grid;

    if (rows.empty())
    {
        return grid;
    }

    int maxWidth = 0;

    for (const std::string& row : rows)
    {
        maxWidth = std::max(maxWidth, static_cast<int>(row.size()));
    }

    grid.resize(maxWidth, static_cast<int>(rows.size()));
    grid.fill(defaultCell);

    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
    {
        const std::string& row = rows[static_cast<std::size_t>(y)];

        for (int x = 0; x < static_cast<int>(row.size()); ++x)
        {
            Cell cell = defaultCell;

            cell.glyph = static_cast<char32_t>(row[static_cast<std::size_t>(x)]);

            grid.set(x, y, cell);
        }
    }

    return grid;
}

TileGrid TileGrid::fromTokenRows(
    const std::vector<std::string>& rows,
    const std::unordered_map<char, Cell>& palette,
    const Cell& fallbackCell)
{
    TileGrid grid;

    if (rows.empty())
    {
        return grid;
    }

    int maxWidth = 0;

    for (const std::string& row : rows)
    {
        maxWidth = std::max(maxWidth, static_cast<int>(row.size()));
    }

    grid.resize(maxWidth, static_cast<int>(rows.size()));
    grid.fill(fallbackCell);

    for (int y = 0; y < static_cast<int>(rows.size()); ++y)
    {
        const std::string& row = rows[static_cast<std::size_t>(y)];

        for (int x = 0; x < static_cast<int>(row.size()); ++x)
        {
            const char token = row[static_cast<std::size_t>(x)];

            const auto it = palette.find(token);

            if (it != palette.end())
            {
                grid.set(x, y, it->second);
            }
            else
            {
                grid.set(x, y, fallbackCell);
            }
        }
    }

    return grid;
}