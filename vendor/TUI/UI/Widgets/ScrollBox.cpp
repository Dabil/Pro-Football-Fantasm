#include "UI/Widgets/ScrollBox.h"

#include <algorithm>

#include "Input/Event.h"
#include "Input/MouseEvent.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/UIThemes.h"
#include "Rendering/Surface.h"
#include "UI/Scrolling/ScrollBehavior.h"

ScrollBox::ScrollBox()
    : ContainerWidget()
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
{
    setFocusable(true);

    m_verticalScrollbarStyle.trackStyle = m_styleSet.normal;
    m_verticalScrollbarStyle.thumbStyle = UIThemes::Selection;
}

ScrollBox::ScrollBox(const Rect& bounds)
    : ContainerWidget(bounds)
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
{
    setFocusable(true);

    m_verticalScrollbarStyle.trackStyle = m_styleSet.normal;
    m_verticalScrollbarStyle.thumbStyle = UIThemes::Selection;
}

Viewport& ScrollBox::viewport()
{
    return m_viewport;
}

const Viewport& ScrollBox::viewport() const
{
    return m_viewport;
}

void ScrollBox::setContentSize(Size size)
{
    m_minimumContentSize.width = std::max(0, size.width);
    m_minimumContentSize.height = std::max(0, size.height);
    updateViewport();
}

void ScrollBox::setContentSize(int width, int height)
{
    setContentSize(Size{ width, height });
}

bool ScrollBox::autoSizesToChildren() const
{
    return m_autoSizeToChildren;
}

void ScrollBox::setAutoSizesToChildren(bool enabled)
{
    m_autoSizeToChildren = enabled;
    updateViewport();
}

bool ScrollBox::isVerticalScrollbarVisible() const
{
    return m_verticalScrollbarVisible;
}

void ScrollBox::setVerticalScrollbarVisible(bool visible)
{
    m_verticalScrollbarVisible = visible;
    updateViewport();
}

bool ScrollBox::reservesVerticalScrollbarColumn() const
{
    return m_reserveVerticalScrollbarColumn;
}

void ScrollBox::setReserveVerticalScrollbarColumn(bool reserveColumn)
{
    m_reserveVerticalScrollbarColumn = reserveColumn;
    updateViewport();
}

const UI::Scrolling::VerticalScrollbarStyle& ScrollBox::verticalScrollbarStyle() const
{
    return m_verticalScrollbarStyle;
}

void ScrollBox::setVerticalScrollbarStyle(const UI::Scrolling::VerticalScrollbarStyle& style)
{
    m_verticalScrollbarStyle = style;
}

const WidgetStyles::StyleSet& ScrollBox::styleSet() const
{
    return m_styleSet;
}

void ScrollBox::setStyleSet(const WidgetStyles::StyleSet& styleSet)
{
    m_styleSet = styleSet;
}

void ScrollBox::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    updateViewport();

    const Rect viewBounds = viewportBounds();
    if (viewBounds.size.width <= 0 || viewBounds.size.height <= 0)
    {
        drawScrollbarIfNeeded(surface);
        return;
    }

    const Style& fillStyle = WidgetStyles::resolve(
        m_styleSet,
        WidgetStyles::stateFor(isEnabled(), isFocused()));

    surface.buffer().fillRect(viewBounds, U' ', fillStyle);

    const Size contentSize = m_viewport.contentSize();
    if (contentSize.width > 0 && contentSize.height > 0)
    {
        Surface contentSurface(contentSize.width, contentSize.height);
        contentSurface.clear(fillStyle);

        ContainerWidget::draw(contentSurface);

        const Point scroll = m_viewport.scrollOffset();

        for (int y = 0; y < viewBounds.size.height; ++y)
        {
            const int sourceY = scroll.y + y;
            if (sourceY < 0 || sourceY >= contentSize.height)
            {
                continue;
            }

            for (int x = 0; x < viewBounds.size.width; ++x)
            {
                const int sourceX = scroll.x + x;
                if (sourceX < 0 || sourceX >= contentSize.width)
                {
                    continue;
                }

                const int destinationX = viewBounds.position.x + x;
                const int destinationY = viewBounds.position.y + y;

                surface.buffer().setCell(
                    destinationX,
                    destinationY,
                    contentSurface.buffer().getCell(sourceX, sourceY));
            }
        }
    }

    drawScrollbarIfNeeded(surface);
}

bool ScrollBox::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        return false;
    }

    updateViewport();

    if (event.asMouse())
    {
        return handleMouseEvent(event);
    }

    if (ContainerWidget::handleEvent(event))
    {
        return true;
    }

    return UI::Scrolling::handleKeyboardScrollEvent(
        event,
        m_viewport,
        UI::Scrolling::ScrollInputOptions{
            true,
            true,
            1,
            std::max(1, viewportBounds().size.height)
        });
}

