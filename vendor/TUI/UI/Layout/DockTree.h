#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/Point.h"
#include "Core/Rect.h"
#include "UI/Layout/DockNode.h"
#include "UI/Layout/DockTypes.h"

namespace UI
{
    class DockTree
    {
    public:
        DockTree();

        DockNode* root();
        const DockNode* root() const;

        bool empty() const;
        void clear();

        const Rect& bounds() const;
        void setBounds(const Rect& bounds);

        DockNode* findNode(int nodeId);
        const DockNode* findNode(int nodeId) const;

        DockNode* findContent(const std::string& contentId);
        const DockNode* findContent(const std::string& contentId) const;

        int attachRoot(DockContentDescriptor content);

        bool splitNode(
            int nodeId,
            DockSplitOrientation orientation,
            float ratio,
            DockContentDescriptor newContent,
            bool newContentInFirstChild = false);

        DockContentDescriptor detachContent(const std::string& contentId);

        std::vector<DockLayoutRecord> createLayoutRecords() const;

        std::vector<DockSnapZone> snapZones() const;
        DockSnapZone snapZoneAt(Point screenPosition) const;

        bool dockContentBeside(
            const std::string& targetContentId,
            const std::string& draggedContentId,
            DockSnapZoneType side);

    private:
        int allocateNodeId();
        std::unique_ptr<DockNode> makeNode();

    private:
        Rect m_bounds{};
        int m_nextNodeId = 1;
        std::unique_ptr<DockNode> m_root;
    };
}