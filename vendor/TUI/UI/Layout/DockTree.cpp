#include "UI/Layout/DockTree.h"

#include <algorithm>
#include <utility>

namespace
{
    int dockZoneThickness(int length)
    {
        if (length <= 0)
        {
            return 0;
        }

        return std::max(1, std::min(6, length / 4));
    }

    void appendSnapZonesForNode(
        const UI::DockNode& node,
        std::vector<UI::DockSnapZone>& zones)
    {
        const Rect bounds = node.bounds();

        if (bounds.size.width <= 0 || bounds.size.height <= 0)
        {
            return;
        }

        if (node.isLeaf() || node.isEmpty())
        {
            const int horizontalThickness = dockZoneThickness(bounds.size.width);
            const int verticalThickness = dockZoneThickness(bounds.size.height);

            zones.push_back({ UI::DockSnapZoneType::Left,
                Rect{ bounds.position, Size{ horizontalThickness, bounds.size.height } },
                node.id() });

            zones.push_back({ UI::DockSnapZoneType::Right,
                Rect{ Point{ bounds.position.x + bounds.size.width - horizontalThickness, bounds.position.y },
                Size{ horizontalThickness, bounds.size.height } },
                node.id() });

            zones.push_back({ UI::DockSnapZoneType::Top,
                Rect{ bounds.position, Size{ bounds.size.width, verticalThickness } },
                node.id() });

            zones.push_back({ UI::DockSnapZoneType::Bottom,
                Rect{ Point{ bounds.position.x, bounds.position.y + bounds.size.height - verticalThickness },
                Size{ bounds.size.width, verticalThickness } },
                node.id() });

            const int centerWidth = std::max(1, bounds.size.width / 3);
            const int centerHeight = std::max(1, bounds.size.height / 3);

            zones.push_back({ UI::DockSnapZoneType::Center,
                Rect{
                    Point{
                        bounds.position.x + (bounds.size.width - centerWidth) / 2,
                        bounds.position.y + (bounds.size.height - centerHeight) / 2
                    },
                    Size{ centerWidth, centerHeight }
                },
                node.id() });
        }

        if (const UI::DockNode* first = node.firstChild())
        {
            appendSnapZonesForNode(*first, zones);
        }

        if (const UI::DockNode* second = node.secondChild())
        {
            appendSnapZonesForNode(*second, zones);
        }
    }

}

std::vector<UI::DockSnapZone> UI::DockTree::snapZones() const
{
    std::vector<DockSnapZone> zones;

    if (m_root)
    {
        appendSnapZonesForNode(*m_root, zones);
    }

    return zones;
}

UI::DockSnapZone UI::DockTree::snapZoneAt(Point screenPosition) const
{
    const std::vector<DockSnapZone> zones = snapZones();

    for (const DockSnapZone& zone : zones)
    {
        if (zone.isValid() && zone.bounds.contains(screenPosition.x, screenPosition.y))
        {
            return zone;
        }
    }

    return DockSnapZone{};
}

namespace UI
{
    DockTree::DockTree()
        : m_root(makeNode())
    {
    }

    DockNode* DockTree::root()
    {
        return m_root.get();
    }

    const DockNode* DockTree::root() const
    {
        return m_root.get();
    }

    bool DockTree::empty() const
    {
        return !m_root || m_root->isEmpty();
    }

    void DockTree::clear()
    {
        m_root = makeNode();
        m_root->setBounds(m_bounds);
    }

    const Rect& DockTree::bounds() const
    {
        return m_bounds;
    }

    void DockTree::setBounds(const Rect& bounds)
    {
        m_bounds = bounds;

        if (m_root)
        {
            m_root->setBounds(bounds);
        }
    }

    DockNode* DockTree::findNode(int nodeId)
    {
        return m_root ? m_root->findNode(nodeId) : nullptr;
    }

    const DockNode* DockTree::findNode(int nodeId) const
    {
        return m_root ? m_root->findNode(nodeId) : nullptr;
    }

    DockNode* DockTree::findContent(const std::string& contentId)
    {
        return m_root ? m_root->findContent(contentId) : nullptr;
    }

    const DockNode* DockTree::findContent(const std::string& contentId) const
    {
        return m_root ? m_root->findContent(contentId) : nullptr;
    }

    int DockTree::attachRoot(DockContentDescriptor content)
    {
        if (!m_root)
        {
            m_root = makeNode();
            m_root->setBounds(m_bounds);
        }

        m_root->attachContent(std::move(content));
        return m_root->id();
    }

    bool DockTree::splitNode(
        int nodeId,
        DockSplitOrientation orientation,
        float ratio,
        DockContentDescriptor newContent,
        bool newContentInFirstChild)
    {
        DockNode* target = findNode(nodeId);
        if (!target || !target->isLeaf())
        {
            return false;
        }

        DockContentDescriptor existingContent = target->detachContent();

        std::unique_ptr<DockNode> first = makeNode();
        std::unique_ptr<DockNode> second = makeNode();

        if (newContentInFirstChild)
        {
            first->attachContent(std::move(newContent));
            second->attachContent(std::move(existingContent));
        }
        else
        {
            first->attachContent(std::move(existingContent));
            second->attachContent(std::move(newContent));
        }

        return target->split(
            orientation,
            ratio,
            std::move(first),
            std::move(second));
    }

    DockContentDescriptor DockTree::detachContent(const std::string& contentId)
    {
        DockNode* node = findContent(contentId);
        if (!node)
        {
            return DockContentDescriptor{};
        }

        return node->detachContent();
    }

    std::vector<DockLayoutRecord> DockTree::createLayoutRecords() const
    {
        std::vector<DockLayoutRecord> records;

        if (m_root)
        {
            m_root->collectLayoutRecords(records);
        }

        return records;
    }

    int DockTree::allocateNodeId()
    {
        return m_nextNodeId++;
    }

    std::unique_ptr<DockNode> DockTree::makeNode()
    {
        return std::make_unique<DockNode>(allocateNodeId());
    }

    bool DockTree::dockContentBeside(
        const std::string& targetContentId,
        const std::string& draggedContentId,
        DockSnapZoneType side)
    {
        if (targetContentId.empty() || draggedContentId.empty())
        {
            return false;
        }

        if (targetContentId == draggedContentId)
        {
            return false;
        }

        if (side != DockSnapZoneType::Top &&
            side != DockSnapZoneType::Bottom &&
            side != DockSnapZoneType::Left &&
            side != DockSnapZoneType::Right)
        {
            return false;
        }

        DockNode* targetNode = findContent(targetContentId);
        if (targetNode == nullptr || !targetNode->isLeaf())
        {
            return false;
        }

        if (findContent(draggedContentId) != nullptr)
        {
            return false;
        }

        const DockSplitOrientation orientation =
            (side == DockSnapZoneType::Left || side == DockSnapZoneType::Right)
            ? DockSplitOrientation::Horizontal
            : DockSplitOrientation::Vertical;

        const bool draggedContentInFirstChild =
            side == DockSnapZoneType::Top ||
            side == DockSnapZoneType::Left;

        DockContentDescriptor draggedContent{};
        draggedContent.contentId = draggedContentId;
        draggedContent.title = draggedContentId;

        return splitNode(
            targetNode->id(),
            orientation,
            0.5f,
            std::move(draggedContent),
            draggedContentInFirstChild);
    }
}