#include "App/ScreenRouteRegistry.h"

#include "Screens/Screen.h"

#include <algorithm>
#include <utility>

bool ScreenRouteRegistry::registerRoute(std::string routeId, ScreenFactory factory)
{
    if (routeId.empty() || !factory)
    {
        return false;
    }

    m_factories[std::move(routeId)] = std::move(factory);
    return true;
}

bool ScreenRouteRegistry::unregisterRoute(const std::string& routeId)
{
    return m_factories.erase(routeId) > 0;
}

bool ScreenRouteRegistry::hasRoute(const std::string& routeId) const
{
    return m_factories.find(routeId) != m_factories.end();
}

std::unique_ptr<Screen> ScreenRouteRegistry::createScreen(const std::string& routeId) const
{
    const auto it = m_factories.find(routeId);

    if (it == m_factories.end() || !it->second)
    {
        return nullptr;
    }

    return it->second();
}

std::vector<std::string> ScreenRouteRegistry::registeredRouteIds() const
{
    std::vector<std::string> ids;
    ids.reserve(m_factories.size());

    for (const auto& pair : m_factories)
    {
        ids.push_back(pair.first);
    }

    std::sort(ids.begin(), ids.end());
    return ids;
}