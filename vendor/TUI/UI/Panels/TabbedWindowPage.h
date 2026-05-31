#pragma once

#include <memory>
#include <string>

#include "Core/Rect.h"
#include "Core/Size.h"
#include "Rendering/Objects/TextObjectFactory.h"
#include "Rendering/Styles/Style.h"
#include "UI/Content/IWindowContent.h"

namespace UI
{
    struct TabbedWindowPageMetadata
    {
        Rect lastStandaloneBounds{};
        bool hasStandaloneBounds = false;

        bool modal = false;
        bool draggable = true;
        bool resizable = true;
        bool enabled = true;
        bool visible = true;

        Size minimumSize{ 4, 3 };
        int resizeBorderThickness = 1;
        int titleBarHeight = 1;

        Style backgroundStyle;
        Style borderStyle;
        Style titleStyle;
        Style hoveredBorderStyle;
        Style hoveredTitleStyle;

        ObjectFactory::BorderGlyphs borderGlyphs{};

        std::string sourceWindowType;
    };

    class TabbedWindowPage
    {
    public:
        TabbedWindowPage();
        TabbedWindowPage(
            std::string contentId,
            std::string title,
            std::unique_ptr<IWindowContent> content);

        TabbedWindowPage(
            std::string contentId,
            std::string title,
            std::unique_ptr<IWindowContent> content,
            TabbedWindowPageMetadata metadata);

        TabbedWindowPage(const TabbedWindowPage&) = delete;
        TabbedWindowPage& operator=(const TabbedWindowPage&) = delete;

        TabbedWindowPage(TabbedWindowPage&&) noexcept;
        TabbedWindowPage& operator=(TabbedWindowPage&&) noexcept;

        ~TabbedWindowPage();

        const std::string& contentId() const;
        void setContentId(std::string contentId);

        const std::string& title() const;
        void setTitle(std::string title);

        const Style& normalTabStyle() const;
        void setNormalTabStyle(const Style& style);

        const Style& hoveredTabStyle() const;
        void setHoveredTabStyle(const Style& style);

        const Style& selectedTabStyle() const;
        void setSelectedTabStyle(const Style& style);

        const TabbedWindowPageMetadata& metadata() const;
        TabbedWindowPageMetadata& metadata();
        void setMetadata(const TabbedWindowPageMetadata& metadata);

        bool isValid() const;
        bool hasContent() const;

        IWindowContent* content();
        const IWindowContent* content() const;

        void setContent(std::unique_ptr<IWindowContent> content);
        std::unique_ptr<IWindowContent> releaseContent();

    private:
        std::string m_contentId;
        std::string m_title;

        Style m_normalTabStyle;
        Style m_hoveredTabStyle;
        Style m_selectedTabStyle;

        TabbedWindowPageMetadata m_metadata{};

        std::unique_ptr<IWindowContent> m_content;
    };
}