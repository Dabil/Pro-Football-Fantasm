#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "UI/Menus/MenuItem.h"

namespace UI::Menus
{
    enum class MenuNavigationMode
    {
        Clamp,
        Wrap
    };

    class MenuModel
    {
    public:
        MenuModel() = default;

        void clear();

        bool isEmpty() const;
        std::size_t itemCount() const;

        std::size_t addItem(MenuItem item);
        std::size_t addItem(std::string label, std::string commandId);
        std::size_t addItem(std::string label, std::string commandId, std::string description);

        bool removeItemAt(std::size_t index);
        bool removeItemByCommandId(const std::string& commandId);

        MenuItem* itemAt(std::size_t index);
        const MenuItem* itemAt(std::size_t index) const;

        MenuItem* findByCommandId(const std::string& commandId);
        const MenuItem* findByCommandId(const std::string& commandId) const;

        std::optional<std::size_t> indexOfCommandId(const std::string& commandId) const;

        bool setLabel(std::size_t index, std::string label);
        bool setCommandId(std::size_t index, std::string commandId);
        bool setDescription(std::size_t index, std::string description);
        bool setEnabled(std::size_t index, bool enabled);
        bool setChecked(std::size_t index, bool checked);
        bool setSelected(std::size_t index, bool selected);

        const std::vector<MenuItem>& items() const;

        bool hasEnabledItems() const;
        std::optional<std::size_t> firstEnabledIndex() const;
        std::optional<std::size_t> lastEnabledIndex() const;

        std::optional<std::size_t> nextEnabledIndex(
            std::size_t currentIndex,
            MenuNavigationMode mode = MenuNavigationMode::Wrap) const;

        std::optional<std::size_t> previousEnabledIndex(
            std::size_t currentIndex,
            MenuNavigationMode mode = MenuNavigationMode::Wrap) const;

    private:
        bool isValidIndex(std::size_t index) const;

    private:
        std::vector<MenuItem> m_items;
    };
}