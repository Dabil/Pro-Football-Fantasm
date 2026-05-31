#include "UI/Base/WindowManager.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <sstream>

#include "Core/Rect.h"
#include "UI/Panels/Window.h"
#include "UI/Panels/ContentWindow.h"
#include "UI/Panels/TabbedWindow.h"
#include "Input/Event.h"
#include "Input/MouseEvent.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/StyleBuilder.h"

namespace
{
    UI::PointerButton toPointerButton(Input::MouseButton button)
    {
        switch (button)
        {
        case Input::MouseButton::Left:
            return UI::PointerButton::Primary;
        case Input::MouseButton::Right:
            return UI::PointerButton::Secondary;
        case Input::MouseButton::Middle:
            return UI::PointerButton::Middle;
        default:
            return UI::PointerButton::None;
        }
    }
}

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() = default;

void WindowManager::addWindow(Window& window)
{
    if (contains(window))
    {
        return;
    }

    m_windows.push_back({
        &window,
        nextFrontZOrder(),
        m_nextInsertionOrder++
        });

    sortWindows();
}

void WindowManager::removeWindow(Window& window)
{
    if (m_hoveredWindow == &window)
    {
        setHoveredWindow(nullptr);
    }

    if (m_focusedWindow == &window)
    {
        setFocusedWindow(nullptr);
    }

    if (m_pointerCapture.window == &window)
    {
        releasePointer();
    }

    if (m_tabDetachDragState.sourceWindow == &window)
    {
        m_tabDetachDragState.clear();
    }

    m_windows.erase(
        std::remove_if(
            m_windows.begin(),
            m_windows.end(),
            [&window](const ManagedWindow& entry)
            {
                return entry.window == &window;
            }),
        m_windows.end());

    m_ownedWindows.erase(
        std::remove_if(
            m_ownedWindows.begin(),
            m_ownedWindows.end(),
            [&window](const std::unique_ptr<Window>& ownedWindow)
            {
                return ownedWindow.get() == &window;
            }),
        m_ownedWindows.end());

    m_ownedTabbedWindows.erase(
        std::remove_if(
            m_ownedTabbedWindows.begin(),
            m_ownedTabbedWindows.end(),
            [&window](const std::unique_ptr<UI::TabbedWindow>& ownedWindow)
            {
                return ownedWindow.get() == &window;
            }),
        m_ownedTabbedWindows.end());
}

void WindowManager::clear()
{
    setHoveredWindow(nullptr);
    setFocusedWindow(nullptr);
    releasePointer();
    m_dragState.clear();
    m_tabDetachDragState.clear();
    m_resizeState.clear();
    m_dockPreview.cancel();
    m_windows.clear();
    m_ownedWindows.clear();
    m_ownedTabbedWindows.clear();
}

bool WindowManager::contains(const Window& window) const
{
    return findEntry(window) != nullptr;
}

std::size_t WindowManager::count() const
{
    return m_windows.size();
}

bool WindowManager::empty() const
{
    return m_windows.empty();
}

void WindowManager::show(Window& window)
{
    if (!contains(window))
    {
        addWindow(window);
    }

    window.show();
}

void WindowManager::hide(Window& window)
{
    if (ManagedWindow* entry = findEntry(window))
    {
        if (m_hoveredWindow == &window)
        {
            setHoveredWindow(nullptr);
        }

        if (m_focusedWindow == &window)
        {
            setFocusedWindow(nullptr);
        }

        entry->window->hide();
    }
}

void WindowManager::bringToFront(Window& window)
{
    ManagedWindow* entry = findEntry(window);

    if (entry == nullptr)
    {
        addWindow(window);
        return;
    }

    entry->zOrder = nextFrontZOrder();
    entry->insertionOrder = m_nextInsertionOrder++;

    sortWindows();
}

void WindowManager::sendToBack(Window& window)
{
    ManagedWindow* entry = findEntry(window);

    if (entry == nullptr)
    {
        addWindow(window);
        entry = findEntry(window);
    }

    if (entry == nullptr)
    {
        return;
    }

    entry->zOrder = nextBackZOrder();
    entry->insertionOrder = m_nextInsertionOrder++;

    sortWindows();
}

void WindowManager::setZOrder(Window& window, int zOrder)
{
    ManagedWindow* entry = findEntry(window);

    if (entry == nullptr)
    {
        addWindow(window);
        entry = findEntry(window);
    }

    if (entry == nullptr)
    {
        return;
    }

    entry->zOrder = zOrder;
    entry->insertionOrder = m_nextInsertionOrder++;

    sortWindows();
}

int WindowManager::zOrderOf(const Window& window) const
{
    const ManagedWindow* entry = findEntry(window);
    return entry != nullptr ? entry->zOrder : 0;
}

Window* WindowManager::topWindow()
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        if (it->window != nullptr && it->window->isVisible())
        {
            return it->window;
        }
    }

    return nullptr;
}

const Window* WindowManager::topWindow() const
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        if (it->window != nullptr && it->window->isVisible())
        {
            return it->window;
        }
    }

    return nullptr;
}

Window* WindowManager::topModalWindow()
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        if (it->window != nullptr &&
            it->window->isVisible() &&
            it->window->isModal())
        {
            return it->window;
        }
    }

    return nullptr;
}

const Window* WindowManager::topModalWindow() const
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        if (it->window != nullptr &&
            it->window->isVisible() &&
            it->window->isModal())
        {
            return it->window;
        }
    }

    return nullptr;
}

Window* WindowManager::hoveredWindow()
{
    return m_hoveredWindow;
}

const Window* WindowManager::hoveredWindow() const
{
    return m_hoveredWindow;
}

Window* WindowManager::focusedWindow()
{
    return m_focusedWindow;
}

const Window* WindowManager::focusedWindow() const
{
    return m_focusedWindow;
}

bool WindowManager::hasModalWindow() const
{
    return topModalWindow() != nullptr;
}

bool WindowManager::canRouteTo(const Window& window) const
{
    if (!window.isVisible() || !window.isEnabled())
    {
        return false;
    }

    const ManagedWindow* targetEntry = findEntry(window);

    if (targetEntry == nullptr)
    {
        return false;
    }

    const Window* modalWindow = topModalWindow();

    if (modalWindow == nullptr)
    {
        return true;
    }

    const ManagedWindow* modalEntry = findEntry(*modalWindow);

    if (modalEntry == nullptr)
    {
        return true;
    }

    return targetEntry->zOrder >= modalEntry->zOrder;
}

