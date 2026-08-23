#pragma once

#include <memory>

#include "Core/Size.h"
#include "UI/Content/IWindowContent.h"

class Widget;

namespace UI
{
    class WidgetWindowContent final : public IWindowContent
    {
    public:
        explicit WidgetWindowContent(std::unique_ptr<Widget> widget);

        WidgetWindowContent(const WidgetWindowContent&) = delete;
        WidgetWindowContent& operator=(const WidgetWindowContent&) = delete;

        WidgetWindowContent(WidgetWindowContent&&) noexcept = default;
        WidgetWindowContent& operator=(WidgetWindowContent&&) noexcept = default;

        ~WidgetWindowContent() override;

        std::string_view contentTypeName() const override;

        Size preferredContentSize(const Rect& availableBounds) const override;

        void setPreferredContentSize(Size size);
        void setPreferredContentSize(int width, int height);
        void clearPreferredContentSize();

        bool hasWidget() const;

        Widget* widget();
        const Widget* widget() const;

        std::unique_ptr<Widget> releaseWidget();

        void update(const Animation::TickEvent& event) override;
        bool handleEvent(const Input::Event& event) override;
        void draw(Surface& surface, const Rect& bounds) override;
        void onBoundsChanged(const Rect& bounds) override;

    private:
        std::unique_ptr<Widget> m_widget;
        bool m_hasPreferredContentSize = false;
        Size m_preferredContentSize{ 0, 0 };
    };
}