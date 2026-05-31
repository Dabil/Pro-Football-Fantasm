#include "UI/Content/ImmediateModeWindowContent.h"

#include <utility>

#include "Input/Event.h"
#include "Rendering/Surface.h"

namespace UI
{
    ImmediateModeWindowContent::ImmediateModeWindowContent(DrawCallback draw)
        : m_draw(std::move(draw))
    {
    }

    ImmediateModeWindowContent::ImmediateModeWindowContent(
        DrawCallback draw,
        UpdateCallback update,
        EventCallback handleEvent,
        BoundsChangedCallback onBoundsChanged)
        : m_draw(std::move(draw))
        , m_update(std::move(update))
        , m_handleEvent(std::move(handleEvent))
        , m_onBoundsChanged(std::move(onBoundsChanged))
    {
    }

    ImmediateModeWindowContent::~ImmediateModeWindowContent() = default;

    std::string_view ImmediateModeWindowContent::contentTypeName() const
    {
        return "UI::ImmediateModeWindowContent";
    }

    void ImmediateModeWindowContent::setDrawCallback(DrawCallback draw)
    {
        m_draw = std::move(draw);
    }

    void ImmediateModeWindowContent::setUpdateCallback(UpdateCallback update)
    {
        m_update = std::move(update);
    }

    void ImmediateModeWindowContent::setEventCallback(EventCallback handleEvent)
    {
        m_handleEvent = std::move(handleEvent);
    }

    void ImmediateModeWindowContent::setBoundsChangedCallback(BoundsChangedCallback onBoundsChanged)
    {
        m_onBoundsChanged = std::move(onBoundsChanged);
    }

    void ImmediateModeWindowContent::update(const Animation::TickEvent& event)
    {
        if (m_update)
        {
            m_update(event);
        }
    }

    bool ImmediateModeWindowContent::handleEvent(const Input::Event& event)
    {
        if (!m_handleEvent)
        {
            return false;
        }

        return m_handleEvent(event);
    }

    void ImmediateModeWindowContent::draw(Surface& surface, const Rect& bounds)
    {
        if (m_draw)
        {
            m_draw(surface, bounds);
        }
    }

    void ImmediateModeWindowContent::onBoundsChanged(const Rect& bounds)
    {
        if (m_onBoundsChanged)
        {
            m_onBoundsChanged(bounds);
        }
    }
}