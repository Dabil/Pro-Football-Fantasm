#include "UI/Content/WidgetWindowContent.h"

#include <utility>

#include "UI/Widgets/Widget.h"

namespace UI
{
    WidgetWindowContent::WidgetWindowContent(std::unique_ptr<Widget> widget)
        : m_widget(std::move(widget))
    {
    }

    WidgetWindowContent::~WidgetWindowContent() = default;

    std::string_view WidgetWindowContent::contentTypeName() const
    {
        return "UI::WidgetWindowContent";
    }

    bool WidgetWindowContent::hasWidget() const
    {
        return m_widget != nullptr;
    }

    Widget* WidgetWindowContent::widget()
    {
        return m_widget.get();
    }

    const Widget* WidgetWindowContent::widget() const
    {
        return m_widget.get();
    }

    std::unique_ptr<Widget> WidgetWindowContent::releaseWidget()
    {
        return std::move(m_widget);
    }

    void WidgetWindowContent::update(const Animation::TickEvent& event)
    {
        if (m_widget == nullptr)
        {
            return;
        }

        m_widget->update(event);
    }

    bool WidgetWindowContent::handleEvent(const Input::Event& event)
    {
        if (m_widget == nullptr)
        {
            return false;
        }

        return m_widget->handleEvent(event);
    }

    void WidgetWindowContent::draw(Surface& surface, const Rect& bounds)
    {
        if (m_widget == nullptr)
        {
            return;
        }

        m_widget->setBounds(bounds);
        m_widget->draw(surface);
    }

    void WidgetWindowContent::onBoundsChanged(const Rect& bounds)
    {
        if (m_widget == nullptr)
        {
            return;
        }

        m_widget->setBounds(bounds);
    }
}