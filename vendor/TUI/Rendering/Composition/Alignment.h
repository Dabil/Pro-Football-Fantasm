#pragma once

namespace Composition
{
    enum class HorizontalAlign
    {
        Left,
        Center,
        Right
    };

    enum class VerticalAlign
    {
        Top,
        Center,
        Bottom
    };

    struct Alignment
    {
        HorizontalAlign horizontal = HorizontalAlign::Left;
        VerticalAlign vertical = VerticalAlign::Top;
    };

    namespace Align
    {
        inline constexpr Alignment topLeft()
        {
            return { HorizontalAlign::Left, VerticalAlign::Top };
        }

        inline constexpr Alignment topCenter()
        {
            return { HorizontalAlign::Center, VerticalAlign::Top };
        }

        inline constexpr Alignment topRight()
        {
            return { HorizontalAlign::Right, VerticalAlign::Top };
        }

        inline constexpr Alignment centerLeft()
        {
            return { HorizontalAlign::Left, VerticalAlign::Center };
        }

        inline constexpr Alignment center()
        {
            return { HorizontalAlign::Center, VerticalAlign::Center };
        }

        inline constexpr Alignment centerRight()
        {
            return { HorizontalAlign::Right, VerticalAlign::Center };
        }

        inline constexpr Alignment bottomLeft()
        {
            return { HorizontalAlign::Left, VerticalAlign::Bottom };
        }

        inline constexpr Alignment bottomCenter()
        {
            return { HorizontalAlign::Center, VerticalAlign::Bottom };
        }

        inline constexpr Alignment bottomRight()
        {
            return { HorizontalAlign::Right, VerticalAlign::Bottom };
        }
    }
}