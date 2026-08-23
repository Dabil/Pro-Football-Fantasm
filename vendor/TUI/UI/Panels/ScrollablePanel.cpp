#include "UI/Panels/ScrollablePanel.h"

#include <algorithm>
#include <utility>

#include "Input/Event.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/UIThemes.h"
#include "UI/Scrolling/ScrollBehavior.h"

ScrollablePanel::ScrollablePanel()
    : Panel()
{
    m_verticalScrollbarStyle.trackStyle = borderStyle();
    m_verticalScrollbarStyle.thumbStyle = UIThemes::Selection;
    syncViewportToPanel();
}

ScrollablePanel::ScrollablePanel(const Rect& bounds)
    : Panel(bounds)
{
    m_verticalScrollbarStyle.trackStyle = borderStyle();
    m_verticalScrollbarStyle.thumbStyle = UIThemes::Selection;
    syncViewportToPanel();
}

ScrollablePanel::ScrollablePanel(const Rect& bounds, std::string title)
    : Panel(bounds, std::move(title))
{
    m_verticalScrollbarStyle.trackStyle = borderStyle();
    m_verticalScrollbarStyle.thumbStyle = UIThemes::Selection;
    syncViewportToPanel();
}

Viewport& ScrollablePanel::viewport()
{
    return m_viewport;
}

const Viewport& ScrollablePanel::viewport() const
{
    return m_viewport;
}

void ScrollablePanel::setContentSize(Size size)
{
    m_viewport.setContentSize(size);
}

void ScrollablePanel::setContentSize(int width, int height)
{
    m_viewport.setContentSize(width, height);
}

void ScrollablePanel::setViewSize(Size size)
{
    m_viewport.setViewSize(size);
}

void ScrollablePanel::setViewSize(int width, int height)
{
    m_viewport.setViewSize(width, height);
}

bool ScrollablePanel::isVerticalScrollbarVisible() const
{
    return m_verticalScrollbarVisible;
}

void ScrollablePanel::setVerticalScrollbarVisible(bool visible)
{
    m_verticalScrollbarVisible = visible;
    syncViewportToPanel();
}

bool ScrollablePanel::reservesVerticalScrollbarColumn() const
{
    return m_reserveVerticalScrollbarColumn;
}

void ScrollablePanel::setReserveVerticalScrollbarColumn(bool reserveColumn)
{
    m_reserveVerticalScrollbarColumn = reserveColumn;
    syncViewportToPanel();
}

const UI::Scrolling::VerticalScrollbarStyle& ScrollablePanel::verticalScrollbarStyle() const
{
    return m_verticalScrollbarStyle;
}

void ScrollablePanel::setVerticalScrollbarStyle(
    const UI::Scrolling::VerticalScrollbarStyle& style)
{
    m_verticalScrollbarStyle = style;
}

Rect ScrollablePanel::viewportBounds() const
{
    return UI::Scrolling::viewportBoundsForContentBounds(
        contentBounds(),
        shouldDrawVerticalScrollbar(),
        m_reserveVerticalScrollbarColumn);
}

Rect ScrollablePanel::scrollbarBounds() const
{
    return UI::Scrolling::verticalScrollbarBoundsForContentBounds(
        contentBounds(),
        shouldDrawVerticalScrollbar() && m_reserveVerticalScrollbarColumn);
}

Rect ScrollablePanel::visibleContentRect() const
{
    return m_viewport.visibleContentRect();
}

bool ScrollablePanel::scrollUp(int lines)
{
    return scrollToAndReportChange(
        m_viewport.scrollX(),
        m_viewport.scrollY() - std::max(0, lines));
}

bool ScrollablePanel::scrollDown(int lines)
{
    return scrollToAndReportChange(
        m_viewport.scrollX(),
        m_viewport.scrollY() + std::max(0, lines));
}

bool ScrollablePanel::scrollLeft(int columns)
{
    return scrollToAndReportChange(
        m_viewport.scrollX() - std::max(0, columns),
        m_viewport.scrollY());
}

