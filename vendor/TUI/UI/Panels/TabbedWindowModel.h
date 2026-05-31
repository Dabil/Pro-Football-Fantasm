#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "UI/Panels/TabbedWindowPage.h"

namespace UI
{
    class TabbedWindowModel
    {
    public:
        using PageCollection = std::vector<TabbedWindowPage>;

        TabbedWindowModel();
        ~TabbedWindowModel();

        TabbedWindowModel(const TabbedWindowModel&) = delete;
        TabbedWindowModel& operator=(const TabbedWindowModel&) = delete;

        TabbedWindowModel(TabbedWindowModel&&) noexcept;
        TabbedWindowModel& operator=(TabbedWindowModel&&) noexcept;

        bool empty() const;
        std::size_t count() const;

        const PageCollection& pages() const;
        PageCollection& pages();

        bool addPage(TabbedWindowPage page, bool selectNewPage = true);
        bool insertPage(
            std::size_t index,
            TabbedWindowPage page,
            bool selectNewPage = true);

        TabbedWindowPage removePageAt(std::size_t index);
        TabbedWindowPage removePageByContentId(const std::string& contentId);

        void clear();

        std::size_t selectedIndex() const;
        bool hasSelectedPage() const;

        TabbedWindowPage* selectedPage();
        const TabbedWindowPage* selectedPage() const;

        const std::string& selectedContentId() const;

        bool selectIndex(std::size_t index);
        bool selectContentId(const std::string& contentId);

        int indexOfContentId(const std::string& contentId) const;
        bool containsContentId(const std::string& contentId) const;

    private:
        void normalizeSelectedIndex();

    private:
        PageCollection m_pages;
        std::size_t m_selectedIndex = 0;
    };
}