bool WindowManager::handleEvent(const Input::Event& event)
{
    const Input::MouseEvent* mouseEvent = event.asMouse();
    if (!mouseEvent)
    {
        return false;
    }

    return handleMouseEvent(*mouseEvent);
}

bool WindowManager::handleMouseEvent(const Input::MouseEvent& mouseEvent)
{
    if (isResizing())
    {
        if (mouseEvent.isRelease())
        {
            endResize();
            return true;
        }

        if (mouseEvent.isDrag() || mouseEvent.isMove())
        {
            return updateResize(mouseEvent.position);
        }

        return true;
    }

    if (isTabDetachDragging())
    {
        if (mouseEvent.isRelease())
        {
            endTabDetachDrag();
            return true;
        }

        if (mouseEvent.isDrag() || mouseEvent.isMove())
        {
            return updateTabDetachDrag(mouseEvent.position, mouseEvent.modifiers);
        }

        return true;
    }

    if (isDragging())
    {
        if (mouseEvent.isRelease())
        {
            endDrag();
            return true;
        }

        if (mouseEvent.isDrag() || mouseEvent.isMove())
        {
            return updateDrag(mouseEvent.position, mouseEvent.modifiers);
        }

        return true;
    }

    UI::WindowHitTestResult result = hitTest(mouseEvent.position);
    Window* target = result.hit() ? result.window : nullptr;

    setHoveredWindow(target);

    if (target == nullptr)
    {
        return false;
    }

    const bool isPrimaryPress =
        mouseEvent.button == Input::MouseButton::Left &&
        mouseEvent.isPress();

    const bool isPrimaryActivation =
        mouseEvent.button == Input::MouseButton::Left &&
        (mouseEvent.isPress() || mouseEvent.isClick());

    if (isPrimaryActivation)
    {
        setFocusedWindow(target);
        bringToFront(*target);
        setHoveredWindow(target);
    }

    if (isPrimaryPress && UI::isResizeRegion(result.region))
    {
        if (beginResize(
            *target,
            result.region,
            mouseEvent.position,
            toPointerButton(mouseEvent.button)))
        {
            return true;
        }
    }

    if (isPrimaryPress && result.region == UI::CursorRegion::TitleBar)
    {
        if (UI::TabbedWindow* tabbedTarget = dynamic_cast<UI::TabbedWindow*>(target))
        {
            const int tabIndex = tabbedTarget->tabIndexAt(mouseEvent.position);

            if (tabIndex >= 0)
            {
                if (isDockingModifierHeld(mouseEvent.modifiers))
                {
                    tabbedTarget->selectTab(static_cast<std::size_t>(tabIndex));

                    if (beginTabDetachDrag(
                        *tabbedTarget,
                        static_cast<std::size_t>(tabIndex),
                        mouseEvent.position,
                        toPointerButton(mouseEvent.button)))
                    {
                        return true;
                    }
                }

                tabbedTarget->handleEvent(Input::Event::mouse(mouseEvent));
            }
        }

        if (beginDrag(
            *target,
            mouseEvent.position,
            toPointerButton(mouseEvent.button)))
        {
            return true;
        }
    }

    return target->handleEvent(Input::Event::mouse(mouseEvent)) || isPrimaryActivation;
}

UI::WindowHitTestResult WindowManager::hitTest(Point screenPosition)
{
    const WindowManager* self = this;
    UI::WindowHitTestResult result = self->hitTest(screenPosition);
    return result;
}

UI::WindowHitTestResult WindowManager::hitTest(Point screenPosition) const
{
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it)
    {
        const ManagedWindow& entry = *it;

        if (entry.window == nullptr)
        {
            continue;
        }

        if (!canRouteTo(*entry.window))
        {
            continue;
        }

        const UI::CursorRegion region = entry.window->hitTest(screenPosition);
        if (region == UI::CursorRegion::Outside)
        {
            continue;
        }

        return UI::WindowHitTestResult{
            entry.window,
            region,
            screenPosition,
            Point{
                screenPosition.x - entry.window->x(),
                screenPosition.y - entry.window->y()
            }
        };
    }

    return UI::WindowHitTestResult{
        nullptr,
        UI::CursorRegion::Outside,
        screenPosition,
        Point{}
    };
}

void WindowManager::capturePointer(
    Window& window,
    UI::PointerButton button,
    Point screenPosition)
{
    if (!canRouteTo(window))
    {
        return;
    }

    m_pointerCapture.window = &window;
    m_pointerCapture.button = button;
    m_pointerCapture.origin = screenPosition;
    m_pointerCapture.current = screenPosition;
}

void WindowManager::releasePointer(Window& window)
{
    if (m_pointerCapture.window == &window)
    {
        releasePointer();
    }
}

void WindowManager::releasePointer()
{
    m_pointerCapture.clear();
}

bool WindowManager::hasPointerCapture() const
{
    return m_pointerCapture.active();
}

const UI::PointerCaptureState& WindowManager::pointerCapture() const
{
    return m_pointerCapture;
}

Window* WindowManager::capturedWindow()
{
    return m_pointerCapture.window;
}

const Window* WindowManager::capturedWindow() const
{
    return m_pointerCapture.window;
}

bool WindowManager::beginDrag(
    Window& window,
    Point screenPosition,
    UI::PointerButton button)
{
    if (!canRouteTo(window) || !window.isDraggable())
    {
        return false;
    }

    m_resizeState.clear();
    m_dockPreview.cancel();

    m_dragState.window = &window;
    m_dragState.pointerOrigin = screenPosition;
    m_dragState.currentPointer = screenPosition;
    m_dragState.originalBounds = window.bounds();

    capturePointer(window, button, screenPosition);
    bringToFront(window);

    return true;
}


bool WindowManager::beginDragAt(Point screenPosition, UI::PointerButton button)
{
    UI::WindowHitTestResult result = hitTest(screenPosition);
    if (!result.hit() || result.region != UI::CursorRegion::TitleBar)
    {
        return false;
    }

    return beginDrag(*result.window, screenPosition, button);
}

