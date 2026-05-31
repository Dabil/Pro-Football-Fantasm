#include "UI/Menus/MenuItem.h"

#include <utility>

namespace UI::Menus
{
    MenuItem::MenuItem(std::string label, std::string commandId)
        : m_label(std::move(label))
        , m_commandId(std::move(commandId))
    {
    }

    MenuItem::MenuItem(std::string label, std::string commandId, std::string description)
        : m_label(std::move(label))
        , m_commandId(std::move(commandId))
        , m_description(std::move(description))
    {
    }

    const std::string& MenuItem::label() const
    {
        return m_label;
    }

    void MenuItem::setLabel(std::string label)
    {
        m_label = std::move(label);
    }

    const std::string& MenuItem::commandId() const
    {
        return m_commandId;
    }

    void MenuItem::setCommandId(std::string commandId)
    {
        m_commandId = std::move(commandId);
    }

    const std::string& MenuItem::description() const
    {
        return m_description;
    }

    void MenuItem::setDescription(std::string description)
    {
        m_description = std::move(description);
    }

    bool MenuItem::hasDescription() const
    {
        return !m_description.empty();
    }

    bool MenuItem::isEnabled() const
    {
        return m_enabled;
    }

    void MenuItem::setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    bool MenuItem::isChecked() const
    {
        return m_checked;
    }

    void MenuItem::setChecked(bool checked)
    {
        m_checked = checked;
    }

    bool MenuItem::isSelected() const
    {
        return m_selected;
    }

    void MenuItem::setSelected(bool selected)
    {
        m_selected = selected;
    }
}