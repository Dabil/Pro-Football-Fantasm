#include "UI/Layout/DockDragPreview.h"

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "UI/Layout/DockTree.h"

namespace UI
{
    void DockDragPreview::begin(const DockTree& dockTree, Point pointerPosition)
    {
        m_state.active = true;
        update(dockTree, pointerPosition);
    }

    void DockDragPreview::begin(
        const std::vector<DockSnapZone>& zones,
        Point pointerPosition)
    {
        m_state.active = true;
        update(zones, pointerPosition);
    }

    void DockDragPreview::update(const DockTree& dockTree, Point pointerPosition)
    {
        if (!m_state.active)
        {
            return;
        }

        m_state.pointerPosition = pointerPosition;
        m_state.candidateZones = dockTree.snapZones();
        m_state.activeZone = findActiveZone(m_state.candidateZones, pointerPosition);
    }

    void DockDragPreview::update(
        const std::vector<DockSnapZone>& zones,
        Point pointerPosition)
    {
        if (!m_state.active)
        {
            return;
        }

        m_state.pointerPosition = pointerPosition;
        m_state.candidateZones = zones;
        m_state.activeZone = findActiveZone(m_state.candidateZones, pointerPosition);
    } 
    
    void DockDragPreview::end()
    {
        m_state.clear();
    }

    void DockDragPreview::cancel()
    {
        m_state.clear();
    }

    bool DockDragPreview::isActive() const
    {
        return m_state.active;
    }

    const DockDragPreviewState& DockDragPreview::state() const
    {
        return m_state;
    }

    void DockDragPreview::draw(Surface& surface) const
    {
        if (!m_state.active || !m_state.activeZone.isValid())
        {
            return;
        }

        ScreenBuffer& buffer = surface.buffer();

        const Rect preview = m_state.activeZone.bounds;
        if (preview.size.width <= 0 || preview.size.height <= 0)
        {
            return;
        }

        const Style previewStyle =
            style::Fg(Color::FromBasic(Color::Basic::BrightCyan))
            + style::Bg(Color::FromBasic(Color::Basic::Blue));

        buffer.fillRect(preview, U'.', previewStyle);

        buffer.drawFrame(
            preview,
            previewStyle,
            U'+',
            U'+',
            U'+',
            U'+',
            U'=',
            U'|');
    }

    DockSnapZone DockDragPreview::findActiveZone(
        const std::vector<DockSnapZone>& zones,
        Point pointerPosition)
    {
        for (const DockSnapZone& zone : zones)
        {
            if (zone.isValid() &&
                zone.bounds.contains(pointerPosition.x, pointerPosition.y))
            {
                return zone;
            }
        }

        return DockSnapZone{};
    }
}