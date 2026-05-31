#pragma once

#include <vector>

#include "Core/Rect.h"

namespace Rendering
{
    class InvalidationRegion
    {
    public:
        InvalidationRegion();

        // Invalidates the entire render target/page.
        void invalidateAll();

        // Invalidates a specific rectangular region.
        void invalidateRect(const Rect& rect);

        // Clears all invalidation state.
        void clear();

        // Returns true if no invalidation is pending.
        bool isEmpty() const;

        // Returns true if the entire page is invalidated.
        bool isWholePageInvalidated() const;

        // Access dirty regions.
        const std::vector<Rect>& dirtyRegions() const;

    private:
        // Attempts simple safe region coalescing.
        void tryMergeRegion(const Rect& rect);

    private:
        bool m_wholePageInvalidated;
        std::vector<Rect> m_dirtyRegions;
    };
}