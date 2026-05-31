#pragma once

#include <string>
#include <vector>

#include "UI/Menus/MenuModel.h"
#include "UI/Panels/StatusBar.h"

namespace UI::Config
{
    struct MenuStatusConfigItem
    {
        std::string label;
        std::string commandId;
        std::string description;
        bool enabled = true;
        bool checked = false;
    };

    struct StatusConfig
    {
        std::string instructions;
        std::vector<std::string> commandHints;
    };

    struct PageInteractionConfig
    {
        std::string pageId;
        std::string title;
        std::string defaultCommandId;
        bool wrapNavigation = true;
    };

    struct MenuStatusConfig
    {
        PageInteractionConfig page;
        std::vector<MenuStatusConfigItem> menuItems;
        StatusConfig status;
    };

    struct MenuStatusConfigLoadResult
    {
        MenuStatusConfig config;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;

        bool succeeded() const;
        bool hasWarnings() const;
    };

    class MenuStatusConfigLoader
    {
    public:
        static MenuStatusConfigLoadResult loadFromFile(const std::string& path);
        static MenuStatusConfigLoadResult loadFromText(const std::string& text);

        static bool applyToRuntime(
            const MenuStatusConfig& config,
            UI::Menus::MenuModel& menuModel,
            StatusBar& statusBar,
            std::vector<std::string>* warnings = nullptr);

    private:
        static MenuStatusConfigLoadResult parseLines(
            const std::vector<std::string>& lines,
            const std::string& sourceName);
    };
}