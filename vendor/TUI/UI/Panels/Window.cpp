#include "UI/Panels/Window.h"

#include <algorithm>
#include <utility>

#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/UIThemes.h"

Window::Window()
    : Panel()
    , m_hoveredBorderStyle(UIThemes::Border + style::Bold)
    , m_hoveredTitleStyle(UIThemes::PanelTitle + style::Bold)
{
    setFocusable(true);
}

Window::Window(const Rect& bounds)
    : Panel(bounds)
    , m_hoveredBorderStyle(UIThemes::Border + style::Bold)
    , m_hoveredTitleStyle(UIThemes::PanelTitle + style::Bold)
{
    setFocusable(true);
}

Window::Window(const Rect& bounds, std::string title)
    : Panel(bounds, std::move(title))
    , m_hoveredBorderStyle(UIThemes::Border + style::Bold)
    , m_hoveredTitleStyle(UIThemes::PanelTitle + style::Bold)
{
    setFocusable(true);
}

Window::Window(const Rect& bounds, std::string title, bool modal)
    : Panel(bounds, std::move(title))
    , m_modal(modal)
    , m_hoveredBorderStyle(UIThemes::Border + style::Bold)
    , m_hoveredTitleStyle(UIThemes::PanelTitle + style::Bold)
{
    setFocusable(true);
}

bool Window::isModal() const
{
    return m_modal;
}

void Window::setModal(bool modal)
{
    m_modal = modal;
}

bool Window::isDraggable() const
{
    return m_draggable;
}

void Window::setDraggable(bool draggable)
{
    m_draggable = draggable;
}

bool Window::isResizable() const
{
    return m_resizable;
}

void Window::setResizable(bool resizable)
{
    m_resizable = resizable;
}

Size Window::minimumSize() const
{
    return m_minimumSize;
}

void Window::setMinimumSize(Size size)
{
    m_minimumSize.width = std::max(1, size.width);
    m_minimumSize.height = std::max(1, size.height);
}

void Window::setMinimumSize(int width, int height)
{
    setMinimumSize(Size{ width, height });
}

int Window::resizeBorderThickness() const
{
    return m_resizeBorderThickness;
}

void Window::setResizeBorderThickness(int thickness)
{
    m_resizeBorderThickness = std::max(0, thickness);
}

int Window::titleBarHeight() const
{
    return m_titleBarHeight;
}

void Window::setTitleBarHeight(int height)
{
    m_titleBarHeight = std::max(0, height);
}

const Style& Window::hoveredBorderStyle() const
{
    return m_hoveredBorderStyle;
}

void Window::setHoveredBorderStyle(const Style& style)
{
    m_hoveredBorderStyle = style;
}

const Style& Window::hoveredTitleStyle() const
{
    return m_hoveredTitleStyle;
}

void Window::setHoveredTitleStyle(const Style& style)
{
    m_hoveredTitleStyle = style;
}

bool Window::hasTransferableContent() const
{
    return false;
}

std::unique_ptr<UI::IWindowContent> Window::releaseContent()
{
    return nullptr;
}

UI::CursorRegion Window::hitTest(Point screenPosition) const
{
    UI::CursorRegion region = UI::hitTestWindowBounds(
        bounds(),
        screenPosition,
        m_resizable ? m_resizeBorderThickness : 0,
        m_titleBarHeight);

    const Rect windowBounds = bounds();

    if (region == UI::CursorRegion::TopEdge && m_draggable && hasTitle())
    {
        const int titleY = windowBounds.position.y;
        const int titleX = windowBounds.position.x + 2;
        const int maxTitleWidth = std::max(0, windowBounds.size.width - 4);
        const int titleWidth = std::min(
            maxTitleWidth,
            static_cast<int>(title().size()));

        const bool overTitleText =
            screenPosition.y == titleY &&
            screenPosition.x >= titleX &&
            screenPosition.x < titleX + titleWidth;

        if (overTitleText)
        {
            return UI::CursorRegion::TitleBar;
        }
    }

    if (!m_draggable && region == UI::CursorRegion::TitleBar)
    {
        return UI::CursorRegion::Client;
    }

    return region;
}

bool Window::containsScreenPoint(Point screenPosition) const
{
    return hitTest(screenPosition) != UI::CursorRegion::Outside;
}

bool Window::isHovered() const
{
    return m_hovered;
}

void Window::setHovered(bool hovered)
{
    m_hovered = hovered;
}

void Window::draw(Surface& surface)
{
    const Style originalBorderStyle = borderStyle();
    const Style originalTitleStyle = titleStyle();

    if (isHovered())
    {
        setBorderStyle(m_hoveredBorderStyle);
        setTitleStyle(m_hoveredTitleStyle);
    }

    if (isFocused())
    {
        setBorderStyle(borderStyle() + style::Bold);
        setTitleStyle(titleStyle() + style::Bold);
    }

    Panel::draw(surface);

    setBorderStyle(originalBorderStyle);
    setTitleStyle(originalTitleStyle);
}