bool WindowManager::updateDrag(
    Point screenPosition,
    const Input::KeyModifiers& modifiers)
{
    if (!m_dragState.active())
    {
        return false;
    }

    Window* window = m_dragState.window;
    if (window == nullptr)
    {
        m_dragState.clear();
        releasePointer();
        m_dockPreview.cancel();
        return false;
    }

    m_dragState.currentPointer = screenPosition;
    m_pointerCapture.current = screenPosition;

    const int deltaX = screenPosition.x - m_dragState.pointerOrigin.x;
    const int deltaY = screenPosition.y - m_dragState.pointerOrigin.y;

    Rect movedBounds = m_dragState.originalBounds;
    movedBounds.position.x += deltaX;
    movedBounds.position.y += deltaY;
    window->setBounds(movedBounds);

    updateDockPreview(screenPosition, modifiers);

    return true;
}


bool WindowManager::beginTabDetachDrag(
    UI::TabbedWindow& sourceWindow,
    std::size_t tabIndex,
    Point screenPosition,
    UI::PointerButton button)
{
    if (!canRouteTo(sourceWindow) || tabIndex >= sourceWindow.tabCount())
    {
        return false;
    }

    const UI::TabbedWindowPage& page = sourceWindow.model().pages()[tabIndex];

    if (!page.isValid() || page.contentId().empty())
    {
        return false;
    }

    m_dragState.clear();
    m_resizeState.clear();
    m_dockPreview.cancel();

    m_tabDetachDragState.sourceWindow = &sourceWindow;
    m_tabDetachDragState.tabIndex = tabIndex;
    m_tabDetachDragState.contentId = page.contentId();
    m_tabDetachDragState.pointerOrigin = screenPosition;
    m_tabDetachDragState.currentPointer = screenPosition;
    m_tabDetachDragState.sourceBounds = sourceWindow.bounds();
    m_tabDetachDragState.previewBounds = makeTabDetachPreviewBounds(
        sourceWindow,
        screenPosition);
    m_tabDetachDragState.active = true;

    capturePointer(sourceWindow, button, screenPosition);
    bringToFront(sourceWindow);

    return true;
}

bool WindowManager::updateTabDetachDrag(
    Point screenPosition,
    const Input::KeyModifiers& modifiers)
{
    if (!m_tabDetachDragState.active)
    {
        return false;
    }

    UI::TabbedWindow* sourceWindow = m_tabDetachDragState.sourceWindow;

    if (sourceWindow == nullptr || !isDockingModifierHeld(modifiers))
    {
        cancelTabDetachPreview();
        return true;
    }

    m_tabDetachDragState.currentPointer = screenPosition;
    m_pointerCapture.current = screenPosition;
    m_tabDetachDragState.previewBounds = makeTabDetachPreviewBounds(
        *sourceWindow,
        screenPosition);

    return true;
}

void WindowManager::endTabDetachDrag()
{
    completeTabDetachDrag();
    m_tabDetachDragState.clear();
    releasePointer();
    m_dockPreview.cancel();
}

bool WindowManager::completeTabDetachDrag()
{
    if (!m_tabDetachDragState.active || m_tabDetachDragState.sourceWindow == nullptr)
    {
        return false;
    }

    UI::TabbedWindow* sourceWindow = m_tabDetachDragState.sourceWindow;

    if (!contains(*sourceWindow))
    {
        return false;
    }

    const std::string detachedContentId = m_tabDetachDragState.contentId;

    if (detachedContentId.empty())
    {
        return false;
    }

    int tabIndex = sourceWindow->model().indexOfContentId(detachedContentId);

    if (tabIndex < 0 &&
        m_tabDetachDragState.tabIndex < sourceWindow->tabCount())
    {
        const UI::TabbedWindowPage& fallbackPage =
            sourceWindow->model().pages()[m_tabDetachDragState.tabIndex];

        if (fallbackPage.contentId() == detachedContentId)
        {
            tabIndex = static_cast<int>(m_tabDetachDragState.tabIndex);
        }
    }

    if (tabIndex < 0)
    {
        return false;
    }

    const Rect previewBounds = m_tabDetachDragState.previewBounds;

    if (previewBounds.size.width <= 0 || previewBounds.size.height <= 0)
    {
        return false;
    }

    UI::TabbedWindowPage page =
        sourceWindow->removePageAt(static_cast<std::size_t>(tabIndex));

    if (!page.isValid())
    {
        return false;
    }

    std::unique_ptr<Window> detachedWindow = makeStandaloneWindowFromTabPage(
        std::move(page),
        previewBounds);

    if (detachedWindow == nullptr)
    {
        return false;
    }

    Window* rawDetachedWindow = detachedWindow.get();

    m_ownedWindows.push_back(std::move(detachedWindow));
    addWindow(*rawDetachedWindow);
    show(*rawDetachedWindow);

    if (sourceWindow->empty())
    {
        removeWindow(*sourceWindow);
    }
    else if (sourceWindow->tabCount() == 1)
    {
        collapseOneTabTabbedWindowToStandalone(*sourceWindow);
    }

    bringToFront(*rawDetachedWindow);
    setFocusedWindow(rawDetachedWindow);
    setHoveredWindow(rawDetachedWindow);

    return true;
}

bool WindowManager::isTabDetachDragging() const
{
    return m_tabDetachDragState.active;
}

Rect WindowManager::makeTabDetachPreviewBounds(
    const UI::TabbedWindow& sourceWindow,
    Point screenPosition) const
{
    const Rect sourceBounds = sourceWindow.bounds();

    const int previewWidth = std::max(8, sourceBounds.size.width);
    const int previewHeight = std::max(4, sourceBounds.size.height);

    return Rect{
        Point{ screenPosition.x + 1, screenPosition.y + 1 },
        Size{ previewWidth, previewHeight }
    };
}

