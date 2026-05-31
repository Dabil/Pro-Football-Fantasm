#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Core/Rect.h"
#include "UI/Layout/DockTypes.h"

namespace UI
{
    class DockNode
    {
    public:
        explicit DockNode(int nodeId);

        DockNode(const DockNode&) = delete;
        DockNode& operator=(const DockNode&) = delete;

        DockNode(DockNode&&) noexcept = default;
        DockNode& operator=(DockNode&&) noexcept = default;

        int id() const;
        int parentId() const;
        void setParentId(int parentId);

        DockNodeKind kind() const;
        bool isEmpty() const;
        bool isLeaf() const;
        bool isSplit() const;

        const Rect& bounds() const;
        void setBounds(const Rect& bounds);

        const DockContentDescriptor& content() const;
        DockContentDescriptor detachContent();
        void attachContent(DockContentDescriptor content);

        DockSplitOrientation splitOrientation() const;
        float splitRatio() const;

        DockNode* firstChild();
        const DockNode* firstChild() const;

        DockNode* secondChild();
        const DockNode* secondChild() const;

        bool split(
            DockSplitOrientation orientation,
            float ratio,
            std::unique_ptr<DockNode> first,
            std::unique_ptr<DockNode> second);

        void clear();

        DockNode* findNode(int nodeId);
        const DockNode* findNode(int nodeId) const;

        DockNode* findContent(const std::string& contentId);
        const DockNode* findContent(const std::string& contentId) const;

        void collectLayoutRecords(std::vector<DockLayoutRecord>& records) const;

    private:
        static float clampRatio(float ratio);
        void updateChildBounds();

    private:
        int m_id = 0;
        int m_parentId = 0;

        DockNodeKind m_kind = DockNodeKind::Empty;
        Rect m_bounds{};

        DockContentDescriptor m_content{};

        DockSplitOrientation m_splitOrientation = DockSplitOrientation::Horizontal;
        float m_splitRatio = 0.5f;

        std::unique_ptr<DockNode> m_firstChild;
        std::unique_ptr<DockNode> m_secondChild;
    };
}