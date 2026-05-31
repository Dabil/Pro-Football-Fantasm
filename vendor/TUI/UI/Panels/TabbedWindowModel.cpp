#include "UI/Panels/TabbedWindowModel.h"

#include <algorithm>
#include <utility>

namespace UI
{
    namespace
    {
        const std::string EmptyContentId;
    }

    TabbedWindowModel::TabbedWindowModel() = default;

    TabbedWindowModel::~TabbedWindowModel() = default;

    TabbedWindowModel::TabbedWindowModel(TabbedWindowModel&&) noexcept = default;

    TabbedWindowModel& TabbedWindowModel::operator=(TabbedWindowModel&&) noexcept = default;

    bool TabbedWindowModel::empty() const
    {
        return m_pages.empty();
    }

    std::size_t TabbedWindowModel::count() const
    {
        return m_pages.size();
    }

    const TabbedWindowModel::PageCollection& TabbedWindowModel::pages() const
    {
        return m_pages;
    }

    TabbedWindowModel::PageCollection& TabbedWindowModel::pages()
    {
        return m_pages;
    }

    bool TabbedWindowModel::addPage(TabbedWindowPage page, bool selectNewPage)
    {
        return insertPage(m_pages.size(), std::move(page), selectNewPage);
    }

    bool TabbedWindowModel::insertPage(
        std::size_t index,
        TabbedWindowPage page,
        bool selectNewPage)
    {
        if (!page.isValid())
        {
            return false;
        }

        if (containsContentId(page.contentId()))
        {
            return false;
        }

        const std::size_t insertIndex = std::min(index, m_pages.size());

        m_pages.insert(
            m_pages.begin() + static_cast<std::ptrdiff_t>(insertIndex),
            std::move(page));

        if (selectNewPage)
        {
            m_selectedIndex = insertIndex;
        }
        else
        {
            normalizeSelectedIndex();
        }

        return true;
    }

    TabbedWindowPage TabbedWindowModel::removePageAt(std::size_t index)
    {
        if (index >= m_pages.size())
        {
            return TabbedWindowPage{};
        }

        TabbedWindowPage removed = std::move(m_pages[index]);

        m_pages.erase(m_pages.begin() + static_cast<std::ptrdiff_t>(index));

        if (m_pages.empty())
        {
            m_selectedIndex = 0;
        }
        else if (m_selectedIndex >= m_pages.size())
        {
            m_selectedIndex = m_pages.size() - 1;
        }

        return removed;
    }

    TabbedWindowPage TabbedWindowModel::removePageByContentId(
        const std::string& contentId)
    {
        const int index = indexOfContentId(contentId);

        if (index < 0)
        {
            return TabbedWindowPage{};
        }

        return removePageAt(static_cast<std::size_t>(index));
    }

    void TabbedWindowModel::clear()
    {
        m_pages.clear();
        m_selectedIndex = 0;
    }

    std::size_t TabbedWindowModel::selectedIndex() const
    {
        return m_selectedIndex;
    }

    bool TabbedWindowModel::hasSelectedPage() const
    {
        return !m_pages.empty() && m_selectedIndex < m_pages.size();
    }

    TabbedWindowPage* TabbedWindowModel::selectedPage()
    {
        if (!hasSelectedPage())
        {
            return nullptr;
        }

        return &m_pages[m_selectedIndex];
    }

    const TabbedWindowPage* TabbedWindowModel::selectedPage() const
    {
        if (!hasSelectedPage())
        {
            return nullptr;
        }

        return &m_pages[m_selectedIndex];
    }

    const std::string& TabbedWindowModel::selectedContentId() const
    {
        const TabbedWindowPage* page = selectedPage();

        if (page == nullptr)
        {
            return EmptyContentId;
        }

        return page->contentId();
    }

    bool TabbedWindowModel::selectIndex(std::size_t index)
    {
        if (index >= m_pages.size())
        {
            return false;
        }

        m_selectedIndex = index;
        return true;
    }

    bool TabbedWindowModel::selectContentId(const std::string& contentId)
    {
        const int index = indexOfContentId(contentId);

        if (index < 0)
        {
            return false;
        }

        return selectIndex(static_cast<std::size_t>(index));
    }

    int TabbedWindowModel::indexOfContentId(const std::string& contentId) const
    {
        if (contentId.empty())
        {
            return -1;
        }

        for (std::size_t index = 0; index < m_pages.size(); ++index)
        {
            if (m_pages[index].contentId() == contentId)
            {
                return static_cast<int>(index);
            }
        }

        return -1;
    }

    bool TabbedWindowModel::containsContentId(const std::string& contentId) const
    {
        return indexOfContentId(contentId) >= 0;
    }

    void TabbedWindowModel::normalizeSelectedIndex()
    {
        if (m_pages.empty())
        {
            m_selectedIndex = 0;
            return;
        }

        if (m_selectedIndex >= m_pages.size())
        {
            m_selectedIndex = m_pages.size() - 1;
        }
    }
}