void WindowManager::drawTabDetachPreview(Surface& surface) const
{
    if (!m_tabDetachDragState.active)
    {
        return;
    }

    const Rect previewBounds = m_tabDetachDragState.previewBounds;

    if (previewBounds.size.width <= 0 || previewBounds.size.height <= 0)
    {
        return;
    }

    ScreenBuffer& buffer = surface.buffer();

    const Style previewStyle =
        style::Fg(Color::FromBasic(Color::Basic::BrightMagenta))
        + style::Bg(Color::FromBasic(Color::Basic::Blue));

    buffer.fillRect(previewBounds, U'.', previewStyle);
    buffer.drawFrame(
        previewBounds,
        previewStyle,
        U'+',
        U'+',
        U'+',
        U'+',
        U'=',
        U'|');

    if (!m_tabDetachDragState.contentId.empty() && previewBounds.size.width > 4)
    {
        const int maxTitleWidth = previewBounds.size.width - 4;
        std::string title = m_tabDetachDragState.contentId;

        if (static_cast<int>(title.size()) > maxTitleWidth)
        {
            title = title.substr(0, static_cast<std::size_t>(maxTitleWidth - 1)) + "~";
        }

        buffer.writeString(
            previewBounds.position.x + 2,
            previewBounds.position.y,
            title,
            previewStyle.withReverse(true));
    }
}

void WindowManager::endDrag()
{
    tryApplyDockPreviewDrop();

    m_dragState.clear();
    releasePointer();
    m_dockPreview.end();
}

bool WindowManager::isDragging() const
{
    return m_dragState.active();
}

const UI::WindowDragState& WindowManager::dragState() const
{
    return m_dragState;
}

bool WindowManager::beginResize(
    Window& window,
    UI::CursorRegion region,
    Point screenPosition,
    UI::PointerButton button)
{
    if (!canRouteTo(window) || !window.isResizable() || !UI::isResizeRegion(region))
    {
        return false;
    }

    m_dragState.clear();
    m_dockPreview.cancel();

    m_resizeState.window = &window;
    m_resizeState.region = region;
    m_resizeState.pointerOrigin = screenPosition;
    m_resizeState.currentPointer = screenPosition;
    m_resizeState.originalBounds = window.bounds();
    m_resizeState.minimumSize = window.minimumSize();

    capturePointer(window, button, screenPosition);
    bringToFront(window);
    return true;
}

bool WindowManager::beginResizeAt(Point screenPosition, UI::PointerButton button)
{
    UI::WindowHitTestResult result = hitTest(screenPosition);
    if (!result.hit() || !UI::isResizeRegion(result.region))
    {
        return false;
    }

    return beginResize(*result.window, result.region, screenPosition, button);
}

bool WindowManager::updateResize(Point screenPosition)
{
    if (!m_resizeState.active())
    {
        return false;
    }

    Window* window = m_resizeState.window;
    if (window == nullptr)
    {
        m_resizeState.clear();
        releasePointer();
        return false;
    }

    m_resizeState.currentPointer = screenPosition;
    m_pointerCapture.current = screenPosition;

    window->setBounds(UI::resizedBounds(
        m_resizeState.originalBounds,
        m_resizeState.region,
        m_resizeState.pointerOrigin,
        screenPosition,
        m_resizeState.minimumSize));

    return true;
}

void WindowManager::endResize()
{
    m_resizeState.clear();
    releasePointer();
    m_dockPreview.cancel();
}

bool WindowManager::isResizing() const
{
    return m_resizeState.active();
}

const UI::WindowResizeState& WindowManager::resizeState() const
{
    return m_resizeState;
}

void WindowManager::update(const Animation::TickEvent& event)
{
    for (ManagedWindow& entry : m_windows)
    {
        if (entry.window == nullptr)
        {
            continue;
        }

        if (!canRouteTo(*entry.window))
        {
            continue;
        }

        entry.window->update(event);
    }
}

void WindowManager::draw(Surface& surface)
{
    sortWindows();

    for (ManagedWindow& entry : m_windows)
    {
        if (entry.window == nullptr || !entry.window->isVisible())
        {
            continue;
        }

        entry.window->draw(surface);
    }

    m_dockPreview.draw(surface);
    drawTabDetachPreview(surface);
}

std::vector<LayerInstance> WindowManager::buildLayers()
{
    sortWindows();

    std::vector<LayerInstance> layers;
    layers.reserve(m_windows.size());

    for (ManagedWindow& entry : m_windows)
    {
        Window* window = entry.window;

        if (window == nullptr || !window->isVisible())
        {
            continue;
        }

        const Rect originalBounds = window->bounds();

        if (originalBounds.size.width <= 0 || originalBounds.size.height <= 0)
        {
            continue;
        }

        Surface layerSurface(
            originalBounds.size.width,
            originalBounds.size.height);

        window->setPosition(0, 0);
        window->draw(layerSurface);
        window->setBounds(originalBounds);

        layers.emplace_back(
            std::move(layerSurface),
            originalBounds.position,
            entry.zOrder,
            true);
    }

    return layers;
}

WindowManager::ManagedWindow* WindowManager::findEntry(Window& window)
{
    auto it = std::find_if(
        m_windows.begin(),
        m_windows.end(),
        [&window](const ManagedWindow& entry)
        {
            return entry.window == &window;
        });

    return it != m_windows.end() ? &(*it) : nullptr;
}

const WindowManager::ManagedWindow* WindowManager::findEntry(const Window& window) const
{
    auto it = std::find_if(
        m_windows.begin(),
        m_windows.end(),
        [&window](const ManagedWindow& entry)
        {
            return entry.window == &window;
        });

    return it != m_windows.end() ? &(*it) : nullptr;
}

void WindowManager::sortWindows()
{
    std::stable_sort(
        m_windows.begin(),
        m_windows.end(),
        [](const ManagedWindow& lhs, const ManagedWindow& rhs)
        {
            if (lhs.zOrder != rhs.zOrder)
            {
                return lhs.zOrder < rhs.zOrder;
            }

            return lhs.insertionOrder < rhs.insertionOrder;
        });
}

int WindowManager::nextFrontZOrder() const
{
    if (m_windows.empty())
    {
        return 0;
    }

    const auto it = std::max_element(
        m_windows.begin(),
        m_windows.end(),
        [](const ManagedWindow& lhs, const ManagedWindow& rhs)
        {
            return lhs.zOrder < rhs.zOrder;
        });

    return it->zOrder + 1;
}

int WindowManager::nextBackZOrder() const
{
    if (m_windows.empty())
    {
        return 0;
    }

    const auto it = std::min_element(
        m_windows.begin(),
        m_windows.end(),
        [](const ManagedWindow& lhs, const ManagedWindow& rhs)
        {
            return lhs.zOrder < rhs.zOrder;
        });

    return it->zOrder - 1;
}

