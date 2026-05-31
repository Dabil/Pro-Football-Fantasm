#pragma once

/*
    TODO:

    Checklist:
        - Add a Unicode-aware frame style enum later if

    Too early to know what enums would be needed for Unicode, 
    will wait till I actually have a need instead of inventing
    something I would just need to rewrite later.
*/

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

enum class Orientation
{
    Horizontal,
    Vertical
};

enum class Anchor
{
    TopLeft,
    TopCenter,
    TopRight,

    CenterLeft,
    Center,
    CenterRight,

    BottomLeft,
    BottomCenter,
    BottomRight
};