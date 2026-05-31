#include "Rendering/InvalidationRegion.h"

#include <algorithm>

namespace Rendering
{
    namespace
    {
        bool intersectsOrTouches(const Rect& a, const Rect& b)
        {
            return !(
                a.position.x + a.size.width  < b.position.x ||
                b.position.x + b.size.width  < a.position.x ||
                a.position.y + a.size.height < b.position.y ||
                b.position.y + b.size.height < a.position.y
                );
        }

        Rect unionRect(const Rect& a, const Rect& b)
        {
            const int left   = std::min(a.position.x, b.position.x);
            const int top    = std::min(a.position.y, b.position.y);
            const int right  = std::max(a.position.x + a.size.width, b.position.x + b.size.width);
            const int bottom = std::max(a.position.y + a.size.height, b.position.y + b.size.height);

            return Rect{ Point{left,top}, Size{right - left, bottom - top} };
        }

        bool isValidRect(const Rect& rect)
        {
            return rect.size.width > 0 && rect.size.height > 0;
        }
    }

    InvalidationRegion::InvalidationRegion()
        : m_wholePageInvalidated(false)
    {
    }

    void InvalidationRegion::invalidateAll()
    {
        m_wholePageInvalidated = true;
        m_dirtyRegions.clear();
    }

    void InvalidationRegion::invalidateRect(const Rect& rect)
    {
        if (m_wholePageInvalidated)
        {
            return;
        }

        if (!isValidRect(rect))
        {
            return;
        }

        tryMergeRegion(rect);
    }

    void InvalidationRegion::clear()
    {
        m_wholePageInvalidated = false;
        m_dirtyRegions.clear();
    }

    bool InvalidationRegion::isEmpty() const
    {
        return !m_wholePageInvalidated
            && m_dirtyRegions.empty();
    }

    bool InvalidationRegion::isWholePageInvalidated() const
    {
        return m_wholePageInvalidated;
    }

    const std::vector<Rect>& InvalidationRegion::dirtyRegions() const
    {
        return m_dirtyRegions;
    }

    void InvalidationRegion::tryMergeRegion(const Rect& rect)
    {
        Rect mergedRect = rect;

        bool merged = true;

        while (merged)
        {
            merged = false;

            for (auto it = m_dirtyRegions.begin();
                it != m_dirtyRegions.end();)
            {
                if (intersectsOrTouches(*it, mergedRect))
                {
                    mergedRect = unionRect(*it, mergedRect);
                    it = m_dirtyRegions.erase(it);
                    merged = true;
                }
                else
                {
                    ++it;
                }
            }
        }

        m_dirtyRegions.push_back(mergedRect);
    }
}