#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/Size.h"
#include "Rendering/ScreenCell.h"

class TileGrid
{
public:
    using Cell = ScreenCell;

    TileGrid();
    TileGrid(int width, int height);
    TileGrid(const Size& size);

    void resize(int width, int height);
    void resize(const Size& size);

    void clear();
    void fill(const Cell& value);

    bool empty() const;

    int width() const;
    int height() const;

    Size size() const;

    bool contains(int x, int y) const;

    std::size_t index(int x, int y) const;

    Cell* tryGet(int x, int y);
    const Cell* tryGet(int x, int y) const;

    Cell& at(int x, int y);
    const Cell& at(int x, int y) const;

    void set(int x, int y, const Cell& value);

    const std::vector<Cell>& cells() const;
    std::vector<Cell>& cells();

    static TileGrid fromTextRows(
        const std::vector<std::string>& rows,
        const Cell& defaultCell = Cell());

    static TileGrid fromTokenRows(
        const std::vector<std::string>& rows,
        const std::unordered_map<char, Cell>& palette,
        const Cell& fallbackCell = Cell());

private:
    Size m_size;
    std::vector<Cell> m_cells;
};