#pragma once

#include <string>

namespace UI::Menus
{
    class MenuItem
    {
    public:
        MenuItem() = default;
        MenuItem(std::string label, std::string commandId);
        MenuItem(std::string label, std::string commandId, std::string description);

        const std::string& label() const;
        void setLabel(std::string label);

        const std::string& commandId() const;
        void setCommandId(std::string commandId);

        const std::string& description() const;
        void setDescription(std::string description);
        bool hasDescription() const;

        bool isEnabled() const;
        void setEnabled(bool enabled);

        bool isChecked() const;
        void setChecked(bool checked);

        bool isSelected() const;
        void setSelected(bool selected);

    private:
        std::string m_label;
        std::string m_commandId;
        std::string m_description;

        bool m_enabled = true;
        bool m_checked = false;
        bool m_selected = false;
    };
}