#include "UI/Panels/ContentWindow.h"

#include <utility>

#include "Input/Event.h"
#include "Rendering/Surface.h"

namespace
{
    bool areSameBounds(const Rect& lhs, const Rect& rhs)
    {
        return lhs.position.x == rhs.position.x &&
               lhs.position.y == rhs.position.y &&
               lhs.size.width == rhs.size.width &&
               lhs.size.height == rhs.size.height;
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
        if (m_content != nullptr)
        {
            m_content->onAttached();
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

        notifyContentBoundsChanged();

        if (m_content != nullptr)
        {
            m_content->update(event);
        }
    }

    bool ContentWindow::handleEvent(const Input::Event& event)
    {
        if (m_content != nullptr && m_content->handleEvent(event))
        {
            return true;
        }

        return Window::handleEvent(event);
    }

    void ContentWindow::draw(Surface& surface)
    {
        if (!isVisible())
        {
            return;
        }

        notifyContentBoundsChanged();

        Window::draw(surface);

        if (m_content != nullptr)
        {
            m_content->draw(surface, contentBounds());
        }
    }

    void ContentWindow::detachContent()
    {
        if (m_content != nullptr)
        {
            m_content->onDetached();
        }
    }

    void ContentWindow::notifyContentBoundsChanged()
    {
        if (m_content == nullptr)
        {
            return;
        }

        const Rect currentBounds = contentBounds();

        if (m_hasLastContentBounds && areSameBounds(currentBounds, m_lastContentBounds))
        {
            return;
        }

        m_lastContentBounds = currentBounds;
        m_hasLastContentBounds = true;

        m_content->onBoundsChanged(currentBounds);
    }
}