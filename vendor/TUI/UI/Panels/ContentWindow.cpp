#include "UI/Panels/ContentWindow.h"

#include <algorithm>
#include <utility>

#include "Input/Event.h"
#include "Input/MouseEvent.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/UIThemes.h"
#include "Rendering/Surface.h"
#include "UI/Scrolling/ScrollBehavior.h"

namespace
{
    bool areSameBounds(const Rect& lhs, const Rect& rhs)
    {
        return lhs.position.x == rhs.position.x &&
            lhs.position.y == rhs.position.y &&
            lhs.size.width == rhs.size.width &&
            lhs.size.height == rhs.size.height;
    }

    Size safeSize(Size size)
    {
        return Size{
            std::max(0, size.width),
            std::max(0, size.height)
        };
    }

    Rect localRectFromSize(Size size)
    {
        return Rect{
            Point{ 0, 0 },
            safeSize(size)
        };
    }
}

namespace UI
{
    ContentWindow::ContentWindow(
        const Rect& bounds,
        std::string title,
        std::unique_ptr<IWindowContent> content)
        : Window(bounds, std::move(title))
        , m_content(std::move(content))
    {
        m_verticalScrollbarStyle.trackStyle = UIThemes::Border;
        m_verticalScrollbarStyle.thumbStyle = UIThemes::Selection;

        if (m_content != nullptr)
        {
            m_content->onAttached();
            syncViewport();
            notifyContentBoundsChanged();
        }
    }

    ContentWindow::~ContentWindow()
    {
        detachContent();
    }

    IWindowContent* ContentWindow::content()
    {
        return m_content.get();
    }

    const IWindowContent* ContentWindow::content() const
    {
        return m_content.get();
    }

    std::string_view ContentWindow::contentTypeName() const
    {
        return m_content != nullptr
            ? m_content->contentTypeName()
            : std::string_view("UI::ContentWindow(empty)");
    }

    Viewport& ContentWindow::viewport()
    {
        return m_viewport;
    }

    const Viewport& ContentWindow::viewport() const
    {
        return m_viewport;
    }

    bool ContentWindow::viewportScrollingEnabled() const
    {
        return m_viewportScrollingEnabled;
    }

    void ContentWindow::setViewportScrollingEnabled(bool enabled)
    {
        m_viewportScrollingEnabled = enabled;
        syncViewport();
    }

    bool ContentWindow::isVerticalScrollbarVisible() const
    {
        return m_verticalScrollbarVisible;
    }

    void ContentWindow::setVerticalScrollbarVisible(bool visible)
    {
        m_verticalScrollbarVisible = visible;
        syncViewport();
    }

    bool ContentWindow::reservesVerticalScrollbarColumn() const
    {
        return m_reserveVerticalScrollbarColumn;
    }

    void ContentWindow::setReserveVerticalScrollbarColumn(bool reserveColumn)
    {
        m_reserveVerticalScrollbarColumn = reserveColumn;
        syncViewport();
    }

    const UI::Scrolling::VerticalScrollbarStyle& ContentWindow::verticalScrollbarStyle() const
    {
        return m_verticalScrollbarStyle;
    }

    void ContentWindow::setVerticalScrollbarStyle(const UI::Scrolling::VerticalScrollbarStyle& style)
    {
        m_verticalScrollbarStyle = style;
    }

    bool ContentWindow::hasTransferableContent() const
    {
        return m_content != nullptr;
    }

    std::unique_ptr<IWindowContent> ContentWindow::releaseContent()
    {
        detachContent();

        m_hasLastContentBounds = false;
        return std::move(m_content);
    }

    void ContentWindow::update(const Animation::TickEvent& event)
    {
        if (!isVisible() || !isEnabled())
        {
            return;
        }

        syncViewport();
        notifyContentBoundsChanged();

        if (m_content != nullptr)
        {
            m_content->update(event);
        }
    }

    bool ContentWindow::handleEvent(const Input::Event& event)
    {
        if (!isVisible() || !isEnabled())
        {
            return false;
        }

        syncViewport();

        if (m_viewportScrollingEnabled &&
            UI::Scrolling::handleScrollEvent(
                event,
                m_viewport,
                contentBounds(),
                isFocused(),
                UI::Scrolling::ScrollInputOptions{
                    true,
                    true,
                    1,
                    std::max(1, viewportBounds().size.height)
                }))
        {
            return true;
        }

        if (m_content != nullptr)
        {
            const Input::Event translatedEvent = translateEventForContent(event);

            if (m_content->handleEvent(translatedEvent))
            {
                return true;
            }
        }

        return Window::handleEvent(event);
    }

    void ContentWindow::draw(Surface& surface)
    {
        if (!isVisible())
        {
            return;
        }

        syncViewport();
        notifyContentBoundsChanged();

        Window::draw(surface);

        if (m_content == nullptr)
        {
            drawScrollbarIfNeeded(surface);
            return;
        }

        const Size contentSize = safeSize(m_viewport.contentSize());
        if (contentSize.width <= 0 || contentSize.height <= 0)
        {
            drawScrollbarIfNeeded(surface);
            return;
        }

        Surface contentSurface(contentSize.width, contentSize.height);
        contentSurface.clear(Style());

        m_content->draw(contentSurface, localRectFromSize(contentSize));

        blitViewportSurface(
            surface,
            contentSurface,
            viewportBounds());

        drawScrollbarIfNeeded(surface);
    }

