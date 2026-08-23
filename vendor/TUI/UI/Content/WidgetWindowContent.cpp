#include "UI/Content/WidgetWindowContent.h"

#include <algorithm>
#include <utility>

#include "UI/Widgets/Widget.h"

namespace
{
    Size safeSize(Size size)
    {
        return Size{
            std::max(0, size.width),
            std::max(0, size.height)
        };
    }
}

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

    Size WidgetWindowContent::preferredContentSize(const Rect& availableBounds) const
    {
        if (m_hasPreferredContentSize)
        {
            return safeSize(m_preferredContentSize);
        }

        return availableBounds.size;
    }

    void WidgetWindowContent::setPreferredContentSize(Size size)
    {
        m_preferredContentSize = safeSize(size);
        m_hasPreferredContentSize = true;
    }

    void WidgetWindowContent::setPreferredContentSize(int width, int height)
    {
        setPreferredContentSize(Size{ width, height });
    }

    void WidgetWindowContent::clearPreferredContentSize()
    {
        m_hasPreferredContentSize = false;
        m_preferredContentSize = Size{ 0, 0 };
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