#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Input/Event.h"
#include "UI/Panels/TabbedWindowModel.h"
#include "UI/Panels/TabbedWindowPage.h"
#include "UI/Panels/Window.h"

class Surface;

namespace UI
{
    class TabbedWindow : public Window
    {
    public:
        TabbedWindow();
        explicit TabbedWindow(const Rect& bounds);
        TabbedWindow(const Rect& bounds, std::string title);

        TabbedWindow(const TabbedWindow&) = delete;
        TabbedWindow& operator=(const TabbedWindow&) = delete;

        TabbedWindow(TabbedWindow&&) noexcept = delete;
        TabbedWindow& operator=(TabbedWindow&&) noexcept = delete;

        ~TabbedWindow() override;

        TabbedWindowModel& model();
        const TabbedWindowModel& model() const;

        bool empty() const;
        std::size_t tabCount() const;

        bool addPage(TabbedWindowPage page, bool selectNewPage = true);

        bool addContentPage(
            std::string contentId,
            std::string title,
            std::unique_ptr<IWindowContent> content,
            bool selectNewPage = true);

        bool addContentPage(
            std::string contentId,
            std::string title,
            std::unique_ptr<IWindowContent> content,
            TabbedWindowPageMetadata metadata,
            bool selectNewPage = true);

        TabbedWindowPage removePageAt(std::size_t index);
        TabbedWindowPage removePageByContentId(const std::string& contentId);

        bool selectTab(std::size_t index);
        bool selectTabByContentId(const std::string& contentId);

        std::size_t selectedTabIndex() const;
        const std::string& selectedContentId() const;

        int hoveredTabIndex() const;
        void setHoveredTabIndex(int index);
        void clearHoveredTabIndex();

        int tabIndexAt(Point screenPosition) const;
        bool isPointInTabHeader(Point screenPosition) const;
        bool isPointInSelectedContent(Point screenPosition) const;

        TabbedWindowPage* selectedPage();
        const TabbedWindowPage* selectedPage() const;

        IWindowContent* selectedContent();
        const IWindowContent* selectedContent() const;

        Rect tabHeaderBounds() const;
        Rect selectedContentBounds() const;
        bool isPointInTabTitle(Point screenPosition) const;

        void update(const Animation::TickEvent& event) override;
        bool handleEvent(const Input::Event& event) override;
        void draw(Surface& surface) override;

    private:
        void synchronizeTitleWithSelectedPage();
        void notifyContentBoundsChanged();

        void attachPageContent(TabbedWindowPage& page);
        void detachPageContent(TabbedWindowPage& page);
        void detachAllPageContent();

        bool handleMouseEvent(const Input::MouseEvent& mouseEvent);
        bool handleTabHeaderMouseEvent(const Input::MouseEvent& mouseEvent);
        bool routeContentMouseEvent(const Input::MouseEvent& mouseEvent);
        void updateHoveredTabFromPoint(Point screenPosition);

        void drawTitleTabs(Surface& surface);
        std::string titleForAllTabs() const;
        std::string tabLabelForPage(const TabbedWindowPage& page) const;
        std::string clippedTabLabel(const std::string& label, int maxWidth) const;
        Style tabStyleForIndex(std::size_t index) const;

        void applySelectedPageChromeMetadata();
        void restoreChromeMetadata(
            const Style& originalBackgroundStyle,
            const Style& originalBorderStyle,
            const Style& originalTitleStyle,
            const Style& originalHoveredBorderStyle,
            const Style& originalHoveredTitleStyle,
            const ObjectFactory::BorderGlyphs& originalBorderGlyphs,
            Size originalMinimumSize,
            int originalResizeBorderThickness,
            int originalTitleBarHeight,
            bool originalDraggable,
            bool originalResizable);

    private:
        TabbedWindowModel m_model;
        int m_hoveredTabIndex = -1;
        Rect m_lastContentBounds{};
    };
}