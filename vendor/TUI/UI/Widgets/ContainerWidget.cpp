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
    const bool wasActive = m_focusedChild == removedChild.get();
    if (m_rememberedFocus == removedChild.get()) m_rememberedFocus = nullptr;

    if (m_focusedChild == removedChild.get())
    {
        removedChild->blur();
        m_focusedChild = nullptr;
    }

    if (m_mouseCaptureChild == removedChild.get())
    {
        m_mouseCaptureChild = nullptr;
    }

    if (m_mouseHoverChild == removedChild.get())
    {
        m_mouseHoverChild = nullptr;
    }
    removedChild->clearPointerState();

    m_children.erase(m_children.begin() + static_cast<std::ptrdiff_t>(index));

    if (wasActive && !m_focusedChild)
    {
        focusFirstChild();
    }

    return removedChild;
}

void ContainerWidget::clearChildren()
{
    clearPointerState();
    clearChildFocus();
    m_children.clear();
    m_rememberedFocus = nullptr;
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

bool ContainerWidget::isPointerFocusVacant() const
{
    return m_pointerOwnsFocus && !m_focusedChild;
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
    m_rememberedFocus = &child;
    m_focusedChild->focus();

    return true;
}

void ContainerWidget::clearChildFocus()
{
    if (!m_focusedChild)
    {
        return;
    }

    m_rememberedFocus = m_focusedChild;
    m_focusedChild->clearPointerState();
    m_focusedChild->blur();
    m_focusedChild = nullptr;
}

void ContainerWidget::resetFocus()
{
    clearPointerState();
    clearChildFocus();
    m_rememberedFocus = nullptr;
    m_hasMousePosition = false;
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
    clearPointerState();
    return focusChildByDirection(1);
}

bool ContainerWidget::focusPreviousChild()
{
    clearPointerState();
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

    // Layout/async results can hide or replace the item under the stationary
    // pointer. Reconcile it before drawing so a form cannot silently refocus
    // a default button while the mouse is over empty space.
    if (m_pointerOwnsFocus && m_hasMousePosition && !m_mouseCaptureChild)
    {
        Widget* target = hitTest(m_lastMousePosition);
        if (!target || !target->canReceivePointerFocus(m_lastMousePosition)) target = nullptr;
        if (target != m_focusedChild || target != m_mouseHoverChild)
        {
            focusAtPointer(target, m_lastMousePosition);
            if (target)
            {
                Input::MouseEvent moved;
                moved.action = Input::MouseAction::Moved;
                moved.position = m_lastMousePosition;
                target->handleEvent(Input::Event::mouse(moved));
            }
        }
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

    // Keyboard input belongs only to the active control. Never offer an
    // unhandled Confirm to some unrelated button elsewhere in the form.
    if (m_focusedChild && !canFocusChild(*m_focusedChild)) clearChildFocus();
    if (m_focusedChild) return m_focusedChild->handleEvent(event);
    // No active item means no typing or activation, not an implicit default.
    if (event.asKey()) return true;
    const auto* command = event.asCommand();
    return command && command->command.code == Input::CommandCode::Confirm;
}

void ContainerWidget::clearPointerState()
{
    m_pointerOwnsFocus = false;
    m_mouseCaptureChild = nullptr;
    m_mouseHoverChild = nullptr;
    for (auto& child : m_children) child->clearPointerState();
}

void ContainerWidget::onFocusLost()
{
    clearPointerState();
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

    if (!m_focusedChild && m_rememberedFocus && containsChild(*m_rememberedFocus) && canFocusChild(*m_rememberedFocus))
        return setFocusedChild(*m_rememberedFocus);

    int startIndex = m_focusedChild ? indexOfChild(*m_focusedChild)
        : m_rememberedFocus ? indexOfChild(*m_rememberedFocus)
        : direction >= 0 ? -1 : static_cast<int>(m_children.size());

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

void ContainerWidget::focusAtPointer(Widget* target, Point position)
{
    if (!target || !target->canReceivePointerFocus(position)) target = nullptr;
    if (m_mouseHoverChild && m_mouseHoverChild != target && containsChild(*m_mouseHoverChild))
        m_mouseHoverChild->clearPointerState();
    m_mouseHoverChild = target;
    if (target) setFocusedChild(*target);
    else clearChildFocus();
}

bool ContainerWidget::dispatchMouseEvent(const Input::Event& event)
{
    const Input::MouseEvent* mouseEvent = event.asMouse();

    if (!mouseEvent)
    {
        return false;
    }

    Widget* target = hitTest(mouseEvent->position);

    if (mouseEvent->isMove() || mouseEvent->isDrag())
    {
        const bool moved = !m_hasMousePosition || m_lastMousePosition.x != mouseEvent->position.x ||
            m_lastMousePosition.y != mouseEvent->position.y;
        m_hasMousePosition = true;
        m_lastMousePosition = mouseEvent->position;
        // Tab leaves the stationary mouse dormant, even if the terminal sends
        // another move report with the very same position.
        if (!m_pointerOwnsFocus && !moved) return true;
        if (m_mouseCaptureChild
            && containsChild(*m_mouseCaptureChild)
            && m_mouseCaptureChild->isVisible()
            && m_mouseCaptureChild->isEnabled())
        {
            focusAtPointer(m_mouseCaptureChild, mouseEvent->position);
            return m_mouseCaptureChild->handleEvent(event);
        }
        m_pointerOwnsFocus = true;
        focusAtPointer(target, mouseEvent->position);
        return target ? target->handleEvent(event) : false;
    }

    if (mouseEvent->isPress())
    {
        m_pointerOwnsFocus = true;
        m_hasMousePosition = true;
        m_lastMousePosition = mouseEvent->position;
        m_mouseCaptureChild = target;
        focusAtPointer(target, mouseEvent->position);

        return target ? target->handleEvent(event) : false;
    }

    if (mouseEvent->isRelease())
    {
        // A Tab during a press cancels capture. Its later release must not
        // restore hover on the old control or activate it.
        if (!m_pointerOwnsFocus && !m_mouseCaptureChild) return true;
        m_hasMousePosition = true;
        m_lastMousePosition = mouseEvent->position;
        Widget* captured = m_mouseCaptureChild;
        m_mouseCaptureChild = nullptr;
        // Finish pointer bookkeeping BEFORE dispatching activation. Return or
        // another navigation action may destroy this entire container.
        focusAtPointer(target, mouseEvent->position);
        if (m_mouseHoverChild)
        {
            Input::MouseEvent moved = *mouseEvent;
            moved.action = Input::MouseAction::Moved;
            m_mouseHoverChild->handleEvent(Input::Event::mouse(moved));
        }
        if (captured && containsChild(*captured) && captured->isVisible() && captured->isEnabled())
            return captured->handleEvent(event);
        if (captured) return true;
        return target ? target->handleEvent(event) : false;
    }

    if (mouseEvent->isClick())
    {
        m_pointerOwnsFocus = true;
        m_hasMousePosition = true;
        m_lastMousePosition = mouseEvent->position;
        focusAtPointer(target, mouseEvent->position);
    }

    return target ? target->handleEvent(event) : false;
}
