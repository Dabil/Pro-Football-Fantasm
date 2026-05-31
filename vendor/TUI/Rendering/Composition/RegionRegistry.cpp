#include "Rendering/Composition/RegionRegistry.h"

namespace Composition
{
    bool RegionRegistry::createRegion(
        int x,
        int y,
        int width,
        int height,
        std::string_view name)
    {
        NamedRegion region;
        region.name = std::string(name);
        region.bounds = makeRect(x, y, width, height);
        return createRegion(region);
    }

    bool RegionRegistry::createRegion(const NamedRegion& region)
    {
        const std::string key = normalizeKey(region.name);
        if (key.empty())
        {
            return false;
        }

        m_regions[key] = region;
        return true;
    }

    bool RegionRegistry::hasRegion(std::string_view name) const
    {
        const std::string key = normalizeKey(name);
        return !key.empty() && m_regions.find(key) != m_regions.end();
    }

    const NamedRegion* RegionRegistry::getRegion(std::string_view name) const
    {
        const std::string key = normalizeKey(name);
        const auto it = m_regions.find(key);
        if (it == m_regions.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    NamedRegion* RegionRegistry::getRegion(std::string_view name)
    {
        const std::string key = normalizeKey(name);
        const auto it = m_regions.find(key);
        if (it == m_regions.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    bool RegionRegistry::removeRegion(std::string_view name)
    {
        const std::string key = normalizeKey(name);
        if (key.empty())
        {
            return false;
        }

        return m_regions.erase(key) > 0;
    }

    void RegionRegistry::clearRegions()
    {
        m_regions.clear();
    }

    std::size_t RegionRegistry::getRegionCount() const
    {
        return m_regions.size();
    }

    std::vector<NamedRegion> RegionRegistry::getAllRegions() const
    {
        std::vector<NamedRegion> regions;
        regions.reserve(m_regions.size());

        for (const auto& entry : m_regions)
        {
            regions.push_back(entry.second);
        }

        return regions;
    }

    std::string RegionRegistry::normalizeKey(std::string_view name)
    {
        return std::string(name);
    }
}