void WindowManager::setHoveredWindow(Window* window)
{
    if (m_hoveredWindow == window)
    {
        return;
    }

    if (m_hoveredWindow != nullptr)
    {
        m_hoveredWindow->setHovered(false);
    }

    m_hoveredWindow = window;

    if (m_hoveredWindow != nullptr)
    {
        m_hoveredWindow->setHovered(true);
    }
}

void WindowManager::setFocusedWindow(Window* window)
{
    if (m_focusedWindow == window)
    {
        return;
    }

    if (m_focusedWindow != nullptr)
    {
        m_focusedWindow->blur();
    }

    m_focusedWindow = window;

    if (m_focusedWindow != nullptr && m_focusedWindow->canReceiveFocus())
    {
        m_focusedWindow->focus();
    }
}

bool WindowManager::isDockingModifierHeld(
    const Input::KeyModifiers& modifiers) const
{
    return modifiers.ctrl;
}

void WindowManager::updateDockPreview(
    Point screenPosition,
    const Input::KeyModifiers& modifiers)
{
    if (!m_dragState.active() || !isDockingModifierHeld(modifiers))
    {
        m_dockPreview.cancel();
        return;
    }

    const UI::DockTarget target = dockTargetAt(screenPosition);

    if (!target.isValid())
    {
        m_dockPreview.cancel();
        return;
    }

    const std::vector<UI::DockSnapZone> zones =
        createDockSnapZonesForTarget(target);

    if (zones.empty())
    {
        m_dockPreview.cancel();
        return;
    }

    if (!m_dockPreview.isActive())
    {
        m_dockPreview.begin(zones, screenPosition);
    }
    else
    {
        m_dockPreview.update(zones, screenPosition);
    }
}

void WindowManager::setDockTree(UI::DockTree* dockTree)
{
    m_dockTree = dockTree;

    if (m_dockTree == nullptr)
    {
        m_dockPreview.cancel();
    }
}

UI::DockTree* WindowManager::dockTree()
{
    return m_dockTree;
}

const UI::DockTree* WindowManager::dockTree() const
{
    return m_dockTree;
}

bool WindowManager::isDockPreviewActive() const
{
    return m_dockPreview.isActive();
}

const UI::DockDragPreviewState& WindowManager::dockPreviewState() const
{
    return m_dockPreview.state();
}

void WindowManager::cancelDockPreview()
{
    m_dockPreview.cancel();
}

bool WindowManager::isTabDetachPreviewActive() const
{
    return m_tabDetachDragState.active;
}

Rect WindowManager::tabDetachPreviewBounds() const
{
    return m_tabDetachDragState.previewBounds;
}

void WindowManager::cancelTabDetachPreview()
{
    m_tabDetachDragState.clear();
    releasePointer();
}

std::vector<UI::DockTarget> WindowManager::dockTargets() const
{
    std::vector<ManagedWindow> entries = m_windows;

    std::stable_sort(
        entries.begin(),
        entries.end(),
        [](const ManagedWindow& lhs, const ManagedWindow& rhs)
        {
            if (lhs.zOrder != rhs.zOrder)
            {
                return lhs.zOrder < rhs.zOrder;
            }

            return lhs.insertionOrder < rhs.insertionOrder;
        });

    std::vector<UI::DockTarget> targets;

    for (const ManagedWindow& entry : entries)
    {
        if (entry.window == nullptr || !isDockTargetEligible(*entry.window))
        {
            continue;
        }

        targets.push_back(UI::DockTarget{
            entry.window,
            dockContentIdForWindow(*entry.window),
            entry.window->bounds(),
            entry.zOrder
            });
    }

    return targets;
}

UI::DockTarget WindowManager::dockTargetAt(Point screenPosition) const
{
    std::vector<UI::DockTarget> targets = dockTargets();

    for (auto it = targets.rbegin(); it != targets.rend(); ++it)
    {
        if (it->isValid() &&
            it->bounds.contains(screenPosition.x, screenPosition.y))
        {
            return *it;
        }
    }

    return UI::DockTarget{};
}

std::vector<UI::DockSnapZone> WindowManager::dockSnapZonesForTargets() const
{
    std::vector<UI::DockSnapZone> zones;

    for (const UI::DockTarget& target : dockTargets())
    {
        std::vector<UI::DockSnapZone> targetZones =
            createDockSnapZonesForTarget(target);

        zones.insert(
            zones.end(),
            targetZones.begin(),
            targetZones.end());
    }

    return zones;
}

bool WindowManager::isDockTargetEligible(const Window& window) const
{
    if (&window == m_dragState.window)
    {
        return false;
    }

    if (!window.isVisible() || !window.isEnabled())
    {
        return false;
    }

    const Rect bounds = window.bounds();

    return bounds.size.width > 0 && bounds.size.height > 0;
}

std::string WindowManager::dockContentIdForWindow(const Window& window) const
{
    if (window.hasTitle())
    {
        return window.title();
    }

    std::ostringstream stream;
    stream << "window@" << &window;
    return stream.str();
}

std::vector<UI::DockSnapZone> WindowManager::createDockSnapZonesForTarget(
    const UI::DockTarget& target) const
{
    std::vector<UI::DockSnapZone> zones;

    if (!target.isValid())
    {
        return zones;
    }

    const Rect bounds = target.bounds;

    if (bounds.size.width <= 0 || bounds.size.height <= 0)
    {
        return zones;
    }

    const int leftWidth = std::max(1, bounds.size.width / 3);
    const int rightWidth = std::max(1, bounds.size.width / 3);
    const int topHeight = std::max(1, bounds.size.height / 3);
    const int bottomHeight = std::max(1, bounds.size.height / 3);

    const int centerX = bounds.position.x + leftWidth;
    const int centerY = bounds.position.y + topHeight;
    const int centerWidth = std::max(1, bounds.size.width - leftWidth - rightWidth);
    const int centerHeight = std::max(1, bounds.size.height - topHeight - bottomHeight);

    zones.push_back(UI::DockSnapZone{
        UI::DockSnapZoneType::Top,
        Rect{
            bounds.position,
            Size{ bounds.size.width, topHeight }
        },
        0,
        target.contentId
        });

    zones.push_back(UI::DockSnapZone{
        UI::DockSnapZoneType::Bottom,
        Rect{
            Point{
                bounds.position.x,
                bounds.position.y + bounds.size.height - bottomHeight
            },
            Size{ bounds.size.width, bottomHeight }
        },
        0,
        target.contentId
        });

    zones.push_back(UI::DockSnapZone{
        UI::DockSnapZoneType::Left,
        Rect{
            bounds.position,
            Size{ leftWidth, bounds.size.height }
        },
        0,
        target.contentId
        });

    zones.push_back(UI::DockSnapZone{
        UI::DockSnapZoneType::Right,
        Rect{
            Point{
                bounds.position.x + bounds.size.width - rightWidth,
                bounds.position.y
            },
            Size{ rightWidth, bounds.size.height }
        },
        0,
        target.contentId
        });

    zones.push_back(UI::DockSnapZone{
        UI::DockSnapZoneType::Center,
        Rect{
            Point{ centerX, centerY },
            Size{ centerWidth, centerHeight }
        },
        0,
        target.contentId
        });

    return zones;
}