    void ContentWindow::detachContent()
    {
        if (m_content != nullptr)
        {
            m_content->onDetached();
        }
    }

    void ContentWindow::syncViewport()
    {
        const Rect clientBounds = contentBounds();
        const Size preferredSize = preferredContentSizeForCurrentClient();

        if (!m_viewportScrollingEnabled)
        {
            m_viewport.setContentSize(clientBounds.size);
            m_viewport.setViewSize(clientBounds.size);
            m_viewport.scrollTo(0, 0);
            return;
        }

        m_viewport.setContentSize(preferredSize);
        m_viewport.setViewSize(clientBounds.size);

        const bool needsVerticalScrollbar =
            m_verticalScrollbarVisible &&
            UI::Scrolling::shouldShowVerticalScrollbar(m_viewport);

        const Rect viewBounds =
            UI::Scrolling::viewportBoundsForContentBounds(
                clientBounds,
                needsVerticalScrollbar,
                m_reserveVerticalScrollbarColumn);

        m_viewport.setViewSize(viewBounds.size);
    }

    Size ContentWindow::preferredContentSizeForCurrentClient() const
    {
        const Rect clientBounds = contentBounds();

        if (m_content == nullptr)
        {
            return safeSize(clientBounds.size);
        }

        Size preferredSize = safeSize(m_content->preferredContentSize(clientBounds));

        preferredSize.width = std::max(preferredSize.width, clientBounds.size.width);
        preferredSize.height = std::max(preferredSize.height, clientBounds.size.height);

        return preferredSize;
    }

    Rect ContentWindow::viewportBounds() const
    {
        const Rect clientBounds = contentBounds();

        return UI::Scrolling::viewportBoundsForContentBounds(
            clientBounds,
            shouldDrawVerticalScrollbar(),
            m_reserveVerticalScrollbarColumn);
    }

    Rect ContentWindow::scrollbarBounds() const
    {
        const Rect clientBounds = contentBounds();

        return UI::Scrolling::verticalScrollbarBoundsForContentBounds(
            clientBounds,
            shouldDrawVerticalScrollbar() && m_reserveVerticalScrollbarColumn);
    }

    bool ContentWindow::shouldDrawVerticalScrollbar() const
    {
        return m_viewportScrollingEnabled
            && m_verticalScrollbarVisible
            && UI::Scrolling::shouldShowVerticalScrollbar(m_viewport);
    }

    void ContentWindow::drawScrollbarIfNeeded(Surface& surface)
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

    Input::Event ContentWindow::translateEventForContent(const Input::Event& event) const
    {
        const Input::MouseEvent* mouseEvent = event.asMouse();
        if (mouseEvent == nullptr)
        {
            return event;
        }

        Input::MouseEvent translatedMouseEvent = *mouseEvent;

        const Rect viewBounds = viewportBounds();

        translatedMouseEvent.position = Point{
            mouseEvent->position.x - viewBounds.position.x + m_viewport.scrollX(),
            mouseEvent->position.y - viewBounds.position.y + m_viewport.scrollY()
        };

        return Input::Event(translatedMouseEvent);
    }

    void ContentWindow::notifyContentBoundsChanged()
    {
        if (m_content == nullptr)
        {
            return;
        }

        const Rect currentBounds = localRectFromSize(m_viewport.contentSize());

        if (m_hasLastContentBounds && areSameBounds(currentBounds, m_lastContentBounds))
        {
            return;
        }

        m_lastContentBounds = currentBounds;
        m_hasLastContentBounds = true;

        m_content->onBoundsChanged(currentBounds);
    }

    void ContentWindow::blitViewportSurface(
        Surface& target,
        const Surface& source,
        const Rect& targetRect) const
    {
        const ScreenBuffer& sourceBuffer = source.buffer();
        ScreenBuffer& targetBuffer = target.buffer();

        const Point scroll = m_viewport.scrollOffset();

        for (int y = 0; y < targetRect.size.height; ++y)
        {
            const int sourceY = scroll.y + y;
            const int targetY = targetRect.position.y + y;

            if (sourceY < 0 || sourceY >= sourceBuffer.getHeight())
            {
                continue;
            }

            for (int x = 0; x < targetRect.size.width; ++x)
            {
                const int sourceX = scroll.x + x;
                const int targetX = targetRect.position.x + x;

                if (sourceX < 0 || sourceX >= sourceBuffer.getWidth())
                {
                    continue;
                }

                if (!targetBuffer.inBounds(targetX, targetY))
                {
                    continue;
                }

                const ScreenCell& cell = sourceBuffer.getCell(sourceX, sourceY);

                if (cell.isEmpty())
                {
                    continue;
                }

                targetBuffer.setCell(targetX, targetY, cell);
            }
        }
    }
}