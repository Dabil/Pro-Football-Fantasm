#include "UI/Layout/DockNode.h"

#include <algorithm>

namespace UI
{
    DockNode::DockNode(int nodeId)
        : m_id(nodeId)
    {
    }

    int DockNode::id() const
    {
        return m_id;
    }

    int DockNode::parentId() const
    {
        return m_parentId;
    }

    void DockNode::setParentId(int parentId)
    {
        m_parentId = parentId;
    }

    DockNodeKind DockNode::kind() const
    {
        return m_kind;
    }

    bool DockNode::isEmpty() const
    {
        return m_kind == DockNodeKind::Empty;
    }

    bool DockNode::isLeaf() const
    {
        return m_kind == DockNodeKind::Leaf;
    }

    bool DockNode::isSplit() const
    {
        return m_kind == DockNodeKind::Split;
    }

    const Rect& DockNode::bounds() const
    {
        return m_bounds;
    }

    void DockNode::setBounds(const Rect& bounds)
    {
        m_bounds = bounds;
        updateChildBounds();
    }

    const DockContentDescriptor& DockNode::content() const
    {
        return m_content;
    }

    DockContentDescriptor DockNode::detachContent()
    {
        DockContentDescriptor detached = m_content;
        m_content = DockContentDescriptor{};

        if (m_kind == DockNodeKind::Leaf)
        {
            m_kind = DockNodeKind::Empty;
        }

        return detached;
    }

    void DockNode::attachContent(DockContentDescriptor content)
    {
        clear();

        m_content = std::move(content);
        m_kind = m_content.isAttached()
            ? DockNodeKind::Leaf
            : DockNodeKind::Empty;
    }

    DockSplitOrientation DockNode::splitOrientation() const
    {
        return m_splitOrientation;
    }

    float DockNode::splitRatio() const
    {
        return m_splitRatio;
    }

    DockNode* DockNode::firstChild()
    {
        return m_firstChild.get();
    }

    const DockNode* DockNode::firstChild() const
    {
        return m_firstChild.get();
    }

    DockNode* DockNode::secondChild()
    {
        return m_secondChild.get();
    }

    const DockNode* DockNode::secondChild() const
    {
        return m_secondChild.get();
    }

    bool DockNode::split(
        DockSplitOrientation orientation,
        float ratio,
        std::unique_ptr<DockNode> first,
        std::unique_ptr<DockNode> second)
    {
        if (!first || !second)
        {
            return false;
        }

        m_content = DockContentDescriptor{};
        m_kind = DockNodeKind::Split;
        m_splitOrientation = orientation;
        m_splitRatio = clampRatio(ratio);

        m_firstChild = std::move(first);
        m_secondChild = std::move(second);

        m_firstChild->setParentId(m_id);
        m_secondChild->setParentId(m_id);

        updateChildBounds();
        return true;
    }

    void DockNode::clear()
    {
        m_kind = DockNodeKind::Empty;
        m_content = DockContentDescriptor{};
        m_firstChild.reset();
        m_secondChild.reset();
        m_splitOrientation = DockSplitOrientation::Horizontal;
        m_splitRatio = 0.5f;
    }

    DockNode* DockNode::findNode(int nodeId)
    {
        if (m_id == nodeId)
        {
            return this;
        }

        if (m_firstChild)
        {
            if (DockNode* found = m_firstChild->findNode(nodeId))
            {
                return found;
            }
        }

        if (m_secondChild)
        {
            if (DockNode* found = m_secondChild->findNode(nodeId))
            {
                return found;
            }
        }

        return nullptr;
    }

    const DockNode* DockNode::findNode(int nodeId) const
    {
        if (m_id == nodeId)
        {
            return this;
        }

        if (m_firstChild)
        {
            if (const DockNode* found = m_firstChild->findNode(nodeId))
            {
                return found;
            }
        }

        if (m_secondChild)
        {
            if (const DockNode* found = m_secondChild->findNode(nodeId))
            {
                return found;
            }
        }

        return nullptr;
    }

    DockNode* DockNode::findContent(const std::string& contentId)
    {
        if (!contentId.empty() && m_content.contentId == contentId)
        {
            return this;
        }

        if (m_firstChild)
        {
            if (DockNode* found = m_firstChild->findContent(contentId))
            {
                return found;
            }
        }

        if (m_secondChild)
        {
            if (DockNode* found = m_secondChild->findContent(contentId))
            {
                return found;
            }
        }

        return nullptr;
    }

    const DockNode* DockNode::findContent(const std::string& contentId) const
    {
        if (!contentId.empty() && m_content.contentId == contentId)
        {
            return this;
        }

        if (m_firstChild)
        {
            if (const DockNode* found = m_firstChild->findContent(contentId))
            {
                return found;
            }
        }

        if (m_secondChild)
        {
            if (const DockNode* found = m_secondChild->findContent(contentId))
            {
                return found;
            }
        }

        return nullptr;
    }

    void DockNode::collectLayoutRecords(std::vector<DockLayoutRecord>& records) const
    {
        DockLayoutRecord record;
        record.nodeId = m_id;
        record.parentNodeId = m_parentId;
        record.kind = m_kind;
        record.splitOrientation = m_splitOrientation;
        record.splitRatio = m_splitRatio;
        record.contentId = m_content.contentId;
        record.title = m_content.title;
        record.bounds = m_bounds;

        records.push_back(record);

        if (m_firstChild)
        {
            m_firstChild->collectLayoutRecords(records);
        }

        if (m_secondChild)
        {
            m_secondChild->collectLayoutRecords(records);
        }
    }

    float DockNode::clampRatio(float ratio)
    {
        return std::max(0.05f, std::min(0.95f, ratio));
    }

    void DockNode::updateChildBounds()
    {
        if (!isSplit() || !m_firstChild || !m_secondChild)
        {
            return;
        }

        Rect firstBounds = m_bounds;
        Rect secondBounds = m_bounds;

        if (m_splitOrientation == DockSplitOrientation::Horizontal)
        {
            const int firstWidth = std::max(0, static_cast<int>(m_bounds.size.width * m_splitRatio));

            firstBounds.size.width = firstWidth;

            secondBounds.position.x = m_bounds.position.x + firstWidth;
            secondBounds.size.width = std::max(0, m_bounds.size.width - firstWidth);
        }
        else
        {
            const int firstHeight = std::max(0, static_cast<int>(m_bounds.size.height * m_splitRatio));

            firstBounds.size.height = firstHeight;

            secondBounds.position.y = m_bounds.position.y + firstHeight;
            secondBounds.size.height = std::max(0, m_bounds.size.height - firstHeight);
        }

        m_firstChild->setBounds(firstBounds);
        m_secondChild->setBounds(secondBounds);
    }
}