bool WindowManager::tryApplyDockPreviewDrop()
{
    if (!m_dragState.active())
    {
        return false;
    }

    Window* draggedWindow = m_dragState.window;
    if (draggedWindow == nullptr)
    {
        return false;
    }

    const UI::DockDragPreviewState& previewState = m_dockPreview.state();
    const UI::DockSnapZone& zone = previewState.activeZone;

    if (!previewState.active || !zone.isValid())
    {
        return false;
    }

    if (zone.targetContentId.empty())
    {
        return false;
    }

    const std::string draggedContentId = dockContentIdForWindow(*draggedWindow);

    if (draggedContentId.empty() || draggedContentId == zone.targetContentId)
    {
        return false;
    }

    Window* targetWindow = findDockWindowByContentId(zone.targetContentId);
    if (targetWindow == nullptr || targetWindow == draggedWindow)
    {
        return false;
    }

    if (zone.type == UI::DockSnapZoneType::Center)
    {
        return tryApplyCenterDockWindowTabs(*draggedWindow, *targetWindow);
    }

    if (!isSideDockZone(zone.type))
    {
        return false;
    }

    if (m_dockTree == nullptr)
    {
        return false;
    }

    if (!buildSideDockTreeForWindows(
        *draggedWindow,
        *targetWindow,
        zone.type))
    {
        return false;
    }

    applyDockTreeLayoutToWindows();

    bringToFront(*draggedWindow);

    return true;
}

bool WindowManager::tryApplyCenterDockWindowTabs(
    Window& draggedWindow,
    Window& targetWindow)
{
    if (&draggedWindow == &targetWindow)
    {
        return false;
    }

    UI::TabbedWindow* draggedTabbedWindow =
        dynamic_cast<UI::TabbedWindow*>(&draggedWindow);
    UI::TabbedWindow* targetTabbedWindow =
        dynamic_cast<UI::TabbedWindow*>(&targetWindow);

    if (draggedTabbedWindow == nullptr && !draggedWindow.hasTransferableContent())
    {
        return false;
    }

    if (targetTabbedWindow == nullptr && !targetWindow.hasTransferableContent())
    {
        return false;
    }

    const Rect targetBounds = targetWindow.bounds();

    if (targetTabbedWindow != nullptr)
    {
        bool transferred = false;

        if (draggedTabbedWindow != nullptr)
        {
            transferred = transferPagesFromTabbedWindow(
                *draggedTabbedWindow,
                *targetTabbedWindow,
                true);
        }
        else
        {
            transferred = transferStandaloneWindowToTabbedWindow(
                draggedWindow,
                *targetTabbedWindow,
                true);
        }

        if (!transferred)
        {
            return false;
        }

        targetTabbedWindow->setBounds(targetBounds);
        removeWindow(draggedWindow);
        bringToFront(*targetTabbedWindow);
        setFocusedWindow(targetTabbedWindow);
        setHoveredWindow(targetTabbedWindow);
        return true;
    }

    if (draggedTabbedWindow != nullptr)
    {
        if (!transferStandaloneWindowToTabbedWindow(
            targetWindow,
            *draggedTabbedWindow,
            false))
        {
            return false;
        }

        draggedTabbedWindow->setBounds(targetBounds);
        removeWindow(targetWindow);
        bringToFront(*draggedTabbedWindow);
        setFocusedWindow(draggedTabbedWindow);
        setHoveredWindow(draggedTabbedWindow);
        return true;
    }

    UI::TabbedWindow* newTabbedWindow = createOwnedTabbedWindow(
        targetBounds,
        tabTitleForWindow(targetWindow));

    if (newTabbedWindow == nullptr)
    {
        return false;
    }

    if (!transferStandaloneWindowToTabbedWindow(
        targetWindow,
        *newTabbedWindow,
        false))
    {
        removeWindow(*newTabbedWindow);
        return false;
    }

    if (!transferStandaloneWindowToTabbedWindow(
        draggedWindow,
        *newTabbedWindow,
        true))
    {
        removeWindow(*newTabbedWindow);
        return false;
    }

    removeWindow(targetWindow);
    removeWindow(draggedWindow);
    addWindow(*newTabbedWindow);
    bringToFront(*newTabbedWindow);
    setFocusedWindow(newTabbedWindow);
    setHoveredWindow(newTabbedWindow);
    return true;
}

bool WindowManager::transferStandaloneWindowToTabbedWindow(
    Window& sourceWindow,
    UI::TabbedWindow& tabbedWindow,
    bool selectNewPage)
{
    const std::string contentId = dockContentIdForWindow(sourceWindow);

    if (contentId.empty() || tabbedWindow.model().containsContentId(contentId))
    {
        return false;
    }

    UI::TabbedWindowPage page = makeTabPageFromWindow(sourceWindow);

    if (!page.isValid())
    {
        return false;
    }

    return tabbedWindow.addPage(std::move(page), selectNewPage);
}

