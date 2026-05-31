#include "UI/Base/Viewport.h"

#include <algorithm>

Viewport::Viewport() = default;

Viewport::Viewport(Size contentSize, Size viewSize)
    : m_contentSize(clampSize(contentSize))
    , m_viewSize(clampSize(viewSize))
{
    clampScrollOffset();
}

Viewport::Viewport(int contentWidth, int contentHeight, int viewWidth, int viewHeight)
    : Viewport(
        Size{ contentWidth, contentHeight },
        Size{ viewWidth, viewHeight })
{
}

Size Viewport::contentSize() const
{
    return m_contentSize;
}

void Viewport::setContentSize(Size size)
{
    m_contentSize = clampSize(size);
    clampScrollOffset();
}

void Viewport::setContentSize(int width, int height)
{
    setContentSize(Size{ width, height });
}

Size Viewport::viewSize() const
{
    return m_viewSize;
}

void Viewport::setViewSize(Size size)
{
    m_viewSize = clampSize(size);
    clampScrollOffset();
}

void Viewport::setViewSize(int width, int height)
{
    setViewSize(Size{ width, height });
}

Point Viewport::scrollOffset() const
{
    return m_scrollOffset;
}

int Viewport::scrollX() const
{
    return m_scrollOffset.x;
}

int Viewport::scrollY() const
{
    return m_scrollOffset.y;
}

int Viewport::maxScrollX() const
{
    return std::max(0, m_contentSize.width - m_viewSize.width);
}

int Viewport::maxScrollY() const
{
    return std::max(0, m_contentSize.height - m_viewSize.height);
}

bool Viewport::canScrollLeft() const
{
    return m_scrollOffset.x > 0;
}

bool Viewport::canScrollRight() const
{
    return m_scrollOffset.x < maxScrollX();
}

bool Viewport::canScrollUp() const
{
    return m_scrollOffset.y > 0;
}

bool Viewport::canScrollDown() const
{
    return m_scrollOffset.y < maxScrollY();
}

void Viewport::scrollBy(int deltaX, int deltaY)
{
    scrollTo(
        m_scrollOffset.x + deltaX,
        m_scrollOffset.y + deltaY);
}

void Viewport::scrollTo(int x, int y)
{
    m_scrollOffset.x = clampValue(x, 0, maxScrollX());
    m_scrollOffset.y = clampValue(y, 0, maxScrollY());
}

void Viewport::scrollToTop()
{
    scrollTo(m_scrollOffset.x, 0);
}

void Viewport::scrollToBottom()
{
    scrollTo(m_scrollOffset.x, maxScrollY());
}

void Viewport::scrollToLeft()
{
    scrollTo(0, m_scrollOffset.y);
}

void Viewport::scrollToRight()
{
    scrollTo(maxScrollX(), m_scrollOffset.y);
}

Point Viewport::contentToView(Point contentPoint) const
{
    return Point{
        contentPoint.x - m_scrollOffset.x,
        contentPoint.y - m_scrollOffset.y
    };
}

Point Viewport::viewToContent(Point viewPoint) const
{
    return Point{
        viewPoint.x + m_scrollOffset.x,
        viewPoint.y + m_scrollOffset.y
    };
}

Rect Viewport::visibleContentRect() const
{
    return Rect{
        m_scrollOffset,
        Size{
            std::min(m_viewSize.width, m_contentSize.width),
            std::min(m_viewSize.height, m_contentSize.height)
        }
    };
}

int Viewport::clampNonNegative(int value)
{
    return std::max(0, value);
}

Size Viewport::clampSize(Size size)
{
    return Size{
        clampNonNegative(size.width),
        clampNonNegative(size.height)
    };
}

int Viewport::clampValue(int value, int minimum, int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

void Viewport::clampScrollOffset()
{
    scrollTo(m_scrollOffset.x, m_scrollOffset.y);
}