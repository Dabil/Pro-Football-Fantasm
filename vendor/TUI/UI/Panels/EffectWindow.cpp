#include "UI/Panels/EffectWindow.h"

#include <utility>

#include "Rendering/Surface.h"
#include "UI/Content/EffectReferenceWindowContent.h"

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

EffectWindow::EffectWindow(Rect bounds, std::string title, IEffect& effect)
    : Window(bounds, std::move(title))
    , m_content(std::make_unique<UI::EffectReferenceWindowContent>(effect))
{
    m_content->onAttached();
    notifyContentBoundsChanged();
}

EffectWindow::~EffectWindow()
{
    detachContent();
}

bool EffectWindow::hasTransferableContent() const
{
    return m_content != nullptr;
}

std::unique_ptr<UI::IWindowContent> EffectWindow::releaseContent()
{
    detachContent();

    m_hasLastContentBounds = false;
    return std::move(m_content);
}

void EffectWindow::update(const Animation::TickEvent& event)
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

void EffectWindow::draw(Surface& surface)
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

void EffectWindow::detachContent()
{
    if (m_content != nullptr)
    {
        m_content->onDetached();
    }
}

void EffectWindow::notifyContentBoundsChanged()
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