bool WindowManager::transferPagesFromTabbedWindow(
    UI::TabbedWindow& sourceWindow,
    UI::TabbedWindow& targetWindow,
    bool selectTransferredPages)
{
    if (&sourceWindow == &targetWindow || sourceWindow.empty())
    {
        return false;
    }

    for (const UI::TabbedWindowPage& page : sourceWindow.model().pages())
    {
        if (targetWindow.model().containsContentId(page.contentId()))
        {
            return false;
        }
    }

    bool transferredAny = false;

    while (!sourceWindow.empty())
    {
        UI::TabbedWindowPage page = sourceWindow.removePageAt(0);

        if (!page.isValid())
        {
            continue;
        }

        const bool added = targetWindow.addPage(
            std::move(page),
            selectTransferredPages);

        if (!added)
        {
            return transferredAny;
        }

        transferredAny = true;
    }

    return transferredAny;
}

UI::TabbedWindow* WindowManager::createOwnedTabbedWindow(
    const Rect& bounds,
    const std::string& title)
{
    auto tabbedWindow = std::make_unique<UI::TabbedWindow>(bounds, title);
    UI::TabbedWindow* rawWindow = tabbedWindow.get();

    rawWindow->setMinimumSize(8, 4);
    m_ownedTabbedWindows.push_back(std::move(tabbedWindow));

    return rawWindow;
}

UI::TabbedWindowPage WindowManager::makeTabPageFromWindow(Window& window)
{
    if (!window.hasTransferableContent())
    {
        return UI::TabbedWindowPage{};
    }

    const std::string contentId = dockContentIdForWindow(window);
    const std::string title = tabTitleForWindow(window);
    UI::TabbedWindowPageMetadata metadata = makeTabPageMetadataFromWindow(window);
    std::unique_ptr<UI::IWindowContent> content = window.releaseContent();

    if (content == nullptr)
    {
        return UI::TabbedWindowPage{};
    }

    return UI::TabbedWindowPage(
        contentId,
        title,
        std::move(content),
        metadata);
}

UI::TabbedWindowPageMetadata WindowManager::makeTabPageMetadataFromWindow(
    const Window& window) const
{
    UI::TabbedWindowPageMetadata metadata{};

    metadata.lastStandaloneBounds = window.bounds();
    metadata.hasStandaloneBounds = true;
    metadata.modal = window.isModal();
    metadata.draggable = window.isDraggable();
    metadata.resizable = window.isResizable();
    metadata.enabled = window.isEnabled();
    metadata.visible = window.isVisible();
    metadata.minimumSize = window.minimumSize();
    metadata.resizeBorderThickness = window.resizeBorderThickness();
    metadata.titleBarHeight = window.titleBarHeight();
    metadata.backgroundStyle = window.backgroundStyle();
    metadata.borderStyle = window.borderStyle();
    metadata.titleStyle = window.titleStyle();
    metadata.hoveredBorderStyle = window.hoveredBorderStyle();
    metadata.hoveredTitleStyle = window.hoveredTitleStyle();
    metadata.borderGlyphs = window.borderGlyphs();

    if (dynamic_cast<const UI::TabbedWindow*>(&window) != nullptr)
    {
        metadata.sourceWindowType = "TabbedWindow";
    }
    else
    {
        metadata.sourceWindowType = "Window";
    }

    return metadata;
}

std::unique_ptr<Window> WindowManager::makeStandaloneWindowFromTabPage(
    UI::TabbedWindowPage page,
    const Rect& bounds)
{
    if (!page.isValid())
    {
        return nullptr;
    }

    const UI::TabbedWindowPageMetadata metadata = page.metadata();
    std::unique_ptr<UI::IWindowContent> content = page.releaseContent();

    if (content == nullptr)
    {
        return nullptr;
    }

    std::string title = page.title();
    if (title.empty())
    {
        title = page.contentId();
    }

    std::unique_ptr<Window> window = std::make_unique<UI::ContentWindow>(
        bounds,
        std::move(title),
        std::move(content));

    applyMetadataToWindow(*window, metadata);
    window->setBounds(bounds);

    return window;
}

Window* WindowManager::collapseOneTabTabbedWindowToStandalone(
    UI::TabbedWindow& sourceWindow)
{
    if (!contains(sourceWindow) || sourceWindow.tabCount() != 1)
    {
        return nullptr;
    }

    const Rect replacementBounds = sourceWindow.bounds();
    const int sourceZOrder = zOrderOf(sourceWindow);
    const bool sourceWasFocused = focusedWindow() == &sourceWindow;
    const bool sourceWasHovered = hoveredWindow() == &sourceWindow;

    UI::TabbedWindowPage remainingPage = sourceWindow.removePageAt(0);

    if (!remainingPage.isValid())
    {
        return nullptr;
    }

    std::unique_ptr<Window> replacementWindow = makeStandaloneWindowFromTabPage(
        std::move(remainingPage),
        replacementBounds);

    if (replacementWindow == nullptr)
    {
        return nullptr;
    }

    Window* rawReplacementWindow = replacementWindow.get();

    m_ownedWindows.push_back(std::move(replacementWindow));
    addWindow(*rawReplacementWindow);
    setZOrder(*rawReplacementWindow, sourceZOrder);
    show(*rawReplacementWindow);

    removeWindow(sourceWindow);

    if (sourceWasFocused)
    {
        setFocusedWindow(rawReplacementWindow);
    }

    if (sourceWasHovered)
    {
        setHoveredWindow(rawReplacementWindow);
    }

    return rawReplacementWindow;
}

void WindowManager::applyMetadataToWindow(
    Window& window,
    const UI::TabbedWindowPageMetadata& metadata)
{
    window.setModal(metadata.modal);
    window.setDraggable(metadata.draggable);
    window.setResizable(metadata.resizable);
    window.setEnabled(metadata.enabled);

    if (metadata.visible)
    {
        window.show();
    }
    else
    {
        window.hide();
    }

    window.setMinimumSize(metadata.minimumSize);
    window.setResizeBorderThickness(metadata.resizeBorderThickness);
    window.setTitleBarHeight(metadata.titleBarHeight);

    window.setBackgroundStyle(metadata.backgroundStyle);
    window.setBorderStyle(metadata.borderStyle);
    window.setTitleStyle(metadata.titleStyle);
    window.setHoveredBorderStyle(metadata.hoveredBorderStyle);
    window.setHoveredTitleStyle(metadata.hoveredTitleStyle);
    window.setBorderGlyphs(metadata.borderGlyphs);
}

std::string WindowManager::tabTitleForWindow(const Window& window) const
{
    if (window.hasTitle())
    {
        return window.title();
    }

    return dockContentIdForWindow(window);
}

