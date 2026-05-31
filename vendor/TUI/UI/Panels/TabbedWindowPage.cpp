#include "UI/Panels/TabbedWindowPage.h"

#include <utility>

#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/UIThemes.h"

namespace UI
{
    TabbedWindowPage::TabbedWindowPage()
        : m_normalTabStyle(UIThemes::PanelTitle)
        , m_hoveredTabStyle(UIThemes::PanelTitle + style::Underline)
        , m_selectedTabStyle(UIThemes::Selection)
    {
    }

    TabbedWindowPage::TabbedWindowPage(
        std::string contentId,
        std::string title,
        std::unique_ptr<IWindowContent> content)
        : TabbedWindowPage()
    {
        m_contentId = std::move(contentId);
        m_title = std::move(title);
        m_content = std::move(content);
    }

    TabbedWindowPage::TabbedWindowPage(
        std::string contentId,
        std::string title,
        std::unique_ptr<IWindowContent> content,
        TabbedWindowPageMetadata metadata)
        : TabbedWindowPage()
    {
        m_contentId = std::move(contentId);
        m_title = std::move(title);
        m_content = std::move(content);
        m_metadata = std::move(metadata);
    }

    TabbedWindowPage::TabbedWindowPage(TabbedWindowPage&&) noexcept = default;

    TabbedWindowPage& TabbedWindowPage::operator=(TabbedWindowPage&&) noexcept = default;

    TabbedWindowPage::~TabbedWindowPage() = default;

    const std::string& TabbedWindowPage::contentId() const
    {
        return m_contentId;
    }

    void TabbedWindowPage::setContentId(std::string contentId)
    {
        m_contentId = std::move(contentId);
    }

    const std::string& TabbedWindowPage::title() const
    {
        return m_title;
    }

    void TabbedWindowPage::setTitle(std::string title)
    {
        m_title = std::move(title);
    }

    const Style& TabbedWindowPage::normalTabStyle() const
    {
        return m_normalTabStyle;
    }

    void TabbedWindowPage::setNormalTabStyle(const Style& style)
    {
        m_normalTabStyle = style;
    }

    const Style& TabbedWindowPage::hoveredTabStyle() const
    {
        return m_hoveredTabStyle;
    }

    void TabbedWindowPage::setHoveredTabStyle(const Style& style)
    {
        m_hoveredTabStyle = style;
    }

    const Style& TabbedWindowPage::selectedTabStyle() const
    {
        return m_selectedTabStyle;
    }

    void TabbedWindowPage::setSelectedTabStyle(const Style& style)
    {
        m_selectedTabStyle = style;
    }

    const TabbedWindowPageMetadata& TabbedWindowPage::metadata() const
    {
        return m_metadata;
    }

    TabbedWindowPageMetadata& TabbedWindowPage::metadata()
    {
        return m_metadata;
    }

    void TabbedWindowPage::setMetadata(const TabbedWindowPageMetadata& metadata)
    {
        m_metadata = metadata;
    }

    bool TabbedWindowPage::isValid() const
    {
        return !m_contentId.empty() && m_content != nullptr;
    }

    bool TabbedWindowPage::hasContent() const
    {
        return m_content != nullptr;
    }

    IWindowContent* TabbedWindowPage::content()
    {
        return m_content.get();
    }

    const IWindowContent* TabbedWindowPage::content() const
    {
        return m_content.get();
    }

    void TabbedWindowPage::setContent(std::unique_ptr<IWindowContent> content)
    {
        m_content = std::move(content);
    }

    std::unique_ptr<IWindowContent> TabbedWindowPage::releaseContent()
    {
        return std::move(m_content);
    }
}