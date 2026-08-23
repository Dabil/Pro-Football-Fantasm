#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "UI/Base/Viewport.h"
#include "UI/Content/IWindowContent.h"
#include "UI/Panels/Window.h"
#include "UI/Scrolling/Scrollbar.h"

namespace UI
{
    class ContentWindow final : public Window
    {
    public:
        ContentWindow(const Rect& bounds, std::string title, std::unique_ptr<IWindowContent> content);
        ~ContentWindow() override;

        IWindowContent* content();
        const IWindowContent* content() const;
        std::string_view contentTypeName() const;

        Viewport& viewport();
        const Viewport& viewport() const;

        bool viewportScrollingEnabled() const;
        void setViewportScrollingEnabled(bool enabled);

        bool isVerticalScrollbarVisible() const;
        void setVerticalScrollbarVisible(bool visible);

        bool reservesVerticalScrollbarColumn() const;
        void setReserveVerticalScrollbarColumn(bool reserveColumn);

        const UI::Scrolling::VerticalScrollbarStyle& verticalScrollbarStyle() const;
        void setVerticalScrollbarStyle(const UI::Scrolling::VerticalScrollbarStyle& style);

        bool hasTransferableContent() const override;
        std::unique_ptr<IWindowContent> releaseContent() override;

        void update(const Animation::TickEvent& event) override;
        bool handleEvent(const Input::Event& event) override;
        void draw(Surface& surface) override;

    private:
        void detachContent();

        void syncViewport();
        Size preferredContentSizeForCurrentClient() const;

        Rect viewportBounds() const;
        Rect scrollbarBounds() const;

        bool shouldDrawVerticalScrollbar() const;
        void drawScrollbarIfNeeded(Surface& surface);

        Input::Event translateEventForContent(const Input::Event& event) const;

        void notifyContentBoundsChanged();
        void blitViewportSurface(
            Surface& target,
            const Surface& source,
            const Rect& targetRect) const;

    private:
        std::unique_ptr<IWindowContent> m_content;

        Viewport m_viewport;

        Rect m_lastContentBounds{};
        bool m_hasLastContentBounds = false;

        bool m_viewportScrollingEnabled = true;
        bool m_verticalScrollbarVisible = true;
        bool m_reserveVerticalScrollbarColumn = true;

        UI::Scrolling::VerticalScrollbarStyle m_verticalScrollbarStyle;
    };
}