bool WindowManager::isSideDockZone(UI::DockSnapZoneType type) const
{
    return type == UI::DockSnapZoneType::Top
        || type == UI::DockSnapZoneType::Bottom
        || type == UI::DockSnapZoneType::Left
        || type == UI::DockSnapZoneType::Right;
}

Window* WindowManager::findDockWindowByContentId(const std::string& contentId)
{
    for (ManagedWindow& entry : m_windows)
    {
        if (entry.window == nullptr)
        {
            continue;
        }

        if (dockContentIdForWindow(*entry.window) == contentId)
        {
            return entry.window;
        }
    }

    return nullptr;
}

const Window* WindowManager::findDockWindowByContentId(
    const std::string& contentId) const
{
    for (const ManagedWindow& entry : m_windows)
    {
        if (entry.window == nullptr)
        {
            continue;
        }

        if (dockContentIdForWindow(*entry.window) == contentId)
        {
            return entry.window;
        }
    }

    return nullptr;
}

bool WindowManager::buildSideDockTreeForWindows(
    const Window& draggedWindow,
    const Window& targetWindow,
    UI::DockSnapZoneType type)
{
    if (m_dockTree == nullptr)
    {
        return false;
    }

    if (&draggedWindow == &targetWindow)
    {
        return false;
    }

    if (!isSideDockZone(type))
    {
        return false;
    }

    const std::string draggedContentId = dockContentIdForWindow(draggedWindow);
    const std::string targetContentId = dockContentIdForWindow(targetWindow);

    if (draggedContentId.empty() ||
        targetContentId.empty() ||
        draggedContentId == targetContentId)
    {
        return false;
    }

    const Rect targetBounds = targetWindow.bounds();
    if (targetBounds.size.width <= 0 || targetBounds.size.height <= 0)
    {
        return false;
    }

    UI::DockContentDescriptor targetContent;
    targetContent.contentId = targetContentId;
    targetContent.title = tabTitleForWindow(targetWindow);

    UI::DockContentDescriptor draggedContent;
    draggedContent.contentId = draggedContentId;
    draggedContent.title = tabTitleForWindow(draggedWindow);

    const UI::DockSplitOrientation orientation =
        (type == UI::DockSnapZoneType::Left ||
            type == UI::DockSnapZoneType::Right)
        ? UI::DockSplitOrientation::Horizontal
        : UI::DockSplitOrientation::Vertical;

    const bool draggedContentInFirstChild =
        type == UI::DockSnapZoneType::Left ||
        type == UI::DockSnapZoneType::Top;

    m_dockTree->clear();
    m_dockTree->setBounds(targetBounds);

    const int rootNodeId = m_dockTree->attachRoot(std::move(targetContent));

    return m_dockTree->splitNode(
        rootNodeId,
        orientation,
        0.5f,
        std::move(draggedContent),
        draggedContentInFirstChild);
}

void WindowManager::applyDockTreeLayoutToWindows()
{
    if (m_dockTree == nullptr)
    {
        return;
    }

    const std::vector<UI::DockLayoutRecord> records =
        m_dockTree->createLayoutRecords();

    for (const UI::DockLayoutRecord& record : records)
    {
        if (record.kind != UI::DockNodeKind::Leaf ||
            record.contentId.empty() ||
            record.bounds.size.width <= 0 ||
            record.bounds.size.height <= 0)
        {
            continue;
        }

        Window* window = findDockWindowByContentId(record.contentId);
        if (window == nullptr)
        {
            continue;
        }

        window->setBounds(record.bounds);
    }
}

void WindowManager::applySideDockWindowBounds(
    Window& draggedWindow,
    Window& targetWindow,
    UI::DockSnapZoneType type)
{
    const Rect targetBounds = targetWindow.bounds();

    if (targetBounds.size.width <= 0 || targetBounds.size.height <= 0)
    {
        return;
    }

    Rect draggedBounds = targetBounds;
    Rect remainingBounds = targetBounds;

    switch (type)
    {
    case UI::DockSnapZoneType::Top:
    {
        const int draggedHeight = std::max(1, targetBounds.size.height / 2);
        const int remainingHeight = std::max(1, targetBounds.size.height - draggedHeight);

        draggedBounds = Rect{
            targetBounds.position,
            Size{ targetBounds.size.width, draggedHeight }
        };

        remainingBounds = Rect{
            Point{
                targetBounds.position.x,
                targetBounds.position.y + draggedHeight
            },
            Size{ targetBounds.size.width, remainingHeight }
        };

        break;
    }

    case UI::DockSnapZoneType::Bottom:
    {
        const int targetHeight = std::max(1, targetBounds.size.height / 2);
        const int draggedHeight = std::max(1, targetBounds.size.height - targetHeight);

        remainingBounds = Rect{
            targetBounds.position,
            Size{ targetBounds.size.width, targetHeight }
        };

        draggedBounds = Rect{
            Point{
                targetBounds.position.x,
                targetBounds.position.y + targetHeight
            },
            Size{ targetBounds.size.width, draggedHeight }
        };

        break;
    }

    case UI::DockSnapZoneType::Left:
    {
        const int draggedWidth = std::max(1, targetBounds.size.width / 2);
        const int remainingWidth = std::max(1, targetBounds.size.width - draggedWidth);

        draggedBounds = Rect{
            targetBounds.position,
            Size{ draggedWidth, targetBounds.size.height }
        };

        remainingBounds = Rect{
            Point{
                targetBounds.position.x + draggedWidth,
                targetBounds.position.y
            },
            Size{ remainingWidth, targetBounds.size.height }
        };

        break;
    }

    case UI::DockSnapZoneType::Right:
    {
        const int targetWidth = std::max(1, targetBounds.size.width / 2);
        const int draggedWidth = std::max(1, targetBounds.size.width - targetWidth);

        remainingBounds = Rect{
            targetBounds.position,
            Size{ targetWidth, targetBounds.size.height }
        };

        draggedBounds = Rect{
            Point{
                targetBounds.position.x + targetWidth,
                targetBounds.position.y
            },
            Size{ draggedWidth, targetBounds.size.height }
        };

        break;
    }

    default:
        return;
    }

    targetWindow.setBounds(remainingBounds);
    draggedWindow.setBounds(draggedBounds);
}