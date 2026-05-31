#include "UI/Panels/TabbedWindow.h"

#include <algorithm>
#include <utility>

#include "Input/MouseEvent.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace UI
{
    TabbedWindow::TabbedWindow()
        : Window()
    {
        setTitle("( Tabbed Window )");
    }

    TabbedWindow::TabbedWindow(const Rect& bounds)
        : Window(bounds, "( Tabbed Window )")
    {
    }

    TabbedWindow::TabbedWindow(const Rect& bounds, std::string title)
        : Window(bounds, std::move(title))
    {
    }

    TabbedWindow::~TabbedWindow()
    {
        detachAllPageContent();
    }

    TabbedWindowModel& TabbedWindow::model()
    {
        return m_model;
    }

    const TabbedWindowModel& TabbedWindow::model() const
    {
        return m_model;
    }

    bool TabbedWindow::empty() const
    {
        return m_model.empty();
    }

    std::size_t TabbedWindow::tabCount() const
    {
        return m_model.count();
    }

    bool TabbedWindow::addPage(TabbedWindowPage page, bool selectNewPage)
    {
        const std::string contentId = page.contentId();

        const bool added = m_model.addPage(std::move(page), selectNewPage);

        if (!added)
        {
            return false;
        }

        const int addedIndex = m_model.indexOfContentId(contentId);
        if (addedIndex >= 0)
        {
            attachPageContent(m_model.pages()[static_cast<std::size_t>(addedIndex)]);
        }

        synchronizeTitleWithSelectedPage();
        notifyContentBoundsChanged();
        return true;
    }

    bool TabbedWindow::addContentPage(
        std::string contentId,
        std::string title,
        std::unique_ptr<IWindowContent> content,
        bool selectNewPage)
    {
        TabbedWindowPage page(
            std::move(contentId),
            std::move(title),
            std::move(content));

        return addPage(std::move(page), selectNewPage);
    }

    bool TabbedWindow::addContentPage(
        std::string contentId,
        std::string title,
        std::unique_ptr<IWindowContent> content,
        TabbedWindowPageMetadata metadata,
        bool selectNewPage)
    {
        TabbedWindowPage page(
            std::move(contentId),
            std::move(title),
            std::move(content),
            std::move(metadata));

        return addPage(std::move(page), selectNewPage);
    }

    TabbedWindowPage TabbedWindow::removePageAt(std::size_t index)
    {
        TabbedWindowPage removed = m_model.removePageAt(index);
        detachPageContent(removed);

        if (m_model.empty())
        {
            clearHoveredTabIndex();
            setTitle("Tabbed Window");
            m_lastContentBounds = Rect{};
            return removed;
        }

        if (m_hoveredTabIndex >= static_cast<int>(m_model.count()))
        {
            clearHoveredTabIndex();
        }

        synchronizeTitleWithSelectedPage();
        notifyContentBoundsChanged();

        return removed;
    }

    TabbedWindowPage TabbedWindow::removePageByContentId(
        const std::string& contentId)
    {
        TabbedWindowPage removed = m_model.removePageByContentId(contentId);
        detachPageContent(removed);

        if (m_model.empty())
        {
            clearHoveredTabIndex();
            setTitle("Tabbed Window");
            m_lastContentBounds = Rect{};
            return removed;
        }

        if (m_hoveredTabIndex >= static_cast<int>(m_model.count()))
        {
            clearHoveredTabIndex();
        }

        synchronizeTitleWithSelectedPage();
        notifyContentBoundsChanged();

        return removed;
    }

    bool TabbedWindow::selectTab(std::size_t index)
    {
        if (!m_model.selectIndex(index))
        {
            return false;
        }

        synchronizeTitleWithSelectedPage();
        notifyContentBoundsChanged();

        return true;
    }

    bool TabbedWindow::selectTabByContentId(const std::string& contentId)
    {
        if (!m_model.selectContentId(contentId))
        {
            return false;
        }

        synchronizeTitleWithSelectedPage();
        notifyContentBoundsChanged();

        return true;
    }

    std::size_t TabbedWindow::selectedTabIndex() const
    {
        return m_model.selectedIndex();
    }

    const std::string& TabbedWindow::selectedContentId() const
    {
        return m_model.selectedContentId();
    }

    int TabbedWindow::hoveredTabIndex() const
    {
        return m_hoveredTabIndex;
    }

    void TabbedWindow::setHoveredTabIndex(int index)
    {
        if (index < 0 || index >= static_cast<int>(m_model.count()))
        {
            m_hoveredTabIndex = -1;
            return;
        }

        m_hoveredTabIndex = index;
    }

    void TabbedWindow::clearHoveredTabIndex()
    {
        m_hoveredTabIndex = -1;
    }

    int TabbedWindow::tabIndexAt(Point screenPosition) const
    {
        const Rect headerBounds = tabHeaderBounds();

        if (!headerBounds.contains(screenPosition.x, screenPosition.y))
        {
            return -1;
        }

        int x = headerBounds.position.x;
        const int maxX = headerBounds.position.x + headerBounds.size.width;

        for (std::size_t index = 0; index < m_model.pages().size(); ++index)
        {
            if (x >= maxX)
            {
                break;
            }

            const int remainingWidth = maxX - x;

            if (remainingWidth <= 0)
            {
                break;
            }

            const TabbedWindowPage& page = m_model.pages()[index];

            std::string label = tabLabelForPage(page);
            label = clippedTabLabel(label, remainingWidth);

            if (label.empty())
            {
                break;
            }

            const int tabStart = x;
            const int tabEnd = x + static_cast<int>(label.size());

            if (screenPosition.x >= tabStart && screenPosition.x < tabEnd)
            {
                return static_cast<int>(index);
            }

            x = tabEnd;

            if (index + 1 < m_model.pages().size())
            {
                x += 3;
            }
        }

        return -1;
    }

    bool TabbedWindow::isPointInTabHeader(Point screenPosition) const
    {
        return tabHeaderBounds().contains(screenPosition.x, screenPosition.y);
    }

    bool TabbedWindow::isPointInSelectedContent(Point screenPosition) const
    {
        return selectedContentBounds().contains(screenPosition.x, screenPosition.y);
    }

    bool TabbedWindow::isPointInTabTitle(Point screenPosition) const
    {
        return tabIndexAt(screenPosition) >= 0;
    }

    TabbedWindowPage* TabbedWindow::selectedPage()
    {
        return m_model.selectedPage();
    }

    const TabbedWindowPage* TabbedWindow::selectedPage() const
    {
        return m_model.selectedPage();
    }

    IWindowContent* TabbedWindow::selectedContent()
    {
        TabbedWindowPage* page = selectedPage();

        if (page == nullptr)
        {
            return nullptr;
        }

        return page->content();
    }

    const IWindowContent* TabbedWindow::selectedContent() const
    {
        const TabbedWindowPage* page = selectedPage();

        if (page == nullptr)
        {
            return nullptr;
        }

        return page->content();
    }

    Rect TabbedWindow::tabHeaderBounds() const
    {
        const Rect windowBounds = bounds();

        if (windowBounds.size.width <= 4 || windowBounds.size.height <= 0)
        {
            return Rect{
                Point{ windowBounds.position.x, windowBounds.position.y },
                Size{ 0, 0 }
            };
        }

        return Rect{
            Point{ windowBounds.position.x + 2, windowBounds.position.y },
            Size{ windowBounds.size.width - 4, 1 }
        };
    }

    Rect TabbedWindow::selectedContentBounds() const
    {
        return contentBounds();
    }

    void TabbedWindow::update(const Animation::TickEvent& event)
    {
        if (!isVisible() || !isEnabled())
        {
            return;
        }

        notifyContentBoundsChanged();

        for (TabbedWindowPage& page : m_model.pages())
        {
            IWindowContent* content = page.content();

            if (content == nullptr)
            {
                continue;
            }

            content->update(event);
        }
    }

    bool TabbedWindow::handleEvent(const Input::Event& event)
    {
        if (const Input::MouseEvent* mouseEvent = event.asMouse())
        {
            return handleMouseEvent(*mouseEvent);
        }

        IWindowContent* content = selectedContent();

        if (content != nullptr && content->handleEvent(event))
        {
            return true;
        }

        return Window::handleEvent(event);
    }

    void TabbedWindow::draw(Surface& surface)
    {
        if (!isVisible())
        {
            return;
        }

        if (m_model.empty())
        {
            Window::draw(surface);
            return;
        }

        notifyContentBoundsChanged();

        const Style originalBackgroundStyle = backgroundStyle();
        const Style originalBorderStyle = borderStyle();
        const Style originalTitleStyle = titleStyle();
        const Style originalHoveredBorderStyle = hoveredBorderStyle();
        const Style originalHoveredTitleStyle = hoveredTitleStyle();
        const ObjectFactory::BorderGlyphs originalBorderGlyphs = borderGlyphs();
        const Size originalMinimumSize = minimumSize();
        const int originalResizeBorderThickness = resizeBorderThickness();
        const int originalTitleBarHeight = titleBarHeight();
        const bool originalDraggable = isDraggable();
        const bool originalResizable = isResizable();

        applySelectedPageChromeMetadata();

        Window::draw(surface);
        drawTitleTabs(surface);

        IWindowContent* content = selectedContent();

        if (content != nullptr)
        {
            const Rect contentRect = selectedContentBounds();

            if (contentRect.size.width > 0 && contentRect.size.height > 0)
            {
                content->draw(surface, contentRect);
            }
        }

        restoreChromeMetadata(
            originalBackgroundStyle,
            originalBorderStyle,
            originalTitleStyle,
            originalHoveredBorderStyle,
            originalHoveredTitleStyle,
            originalBorderGlyphs,
            originalMinimumSize,
            originalResizeBorderThickness,
            originalTitleBarHeight,
            originalDraggable,
            originalResizable);
    }

    void TabbedWindow::synchronizeTitleWithSelectedPage()
    {
        setTitle(titleForAllTabs());
    }

    void TabbedWindow::notifyContentBoundsChanged()
    {
        const Rect contentRect = selectedContentBounds();

        if (contentRect.position.x == m_lastContentBounds.position.x &&
            contentRect.position.y == m_lastContentBounds.position.y &&
            contentRect.size.width == m_lastContentBounds.size.width &&
            contentRect.size.height == m_lastContentBounds.size.height)
        {
            return;
        }

        m_lastContentBounds = contentRect;

        for (TabbedWindowPage& page : m_model.pages())
        {
            IWindowContent* content = page.content();

            if (content == nullptr)
            {
                continue;
            }

            content->onBoundsChanged(contentRect);
        }
    }

    void TabbedWindow::attachPageContent(TabbedWindowPage& page)
    {
        IWindowContent* content = page.content();

        if (content == nullptr)
        {
            return;
        }

        content->onAttached();
        content->onBoundsChanged(selectedContentBounds());
    }

    void TabbedWindow::detachPageContent(TabbedWindowPage& page)
    {
        IWindowContent* content = page.content();

        if (content == nullptr)
        {
            return;
        }

        content->onDetached();
    }

    void TabbedWindow::detachAllPageContent()
    {
        for (TabbedWindowPage& page : m_model.pages())
        {
            detachPageContent(page);
        }
    }

    bool TabbedWindow::handleMouseEvent(const Input::MouseEvent& mouseEvent)
    {
        updateHoveredTabFromPoint(mouseEvent.position);

        if (handleTabHeaderMouseEvent(mouseEvent))
        {
            return true;
        }

        if (routeContentMouseEvent(mouseEvent))
        {
            return true;
        }

        return Window::handleEvent(Input::Event::mouse(mouseEvent));
    }

    bool TabbedWindow::handleTabHeaderMouseEvent(
        const Input::MouseEvent& mouseEvent)
    {
        if (!isPointInTabHeader(mouseEvent.position))
        {
            return false;
        }

        const int tabIndex = tabIndexAt(mouseEvent.position);

        if (mouseEvent.isMove() || mouseEvent.isDrag())
        {
            setHoveredTabIndex(tabIndex);
            return tabIndex >= 0;
        }

        if (mouseEvent.button == Input::MouseButton::Left &&
            (mouseEvent.isPress() || mouseEvent.isClick()))
        {
            if (tabIndex < 0)
            {
                return false;
            }

            selectTab(static_cast<std::size_t>(tabIndex));
            setHoveredTabIndex(tabIndex);
            return true;
        }

        return tabIndex >= 0;
    }

    bool TabbedWindow::routeContentMouseEvent(
        const Input::MouseEvent& mouseEvent)
    {
        if (!isPointInSelectedContent(mouseEvent.position))
        {
            return false;
        }

        IWindowContent* content = selectedContent();

        if (content == nullptr)
        {
            return false;
        }

        return content->handleEvent(Input::Event::mouse(mouseEvent));
    }

    void TabbedWindow::updateHoveredTabFromPoint(Point screenPosition)
    {
        const int tabIndex = tabIndexAt(screenPosition);
        if (tabIndex >= 0)
        {
            setHoveredTabIndex(tabIndex);
            return;
        }

        clearHoveredTabIndex();
    }

    void TabbedWindow::drawTitleTabs(Surface& surface)
    {
        const Rect headerBounds = tabHeaderBounds();

        if (headerBounds.size.width <= 0 || headerBounds.size.height <= 0)
        {
            return;
        }

        ScreenBuffer& buffer = surface.buffer();

        int x = headerBounds.position.x;
        const int y = headerBounds.position.y;
        const int maxX = headerBounds.position.x + headerBounds.size.width;

        for (std::size_t index = 0; index < m_model.pages().size(); ++index)
        {
            if (x >= maxX)
            {
                break;
            }

            const int remainingWidth = maxX - x;

            if (remainingWidth <= 0)
            {
                break;
            }

            const TabbedWindowPage& page = m_model.pages()[index];

            std::string label = tabLabelForPage(page);
            label = clippedTabLabel(label, remainingWidth);

            if (label.empty())
            {
                break;
            }

            buffer.writeString(
                x,
                y,
                label,
                tabStyleForIndex(index));

            x += static_cast<int>(label.size());

            if (index + 1 < m_model.pages().size() && x + 3 <= maxX)
            {
                const Style separatorStyle = isHovered()
                    ? hoveredBorderStyle()
                    : borderStyle();

                buffer.writeChar(x, y, U' ', separatorStyle);
                ++x;

                buffer.writeChar(x, y, borderGlyphs().horizontal, separatorStyle);
                ++x;

                buffer.writeChar(x, y, U' ', separatorStyle);
                ++x;
            }
        }
    }

    std::string TabbedWindow::titleForAllTabs() const
    {
        if (m_model.empty())
        {
            return "Tabbed Window";
        }

        std::u32string result;

        for (const TabbedWindowPage& page : m_model.pages())
        {
            const std::string label = tabLabelForPage(page);

            if (!result.empty())
            {
                result += U' ';
                result += borderGlyphs().horizontal;
                result += U' ';
            }

            const std::u32string labelText = UnicodeConversion::utf8ToU32(label);
            result += labelText;
        }

        return UnicodeConversion::u32ToUtf8(result);
    }

    std::string TabbedWindow::tabLabelForPage(
        const TabbedWindowPage& page) const
    {
        if (!page.title().empty())
        {
            return page.title();
        }

        if (!page.contentId().empty())
        {
            return page.contentId();
        }

        return "untitled";
    }

    std::string TabbedWindow::clippedTabLabel(
        const std::string& label,
        int maxWidth) const
    {
        if (maxWidth <= 0 || label.empty())
        {
            return {};
        }

        if (static_cast<int>(label.size()) <= maxWidth)
        {
            return label;
        }

        if (maxWidth <= 1)
        {
            return label.substr(0, static_cast<std::size_t>(maxWidth));
        }

        return label.substr(0, static_cast<std::size_t>(maxWidth - 1)) + "~";
    }

    Style TabbedWindow::tabStyleForIndex(std::size_t index) const
    {
        const TabbedWindowPage& page = m_model.pages()[index];
        const TabbedWindowPageMetadata& metadata = page.metadata();

        Style style = metadata.titleStyle.isEmpty()
            ? page.normalTabStyle()
            : metadata.titleStyle;

        if (isHovered())
        {
            if (!metadata.hoveredTitleStyle.isEmpty())
            {
                style = metadata.hoveredTitleStyle;
            }
            else
            {
                style = page.hoveredTabStyle();
            }
        }

        if (index == selectedTabIndex())
        {
            style = style.withReverse(true);
        }

        return style;
    }

    void TabbedWindow::applySelectedPageChromeMetadata()
    {
        const TabbedWindowPage* page = selectedPage();

        if (page == nullptr)
        {
            return;
        }

        const TabbedWindowPageMetadata& metadata = page->metadata();

        setModal(metadata.modal);
        setDraggable(metadata.draggable);
        setResizable(metadata.resizable);
        setMinimumSize(metadata.minimumSize);
        setResizeBorderThickness(metadata.resizeBorderThickness);
        setTitleBarHeight(metadata.titleBarHeight);

        if (!metadata.backgroundStyle.isEmpty())
        {
            setBackgroundStyle(metadata.backgroundStyle);
        }

        if (!metadata.borderStyle.isEmpty())
        {
            setBorderStyle(metadata.borderStyle);
        }

        if (!metadata.titleStyle.isEmpty())
        {
            setTitleStyle(metadata.titleStyle);
        }

        if (!metadata.hoveredBorderStyle.isEmpty())
        {
            setHoveredBorderStyle(metadata.hoveredBorderStyle);
        }

        if (!metadata.hoveredTitleStyle.isEmpty())
        {
            setHoveredTitleStyle(metadata.hoveredTitleStyle);
        }

        setBorderGlyphs(metadata.borderGlyphs);
    }

    void TabbedWindow::restoreChromeMetadata(
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
        bool originalResizable)
    {
        setBackgroundStyle(originalBackgroundStyle);
        setBorderStyle(originalBorderStyle);
        setTitleStyle(originalTitleStyle);
        setHoveredBorderStyle(originalHoveredBorderStyle);
        setHoveredTitleStyle(originalHoveredTitleStyle);
        setBorderGlyphs(originalBorderGlyphs);
        setMinimumSize(originalMinimumSize);
        setResizeBorderThickness(originalResizeBorderThickness);
        setTitleBarHeight(originalTitleBarHeight);
        setDraggable(originalDraggable);
        setResizable(originalResizable);
    }
}