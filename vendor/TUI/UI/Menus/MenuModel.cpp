#include "UI/Menus/MenuModel.h"

#include <utility>

namespace UI::Menus
{
    void MenuModel::clear()
    {
        m_items.clear();
    }

    bool MenuModel::isEmpty() const
    {
        return m_items.empty();
    }

    std::size_t MenuModel::itemCount() const
    {
        return m_items.size();
    }

    std::size_t MenuModel::addItem(MenuItem item)
    {
        m_items.push_back(std::move(item));
        return m_items.size() - 1;
    }

    std::size_t MenuModel::addItem(std::string label, std::string commandId)
    {
        return addItem(MenuItem(std::move(label), std::move(commandId)));
    }

    std::size_t MenuModel::addItem(std::string label, std::string commandId, std::string description)
    {
        return addItem(MenuItem(std::move(label), std::move(commandId), std::move(description)));
    }

    bool MenuModel::removeItemAt(std::size_t index)
    {
        if (!isValidIndex(index))
        {
            return false;
        }

        m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool MenuModel::removeItemByCommandId(const std::string& commandId)
    {
        const std::optional<std::size_t> index = indexOfCommandId(commandId);

        if (!index.has_value())
        {
            return false;
        }

        return removeItemAt(index.value());
    }

    MenuItem* MenuModel::itemAt(std::size_t index)
    {
        if (!isValidIndex(index))
        {
            return nullptr;
        }

        return &m_items[index];
    }

    const MenuItem* MenuModel::itemAt(std::size_t index) const
    {
        if (!isValidIndex(index))
        {
            return nullptr;
        }

        return &m_items[index];
    }

    MenuItem* MenuModel::findByCommandId(const std::string& commandId)
    {
        const std::optional<std::size_t> index = indexOfCommandId(commandId);

        if (!index.has_value())
        {
            return nullptr;
        }

        return &m_items[index.value()];
    }

    const MenuItem* MenuModel::findByCommandId(const std::string& commandId) const
    {
        const std::optional<std::size_t> index = indexOfCommandId(commandId);

        if (!index.has_value())
        {
            return nullptr;
        }

        return &m_items[index.value()];
    }

    std::optional<std::size_t> MenuModel::indexOfCommandId(const std::string& commandId) const
    {
        for (std::size_t index = 0; index < m_items.size(); ++index)
        {
            if (m_items[index].commandId() == commandId)
            {
                return index;
            }
        }

        return std::nullopt;
    }

    bool MenuModel::setLabel(std::size_t index, std::string label)
    {
        MenuItem* item = itemAt(index);

        if (item == nullptr)
        {
            return false;
        }

        item->setLabel(std::move(label));
        return true;
    }

    bool MenuModel::setCommandId(std::size_t index, std::string commandId)
    {
        MenuItem* item = itemAt(index);

        if (item == nullptr)
        {
            return false;
        }

        item->setCommandId(std::move(commandId));
        return true;
    }

    bool MenuModel::setDescription(std::size_t index, std::string description)
    {
        MenuItem* item = itemAt(index);

        if (item == nullptr)
        {
            return false;
        }

        item->setDescription(std::move(description));
        return true;
    }

    bool MenuModel::setEnabled(std::size_t index, bool enabled)
    {
        MenuItem* item = itemAt(index);

        if (item == nullptr)
        {
            return false;
        }

        item->setEnabled(enabled);
        return true;
    }

    bool MenuModel::setChecked(std::size_t index, bool checked)
    {
        MenuItem* item = itemAt(index);

        if (item == nullptr)
        {
            return false;
        }

        item->setChecked(checked);
        return true;
    }

    bool MenuModel::setSelected(std::size_t index, bool selected)
    {
        MenuItem* item = itemAt(index);

        if (item == nullptr)
        {
            return false;
        }

        item->setSelected(selected);
        return true;
    }

    const std::vector<MenuItem>& MenuModel::items() const
    {
        return m_items;
    }

    bool MenuModel::hasEnabledItems() const
    {
        return firstEnabledIndex().has_value();
    }

    std::optional<std::size_t> MenuModel::firstEnabledIndex() const
    {
        for (std::size_t index = 0; index < m_items.size(); ++index)
        {
            if (m_items[index].isEnabled())
            {
                return index;
            }
        }

        return std::nullopt;
    }

    std::optional<std::size_t> MenuModel::lastEnabledIndex() const
    {
        for (std::size_t remaining = m_items.size(); remaining > 0; --remaining)
        {
            const std::size_t index = remaining - 1;

            if (m_items[index].isEnabled())
            {
                return index;
            }
        }

        return std::nullopt;
    }

    std::optional<std::size_t> MenuModel::nextEnabledIndex(
        std::size_t currentIndex,
        MenuNavigationMode mode) const
    {
        if (m_items.empty())
        {
            return std::nullopt;
        }

        const std::size_t startIndex = currentIndex < m_items.size()
            ? currentIndex
            : m_items.size() - 1;

        for (std::size_t index = startIndex + 1; index < m_items.size(); ++index)
        {
            if (m_items[index].isEnabled())
            {
                return index;
            }
        }

        if (mode == MenuNavigationMode::Clamp)
        {
            return std::nullopt;
        }

        for (std::size_t index = 0; index <= startIndex && index < m_items.size(); ++index)
        {
            if (m_items[index].isEnabled())
            {
                return index;
            }
        }

        return std::nullopt;
    }

    std::optional<std::size_t> MenuModel::previousEnabledIndex(
        std::size_t currentIndex,
        MenuNavigationMode mode) const
    {
        if (m_items.empty())
        {
            return std::nullopt;
        }

        const std::size_t startIndex = currentIndex < m_items.size()
            ? currentIndex
            : m_items.size() - 1;

        for (std::size_t remaining = startIndex; remaining > 0; --remaining)
        {
            const std::size_t index = remaining - 1;

            if (m_items[index].isEnabled())
            {
                return index;
            }
        }

        if (mode == MenuNavigationMode::Clamp)
        {
            return std::nullopt;
        }

        for (std::size_t remaining = m_items.size(); remaining > startIndex + 1; --remaining)
        {
            const std::size_t index = remaining - 1;

            if (m_items[index].isEnabled())
            {
                return index;
            }
        }

        return std::nullopt;
    }

    bool MenuModel::isValidIndex(std::size_t index) const
    {
        return index < m_items.size();
    }
}