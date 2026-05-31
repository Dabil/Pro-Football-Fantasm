// UI/Layout/DockDragPreview.h
#pragma once

#include <vector>

#include "Core/Point.h"
#include "Rendering/Surface.h"
#include "UI/Layout/DockTypes.h"

namespace UI
{
    class DockTree;

    struct DockDragPreviewState
    {
        bool active = false;
        Point pointerPosition{};
        DockSnapZone activeZone{};
        std::vector<DockSnapZone> candidateZones;

        void clear()
        {
            active = false;
            pointerPosition = Point{};
            activeZone = DockSnapZone{};
            candidateZones.clear();
        }
    };

    class DockDragPreview
    {
    public:
        void begin(const DockTree& dockTree, Point pointerPosition);
        void begin(const std::vector<DockSnapZone>& zones, Point pointerPosition);
        void update(const DockTree& dockTree, Point pointerPosition);
        void update(const std::vector<DockSnapZone>& zones, Point pointerPosition);
        void end();
        void cancel();

        bool isActive() const;
        const DockDragPreviewState& state() const;

        void draw(Surface& surface) const;

    private:
        static DockSnapZone findActiveZone(
            const std::vector<DockSnapZone>& zones,
            Point pointerPosition);

    private:
        DockDragPreviewState m_state;
    };
}