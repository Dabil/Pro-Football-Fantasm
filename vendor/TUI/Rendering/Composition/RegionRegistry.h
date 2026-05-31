#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Rendering/Composition/Placement.h"

namespace Composition
{
    class RegionRegistry
    {
    public:
        bool createRegion(int x, int y, int width, int height, std::string_view name);
        bool createRegion(const NamedRegion& region);

        bool hasRegion(std::string_view name) const;

        const NamedRegion* getRegion(std::string_view name) const;
        NamedRegion* getRegion(std::string_view name);

        bool removeRegion(std::string_view name);
        void clearRegions();

        std::size_t getRegionCount() const;
        std::vector<NamedRegion> getAllRegions() const;

    private:
        static std::string normalizeKey(std::string_view name);

    private:
        std::unordered_map<std::string, NamedRegion> m_regions;
    };
}