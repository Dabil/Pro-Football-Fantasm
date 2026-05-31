#pragma once

#include <string>

#include "Core/Rect.h"

class Widget;

namespace UI
{
    enum class DockNodeKind
    {
        Empty,
        Leaf,
        Split
    };

    enum class DockSplitOrientation
    {
        Horizontal,
        Vertical
    };

    enum class DockSnapZoneType
    {
        None,
        Left,
        Right,
        Top,
        Bottom,
        Center,
        Tab,
        Float
    };

    struct DockContentDescriptor
    {
        std::string contentId;
        std::string title;
        Widget* widget = nullptr;

        bool isAttached() const
        {
            return widget != nullptr || !contentId.empty();
        }
    };

    struct DockLayoutRecord
    {
        int nodeId = 0;
        int parentNodeId = 0;

        DockNodeKind kind = DockNodeKind::Empty;
        DockSplitOrientation splitOrientation = DockSplitOrientation::Horizontal;
        float splitRatio = 0.5f;

        std::string contentId;
        std::string title;

        Rect bounds{};
        bool floating = false;
    };

    struct DockSnapZone
    {
        DockSnapZoneType type = DockSnapZoneType::None;
        Rect bounds{};
        int targetNodeId = 0;
        std::string targetContentId;

        bool isValid() const
        {
            return type != DockSnapZoneType::None
                && (targetNodeId != 0 || !targetContentId.empty());
        }
    };
}