void ScrollBox::updateViewport()
{
    const Size contentSize = calculatedContentSize();
    const Rect viewBounds = viewportBounds();

    m_viewport.setContentSize(contentSize);
    m_viewport.setViewSize(
        std::max(0, viewBounds.size.width),
        std::max(0, viewBounds.size.height));
}

Size ScrollBox::calculatedContentSize() const
{
    Size contentSize = m_minimumContentSize;

    if (!m_autoSizeToChildren)
    {
        return contentSize;
    }

    for (std::size_t index = 0; index < childCount(); ++index)
    {
        const Widget* child = childAt(index);
        if (!child)
        {
            continue;
        }

        const Rect& childBounds = child->bounds();

        contentSize.width = std::max(
            contentSize.width,
            childBounds.position.x + childBounds.size.width);

        contentSize.height = std::max(
            contentSize.height,
            childBounds.position.y + childBounds.size.height);
    }

    return Size{
        std::max(0, contentSize.width),
        std::max(0, contentSize.height)
    };
}

Rect ScrollBox::viewportBounds() const
{
    return UI::Scrolling::viewportBoundsForContentBounds(
        bounds(),
        shouldDrawVerticalScrollbar(),
        m_reserveVerticalScrollbarColumn);
}

Rect ScrollBox::scrollbarBounds() const
{
    return UI::Scrolling::verticalScrollbarBoundsForContentBounds(
        bounds(),
        shouldDrawVerticalScrollbar() && m_reserveVerticalScrollbarColumn);
}

bool ScrollBox::shouldDrawVerticalScrollbar() const
{
    return m_verticalScrollbarVisible
        && UI::Scrolling::shouldShowVerticalScrollbar(m_viewport);
}

void ScrollBox::drawScrollbarIfNeeded(Surface& surface)
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

bool ScrollBox::handleMouseEvent(const Input::Event& event)
{
    const Input::MouseEvent* mouseEvent = event.asMouse();
    if (!mouseEvent)
    {
        return false;
    }

    if (mouseEvent->isWheel())
    {
        return UI::Scrolling::handleMouseWheelScrollEvent(
            event,
            m_viewport,
            bounds(),
            isFocused(),
            UI::Scrolling::ScrollInputOptions{
                false,
                true,
                1,
                1
            });
    }

    return dispatchTranslatedMouseEvent(*mouseEvent);
}

bool ScrollBox::dispatchTranslatedMouseEvent(const Input::MouseEvent& mouseEvent)
{
    const Rect viewBounds = viewportBounds();
    if (!viewBounds.contains(mouseEvent.position.x, mouseEvent.position.y)
        && !m_mouseCaptureChild)
    {
        return false;
    }

    Input::MouseEvent translatedMouseEvent = mouseEvent;
    translatedMouseEvent.position = screenToContent(mouseEvent.position);

    Input::Event translatedEvent(translatedMouseEvent);

    Widget* target = hitTestContent(translatedMouseEvent.position);

    if (mouseEvent.isPress())
    {
        m_mouseCaptureChild = target;

        if (target && canDispatchToChild(*target))
        {
            setFocusedChild(*target);
            return target->handleEvent(translatedEvent);
        }

        focus();
        return false;
    }

    if (mouseEvent.isRelease())
    {
        Widget* captured = m_mouseCaptureChild;
        m_mouseCaptureChild = nullptr;

        if (captured && containsChild(*captured) && canDispatchToChild(*captured))
        {
            return captured->handleEvent(translatedEvent);
        }

        return target && canDispatchToChild(*target)
            ? target->handleEvent(translatedEvent)
            : false;
    }

    if (mouseEvent.isClick() && target && canDispatchToChild(*target))
    {
        setFocusedChild(*target);
    }

    return target && canDispatchToChild(*target)
        ? target->handleEvent(translatedEvent)
        : false;
}

Point ScrollBox::screenToContent(Point screenPoint) const
{
    const Rect viewBounds = viewportBounds();

    return Point{
        screenPoint.x - viewBounds.position.x + m_viewport.scrollX(),
        screenPoint.y - viewBounds.position.y + m_viewport.scrollY()
    };
}

Widget* ScrollBox::hitTestContent(Point contentPoint)
{
    for (std::size_t offset = 0; offset < childCount(); ++offset)
    {
        const std::size_t index = childCount() - 1 - offset;
        Widget* child = childAt(index);

        if (!child || !canDispatchToChild(*child))
        {
            continue;
        }

        if (child->bounds().contains(contentPoint.x, contentPoint.y))
        {
            return child;
        }
    }

    return nullptr;
}

bool ScrollBox::canDispatchToChild(const Widget& child) const
{
    return child.isVisible() && child.isEnabled();
}