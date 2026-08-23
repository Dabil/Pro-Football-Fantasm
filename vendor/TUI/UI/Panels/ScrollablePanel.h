#pragma once

#include "Core/Rect.h"
#include "Core/Size.h"
#include "Rendering/Surface.h"
#include "UI/Base/Viewport.h"
#include "UI/Panels/Panel.h"
#include "UI/Scrolling/Scrollbar.h"

namespace Input
{
    class Event;
}

class ScrollablePanel : public Panel
{
public:
    ScrollablePanel();
    explicit ScrollablePanel(const Rect& bounds);
    ScrollablePanel(const Rect& bounds, std::string title);

    Viewport& viewport();
    const Viewport& viewport() const;

    void setContentSize(Size size);
    void setContentSize(int width, int height);

    void setViewSize(Size size);
    void setViewSize(int width, int height);

    bool isVerticalScrollbarVisible() const;
    void setVerticalScrollbarVisible(bool visible);

    bool reservesVerticalScrollbarColumn() const;
    void setReserveVerticalScrollbarColumn(bool reserveColumn);

    const UI::Scrolling::VerticalScrollbarStyle& verticalScrollbarStyle() const;
    void setVerticalScrollbarStyle(const UI::Scrolling::VerticalScrollbarStyle& style);

    Rect viewportBounds() const;
    Rect scrollbarBounds() const;
    Rect visibleContentRect() const;

    bool scrollUp(int lines = 1);
    bool scrollDown(int lines = 1);
    bool scrollLeft(int columns = 1);
    bool scrollRight(int columns = 1);

    bool pageUp();
    bool pageDown();

    bool home();
    bool end();

    bool handleEvent(const Input::Event& event) override;
    void draw(Surface& surface) override;

protected:
    virtual void drawScrollableContent(
        Surface& surface,
        const Rect& visibleContentRect);

    Point contentToViewport(Point contentPoint) const;
    Point viewportToContent(Point viewportPoint) const;

private:
    void syncViewportToPanel();
    bool scrollToAndReportChange(int x, int y);
    bool shouldDrawVerticalScrollbar() const;

    void drawScrollbarIfNeeded(Surface& surface) const;

    void blitViewportSurface(
        Surface& target,
        const Surface& source,
        const Rect& targetRect) const;

private:
    Viewport m_viewport;

    bool m_verticalScrollbarVisible = false;
    bool m_reserveVerticalScrollbarColumn = true;

    UI::Scrolling::VerticalScrollbarStyle m_verticalScrollbarStyle;
};