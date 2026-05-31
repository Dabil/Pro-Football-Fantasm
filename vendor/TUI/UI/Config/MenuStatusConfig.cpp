#include "UI/Config/MenuStatusConfig.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

/*
    Example usage:

    const UI::Config::MenuStatusConfigLoadResult result =
    UI::Config::MenuStatusConfigLoader::loadFromFile("Screens/Content/MenuDemo.menu");

        if (result.succeeded())
        {
            std::vector<std::string> applyWarnings;
            UI::Config::MenuStatusConfigLoader::applyToRuntime(
                result.config,
                m_menuModel,
                m_statusBar,
                &applyWarnings);
        }
        else
        {
            buildMenu(); // existing fallback path
        }
*/

namespace UI::Config
{
    namespace
    {
        enum class Section
        {
            None,
            Page,
            Status,
            Menu
        };

        std::string trim(const std::string& value)
        {
            const auto begin = std::find_if_not(
                value.begin(),
                value.end(),
                [](unsigned char ch) { return std::isspace(ch) != 0; });

            const auto end = std::find_if_not(
                value.rbegin(),
                value.rend(),
                [](unsigned char ch) { return std::isspace(ch) != 0; }).base();

            if (begin >= end)
            {
                return {};
            }

            return std::string(begin, end);
        }

        std::string toLower(std::string value)
        {
            std::transform(
                value.begin(),
                value.end(),
                value.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            return value;
        }

        bool parseBool(const std::string& value, bool fallback, bool* parsed)
        {
            const std::string normalized = toLower(trim(value));

            if (normalized == "true" || normalized == "yes" || normalized == "on" || normalized == "1")
            {
                if (parsed != nullptr)
                {
                    *parsed = true;
                }
                return true;
            }

            if (normalized == "false" || normalized == "no" || normalized == "off" || normalized == "0")
            {
                if (parsed != nullptr)
                {
                    *parsed = true;
                }
                return false;
            }

            if (parsed != nullptr)
            {
                *parsed = false;
            }

            return fallback;
        }

        std::vector<std::string> splitPipes(const std::string& value)
        {
            std::vector<std::string> parts;
            std::string current;
            bool escaping = false;

            for (const char ch : value)
            {
                if (escaping)
                {
                    current.push_back(ch);
                    escaping = false;
                    continue;
                }

                if (ch == '\\')
                {
                    escaping = true;
                    continue;
                }

                if (ch == '|')
                {
                    parts.push_back(trim(current));
                    current.clear();
                    continue;
                }

                current.push_back(ch);
            }

            if (escaping)
            {
                current.push_back('\\');
            }

            parts.push_back(trim(current));
            return parts;
        }

        std::string linePrefix(const std::string& sourceName, int lineNumber)
        {
            std::ostringstream stream;
            stream << sourceName << ":" << lineNumber << ": ";
            return stream.str();
        }

        bool parseKeyValue(
            const std::string& line,
            std::string* key,
            std::string* value)
        {
            const std::size_t equals = line.find('=');

            if (equals == std::string::npos)
            {
                return false;
            }

            *key = toLower(trim(line.substr(0, equals)));
            *value = trim(line.substr(equals + 1));
            return !key->empty();
        }

        void appendWarning(
            MenuStatusConfigLoadResult& result,
            const std::string& sourceName,
            int lineNumber,
            const std::string& message)
        {
            result.warnings.push_back(linePrefix(sourceName, lineNumber) + message);
        }

        void appendError(
            MenuStatusConfigLoadResult& result,
            const std::string& sourceName,
            int lineNumber,
            const std::string& message)
        {
            result.errors.push_back(linePrefix(sourceName, lineNumber) + message);
        }

        bool addMenuItemFromValue(
            MenuStatusConfigLoadResult& result,
            const std::string& sourceName,
            int lineNumber,
            const std::string& value)
        {
            const std::vector<std::string> parts = splitPipes(value);

            if (parts.size() < 2)
            {
                appendError(result, sourceName, lineNumber,
                    "menu item must be 'label | commandId | enabled | description'.");
                return false;
            }

            MenuStatusConfigItem item;
            item.label = parts[0];
            item.commandId = parts[1];

            if (item.label.empty())
            {
                appendError(result, sourceName, lineNumber, "menu item label is required.");
                return false;
            }

            if (item.commandId.empty())
            {
                appendError(result, sourceName, lineNumber, "menu item commandId is required.");
                return false;
            }

            if (parts.size() >= 3 && !parts[2].empty())
            {
                bool parsed = false;
                item.enabled = parseBool(parts[2], true, &parsed);

                if (!parsed)
                {
                    appendWarning(result, sourceName, lineNumber,
                        "invalid enabled value; defaulting menu item to enabled.");
                }
            }

            if (parts.size() >= 4)
            {
                item.description = parts[3];
            }

            if (parts.size() >= 5 && !parts[4].empty())
            {
                bool parsed = false;
                item.checked = parseBool(parts[4], false, &parsed);

                if (!parsed)
                {
                    appendWarning(result, sourceName, lineNumber,
                        "invalid checked value; defaulting menu item to unchecked.");
                }
            }

            if (parts.size() > 5)
            {
                appendWarning(result, sourceName, lineNumber,
                    "extra menu item fields were ignored.");
            }

            result.config.menuItems.push_back(std::move(item));
            return true;
        }
    }

