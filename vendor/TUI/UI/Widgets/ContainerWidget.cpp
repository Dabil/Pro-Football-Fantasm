#include "UI/Widgets/ContainerWidget.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "Input/Command.h"
#include "Input/Event.h"
#include "Input/MouseEvent.h"
#include "Rendering/Surface.h"

ContainerWidget::ContainerWidget() = default;

ContainerWidget::ContainerWidget(const Rect& bounds)
    : Widget(bounds)
{
}

ContainerWidget::~ContainerWidget()
{
    clearChildren();
}

Widget& ContainerWidget::addChild(ChildPtr child)
{
    if (!child)
    {
        throw std::invalid_argument("ContainerWidget::addChild requires a non-null child.");
    }

    Widget& childRef = *child;
    m_children.push_back(std::move(child));

    if (!m_focusedChild && canFocusChild(childRef))
    {
        setFocusedChild(childRef);
    }

    return childRef;
}

ContainerWidget::ChildPtr ContainerWidget::removeChild(const Widget& child)
{
    for (std::size_t index = 0; index < m_children.size(); ++index)
    {
        if (m_children[index].get() == &child)
        {
            return removeChildAt(index);
        }
    }

    return nullptr;
}

ContainerWidget::ChildPtr ContainerWidget::removeChildAt(std::size_t index)
{
    if (index >= m_children.size())
    {
        return nullptr;
    }

    ChildPtr removedChild = std::move(m_children[index]);

    if (m_focusedChild == removedChild.get())
    {
        removedChild->blur();
        m_focusedChild = nullptr;
    }

    if (m_mouseCaptureChild == removedChild.get())
    {
        m_mouseCaptureChild = nullptr;
    }

    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));

    if (!m_focusedChild)
    {
        focusFirstChild();
    }

    return removedChild;
}

void ContainerWidget::clearChildren()
{
    m_mouseCaptureChild = nullptr;
    clearChildFocus();
    m_children.clear();
}

bool ContainerWidget::containsChild(const Widget& child) const
{
    return indexOfChild(child) >= 0;
}

int ContainerWidget::indexOfChild(const Widget& child) const
{
    for (std::size_t index = 0; index < m_children.size(); ++index)
    {
        if (m_children[index].get() == &child)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

std::size_t ContainerWidget::childCount() const
{
    return m_children.size();
}

bool ContainerWidget::empty() const
{
    return m_children.empty();
}

Widget* ContainerWidget::childAt(std::size_t index)
{
    if (index >= m_children.size())
    {
        return nullptr;
    }

    return m_children[index].get();
}

const Widget* ContainerWidget::childAt(std::size_t index) const
{
    if (index >= m_children.size())
    {
        return nullptr;
    }

    return m_children[index].get();
}

Widget* ContainerWidget::focusedChild()
{
    return m_focusedChild;
}

const Widget* ContainerWidget::focusedChild() const
{
    return m_focusedChild;
}

Widget* ContainerWidget::hitTest(Point position)
{
    if (!isVisible() || !isEnabled())
    {
        return nullptr;
    }

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        Widget* child = it->get();

        if (!child || !child->isVisible() || !child->isEnabled())
        {
            continue;
        }

        if (child->bounds().contains(position.x, position.y))
        {
            return child;
        }
    }

    return nullptr;
}

const Widget* ContainerWidget::hitTest(Point position) const
{
    if (!isVisible() || !isEnabled())
    {
        return nullptr;
    }

    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it)
    {
        const Widget* child = it->get();

        if (!child || !child->isVisible() || !child->isEnabled())
        {
            continue;
        }

        if (child->bounds().contains(position.x, position.y))
        {
            return child;
        }
    }

    return nullptr;
}

bool ContainerWidget::setFocusedChild(Widget& child)
{
    if (!containsChild(child) || !canFocusChild(child))
    {
        return false;
    }

    if (m_focusedChild == &child)
    {
        return true;
    }

    clearChildFocus();

    m_focusedChild = &child;
    m_focusedChild->focus();

    return true;
}

void ContainerWidget::clearChildFocus()
{
    if (!m_focusedChild)
    {
        return;
    }

    m_focusedChild->blur();
    m_focusedChild = nullptr;
}

