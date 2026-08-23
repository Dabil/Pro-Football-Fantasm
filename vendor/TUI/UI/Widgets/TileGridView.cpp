#include "UI/Widgets/TileGridView.h"

#include <utility>

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"

TileGridView::TileGridView()
    : ScrollablePanel()
{
    setFocusable(true);
    syncContentSize();
}

TileGridView::TileGridView(std::string title)
    : ScrollablePanel(Rect{}, std::move(title))
{
    setFocusable(true);
    syncContentSize();
}

void TileGridView::setGrid(const TileGrid& grid)
{
    m_grid = grid;
    syncContentSize();
}

void TileGridView::setGrid(TileGrid&& grid)
{
    m_grid = std::move(grid);
    syncContentSize();
}

const TileGrid& TileGridView::grid() const
{
    return m_grid;
}

TileGrid& TileGridView::grid()
{
    return m_grid;
}

void TileGridView::setEmptyCellStyle(const Style& style)
{
    m_emptyCellStyle = style;
}

const Style& TileGridView::emptyCellStyle() const
{
    return m_emptyCellStyle;
}

void TileGridView::drawScrollableContent(
    Surface& surface,
    const Rect& visibleContentRect)
{
    ScreenBuffer& buffer = surface.buffer();

    for (int viewY = 0; viewY < visibleContentRect.size.height; ++viewY)
    {
        const int contentY = visibleContentRect.position.y + viewY;

        for (int viewX = 0; viewX < visibleContentRect.size.width; ++viewX)
        {
            const int contentX = visibleContentRect.position.x + viewX;

            const TileGrid::Cell* cell = m_grid.tryGet(contentX, contentY);
            if (cell != nullptr)
            {
                buffer.setCell(viewX, viewY, *cell);
            }
            else
            {
                buffer.writeCodePoint(viewX, viewY, U' ', m_emptyCellStyle);
            }
        }
    }
}

void TileGridView::syncContentSize()
{
    setContentSize(m_grid.size());
}