    bool MenuStatusConfigLoadResult::succeeded() const
    {
        return errors.empty();
    }

    bool MenuStatusConfigLoadResult::hasWarnings() const
    {
        return !warnings.empty();
    }

    MenuStatusConfigLoadResult MenuStatusConfigLoader::loadFromFile(const std::string& path)
    {
        MenuStatusConfigLoadResult result;
        std::ifstream file(path);

        if (!file)
        {
            result.errors.push_back("Unable to open menu/status config file: " + path);
            return result;
        }

        std::vector<std::string> lines;
        std::string line;

        while (std::getline(file, line))
        {
            lines.push_back(line);
        }

        return parseLines(lines, path);
    }

    MenuStatusConfigLoadResult MenuStatusConfigLoader::loadFromText(const std::string& text)
    {
        std::vector<std::string> lines;
        std::istringstream stream(text);
        std::string line;

        while (std::getline(stream, line))
        {
            lines.push_back(line);
        }

        return parseLines(lines, "<text>");
    }

    bool MenuStatusConfigLoader::applyToRuntime(
        const MenuStatusConfig& config,
        UI::Menus::MenuModel& menuModel,
        StatusBar& statusBar,
        std::vector<std::string>* warnings)
    {
        if (config.menuItems.empty())
        {
            if (warnings != nullptr)
            {
                warnings->push_back("Config contained no menu items; runtime menu was left unchanged.");
            }

            return false;
        }

        menuModel.clear();

        for (const MenuStatusConfigItem& configItem : config.menuItems)
        {
            UI::Menus::MenuItem item(configItem.label, configItem.commandId, configItem.description);
            item.setEnabled(configItem.enabled);
            item.setChecked(configItem.checked);
            menuModel.addItem(std::move(item));
        }

        statusBar.setInstructions(config.status.instructions);
        statusBar.setCommandHints(config.status.commandHints);
        return true;
    }

    MenuStatusConfigLoadResult MenuStatusConfigLoader::parseLines(
        const std::vector<std::string>& lines,
        const std::string& sourceName)
    {
        MenuStatusConfigLoadResult result;
        Section section = Section::None;

        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            const int lineNumber = static_cast<int>(index + 1);
            std::string line = trim(lines[index]);

            if (line.empty() || line[0] == '#' || line[0] == ';')
            {
                continue;
            }

            if (line.front() == '[' && line.back() == ']')
            {
                const std::string sectionName = toLower(trim(line.substr(1, line.size() - 2)));

                if (sectionName == "page")
                {
                    section = Section::Page;
                }
                else if (sectionName == "status")
                {
                    section = Section::Status;
                }
                else if (sectionName == "menu")
                {
                    section = Section::Menu;
                }
                else
                {
                    section = Section::None;
                    appendWarning(result, sourceName, lineNumber,
                        "unknown section '" + sectionName + "' was ignored.");
                }

                continue;
            }

            std::string key;
            std::string value;
            if (!parseKeyValue(line, &key, &value))
            {
                appendWarning(result, sourceName, lineNumber,
                    "line was ignored because it is not a key=value pair.");
                continue;
            }

            switch (section)
            {
            case Section::Page:
                if (key == "id" || key == "pageid")
                {
                    result.config.page.pageId = value;
                }
                else if (key == "title")
                {
                    result.config.page.title = value;
                }
                else if (key == "default" || key == "defaultcommand" || key == "defaultcommandid")
                {
                    result.config.page.defaultCommandId = value;
                }
                else if (key == "wrap" || key == "wrapnavigation")
                {
                    bool parsed = false;
                    result.config.page.wrapNavigation = parseBool(value, true, &parsed);

                    if (!parsed)
                    {
                        appendWarning(result, sourceName, lineNumber,
                            "invalid wrapNavigation value; defaulting to true.");
                    }
                }
                else
                {
                    appendWarning(result, sourceName, lineNumber,
                        "unknown page key '" + key + "' was ignored.");
                }
                break;

            case Section::Status:
                if (key == "instructions" || key == "instruction")
                {
                    result.config.status.instructions = value;
                }
                else if (key == "hint" || key == "commandhint")
                {
                    if (!value.empty())
                    {
                        result.config.status.commandHints.push_back(value);
                    }
                }
                else
                {
                    appendWarning(result, sourceName, lineNumber,
                        "unknown status key '" + key + "' was ignored.");
                }
                break;

            case Section::Menu:
                if (key == "item")
                {
                    addMenuItemFromValue(result, sourceName, lineNumber, value);
                }
                else
                {
                    appendWarning(result, sourceName, lineNumber,
                        "unknown menu key '" + key + "' was ignored.");
                }
                break;

            case Section::None:
                appendWarning(result, sourceName, lineNumber,
                    "key outside a known section was ignored.");
                break;
            }
        }

        if (result.config.menuItems.empty())
        {
            result.errors.push_back(sourceName + ": no valid menu items were found.");
        }

        return result;
    }
}