bool ContainerWidget::focusFirstChild()
{
    for (std::size_t index = 0; index < m_children.size(); ++index)
    {
        if (focusChildAt(index))
        {
            return true;
        }
    }

    return false;
}

bool ContainerWidget::focusLastChild()
{
    for (std::size_t offset = 0; offset < m_children.size(); ++offset)
    {
        const std::size_t index = m_children.size() - 1 - offset;

        if (focusChildAt(index))
        {
            return true;
        }
    }

    return false;
}

bool ContainerWidget::focusNextChild()
{
    return focusChildByDirection(1);
}

bool ContainerWidget::focusPreviousChild()
{
    return focusChildByDirection(-1);
}

void ContainerWidget::update(const Animation::TickEvent& event)
{
    Widget::update(event);

    if (!isVisible())
    {
        return;
    }

    for (ChildPtr& child : m_children)
    {
        if (child && child->isVisible())
        {
            child->update(event);
        }
    }
}

void ContainerWidget::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    for (ChildPtr& child : m_children)
    {
        if (child && child->isVisible())
        {
            child->draw(surface);
        }
    }
}

bool ContainerWidget::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        return false;
    }

    if (event.asMouse())
    {
        return dispatchMouseEvent(event);
    }

    if (const Input::CommandEvent* commandEvent = event.asCommand())
    {
        switch (commandEvent->command.code)
        {
        case Input::CommandCode::NextFocus:
            return focusNextChild();

        case Input::CommandCode::PreviousFocus:
            return focusPreviousChild();

        default:
            break;
        }
    }

    if (m_focusedChild && m_focusedChild->handleEvent(event))
    {
        return true;
    }

    for (ChildPtr& child : m_children)
    {
        if (!child || child.get() == m_focusedChild)
        {
            continue;
        }

        if (!child->isVisible() || !child->isEnabled())
        {
            continue;
        }

        if (child->handleEvent(event))
        {
            return true;
        }
    }

    return false;
}

void ContainerWidget::onFocusLost()
{
    m_mouseCaptureChild = nullptr;
    clearChildFocus();
}

bool ContainerWidget::canFocusChild(const Widget& child) const
{
    return child.canReceiveFocus();
}

bool ContainerWidget::focusChildAt(std::size_t index)
{
    Widget* child = childAt(index);

    if (!child || !canFocusChild(*child))
    {
        return false;
    }

    return setFocusedChild(*child);
}

bool ContainerWidget::focusChildByDirection(int direction)
{
    if (m_children.empty())
    {
        return false;
    }

    int startIndex = indexOfChild(m_focusedChild ? *m_focusedChild : *m_children.front());

    if (startIndex < 0)
    {
        startIndex = direction >= 0
            ? -1
            : static_cast<int>(m_children.size());
    }

    for (std::size_t step = 1; step <= m_children.size(); ++step)
    {
        const int rawIndex = startIndex + direction * static_cast<int>(step);
        const int wrappedIndex =
            (rawIndex + static_cast<int>(m_children.size()))
            % static_cast<int>(m_children.size());

        if (focusChildAt(static_cast<std::size_t>(wrappedIndex)))
        {
            return true;
        }
    }

    return false;
}

bool ContainerWidget::dispatchMouseEvent(const Input::Event& event)
{
    const Input::MouseEvent* mouseEvent = event.asMouse();

    if (!mouseEvent)
    {
        return false;
    }

    Widget* target = hitTest(mouseEvent->position);

    if (mouseEvent->isPress())
    {
        m_mouseCaptureChild = target;

        if (target && canFocusChild(*target))
        {
            setFocusedChild(*target);
        }

        return target ? target->handleEvent(event) : false;
    }

    if (mouseEvent->isRelease())
    {
        Widget* captured = m_mouseCaptureChild;
        m_mouseCaptureChild = nullptr;

        if (captured && containsChild(*captured) && captured->isVisible() && captured->isEnabled())
        {
            return captured->handleEvent(event);
        }

        return target ? target->handleEvent(event) : false;
    }

    if (mouseEvent->isClick() && target && canFocusChild(*target))
    {
        setFocusedChild(*target);
    }

    return target ? target->handleEvent(event) : false;
}