#pragma once

#include <string>

#include "Rendering/Styles/Style.h"
#include "UI/Content/TileGrid.h"
#include "UI/Panels/ScrollablePanel.h"

class Surface;

class TileGridView : public ScrollablePanel
{
public:
    TileGridView();
    explicit TileGridView(std::string title);

    void setGrid(const TileGrid& grid);
    void setGrid(TileGrid&& grid);

    const TileGrid& grid() const;
    TileGrid& grid();

    void setEmptyCellStyle(const Style& style);
    const Style& emptyCellStyle() const;

protected:
    void drawScrollableContent(
        Surface& surface,
        const Rect& visibleContentRect) override;

private:
    void syncContentSize();

private:
    TileGrid m_grid;
    Style m_emptyCellStyle;
};