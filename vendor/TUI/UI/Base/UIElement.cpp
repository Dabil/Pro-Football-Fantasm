#include "UI/Base/UIElement.h"

UIElement::UIElement() = default;

UIElement::UIElement(const Rect& bounds)
    : m_bounds(bounds)
{
}

UIElement::~UIElement() = default;

const Rect& UIElement::bounds() const
{
    return m_bounds;
}

void UIElement::setBounds(const Rect& bounds)
{
    m_bounds = bounds;
}

Point UIElement::position() const
{
    return m_bounds.position;
}

void UIElement::setPosition(Point position)
{
    m_bounds.position = position;
}

void UIElement::setPosition(int x, int y)
{
    m_bounds.position = { x, y };
}

Size UIElement::size() const
{
    return m_bounds.size;
}

void UIElement::setSize(Size size)
{
    m_bounds.size = size;
}

void UIElement::setSize(int width, int height)
{
    m_bounds.size = { width, height };
}

int UIElement::x() const
{
    return m_bounds.position.x;
}

int UIElement::y() const
{
    return m_bounds.position.y;
}

int UIElement::width() const
{
    return m_bounds.size.width;
}

int UIElement::height() const
{
    return m_bounds.size.height;
}

bool UIElement::isVisible() const
{
    return m_visible;
}

void UIElement::setVisible(bool visible)
{
    m_visible = visible;
}

void UIElement::show()
{
    m_visible = true;
}

void UIElement::hide()
{
    m_visible = false;
}

bool UIElement::isEnabled() const
{
    return m_enabled;
}

void UIElement::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

void UIElement::enable()
{
    m_enabled = true;
}

void UIElement::disable()
{
    m_enabled = false;
}

void UIElement::update(const Animation::TickEvent& event)
{
    (void)event;
}