#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Screen;

class ScreenRouteRegistry
{
public:
    using ScreenFactory = std::function<std::unique_ptr<Screen>()>;

    bool registerRoute(std::string routeId, ScreenFactory factory);
    bool unregisterRoute(const std::string& routeId);
    bool hasRoute(const std::string& routeId) const;

    std::unique_ptr<Screen> createScreen(const std::string& routeId) const;

    std::vector<std::string> registeredRouteIds() const;

private:
    std::unordered_map<std::string, ScreenFactory> m_factories;
};