bool ScrollablePanel::scrollRight(int columns)
{
    return scrollToAndReportChange(
        m_viewport.scrollX() + std::max(0, columns),
        m_viewport.scrollY());
}

bool ScrollablePanel::pageUp()
{
    const int pageAmount = std::max(1, m_viewport.viewSize().height);
    return scrollUp(pageAmount);
}

bool ScrollablePanel::pageDown()
{
    const int pageAmount = std::max(1, m_viewport.viewSize().height);
    return scrollDown(pageAmount);
}

bool ScrollablePanel::home()
{
    return scrollToAndReportChange(0, 0);
}

bool ScrollablePanel::end()
{
    return scrollToAndReportChange(
        m_viewport.scrollX(),
        m_viewport.maxScrollY());
}

bool ScrollablePanel::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        return false;
    }

    syncViewportToPanel();

    return UI::Scrolling::handleScrollEvent(
        event,
        m_viewport,
        bounds(),
        isFocused());
}

void ScrollablePanel::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    Panel::draw(surface);

    syncViewportToPanel();

    const Rect targetRect = viewportBounds();

    if (targetRect.size.width <= 0 || targetRect.size.height <= 0)
    {
        drawScrollbarIfNeeded(surface);
        return;
    }

    Surface viewportSurface(targetRect.size.width, targetRect.size.height);
    viewportSurface.clear(backgroundStyle());

    drawScrollableContent(viewportSurface, m_viewport.visibleContentRect());
    blitViewportSurface(surface, viewportSurface, targetRect);

    drawScrollbarIfNeeded(surface);
}

void ScrollablePanel::drawScrollableContent(
    Surface& surface,
    const Rect& visibleContentRect)
{
    (void)surface;
    (void)visibleContentRect;
}

Point ScrollablePanel::contentToViewport(Point contentPoint) const
{
    return m_viewport.contentToView(contentPoint);
}

Point ScrollablePanel::viewportToContent(Point viewportPoint) const
{
    return m_viewport.viewToContent(viewportPoint);
}

void ScrollablePanel::syncViewportToPanel()
{
    const Rect viewRect = viewportBounds();
    m_viewport.setViewSize(viewRect.size);
}

bool ScrollablePanel::scrollToAndReportChange(int x, int y)
{
    const Point before = m_viewport.scrollOffset();

    m_viewport.scrollTo(x, y);

    const Point after = m_viewport.scrollOffset();

    return before.x != after.x || before.y != after.y;
}

bool ScrollablePanel::shouldDrawVerticalScrollbar() const
{
    return m_verticalScrollbarVisible
        && UI::Scrolling::shouldShowVerticalScrollbar(m_viewport);
}

void ScrollablePanel::drawScrollbarIfNeeded(Surface& surface) const
{
    if (!shouldDrawVerticalScrollbar())
    {
        return;
    }

    UI::Scrolling::drawVerticalScrollbar(
        surface,
        scrollbarBounds(),
        m_viewport,
        m_verticalScrollbarStyle);
}

void ScrollablePanel::blitViewportSurface(
    Surface& target,
    const Surface& source,
    const Rect& targetRect) const
{
    ScreenBuffer& targetBuffer = target.buffer();
    const ScreenBuffer& sourceBuffer = source.buffer();

    const int copyWidth = std::min(targetRect.size.width, sourceBuffer.getWidth());
    const int copyHeight = std::min(targetRect.size.height, sourceBuffer.getHeight());

    for (int y = 0; y < copyHeight; ++y)
    {
        for (int x = 0; x < copyWidth; ++x)
        {
            const int targetX = targetRect.position.x + x;
            const int targetY = targetRect.position.y + y;

            if (!targetBuffer.inBounds(targetX, targetY))
            {
                continue;
            }

            targetBuffer.setCell(
                targetX,
                targetY,
                sourceBuffer.getCell(x, y));
        }
    }
}