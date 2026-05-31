#include "UI/Widgets/Widget.h"

#include "Input/Event.h"

Widget::Widget() = default;

Widget::Widget(const Rect& bounds)
    : UIElement(bounds)
{
}

Widget::~Widget() = default;

bool Widget::isFocusable() const
{
    return m_focusable;
}

void Widget::setFocusable(bool focusable)
{
    m_focusable = focusable;

    if (!m_focusable)
    {
        setFocused(false);
    }
}

bool Widget::isFocused() const
{
    return m_focused;
}

void Widget::setFocused(bool focused)
{
    const bool shouldBeFocused = focused && canReceiveFocus();

    if (m_focused == shouldBeFocused)
    {
        return;
    }

    m_focused = shouldBeFocused;

    if (m_focused)
    {
        onFocusGained();
    }
    else
    {
        onFocusLost();
    }
}

bool Widget::canReceiveFocus() const
{
    return isVisible() && isEnabled() && m_focusable;
}

void Widget::focus()
{
    setFocused(true);
}

void Widget::blur()
{
    setFocused(false);
}

bool Widget::handleEvent(const Input::Event& event)
{
    (void)event;
    return false;
}

void Widget::onFocusGained()
{
}

void Widget::onFocusLost()
{
}