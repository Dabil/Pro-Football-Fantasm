#pragma once

#include <cstdint>

/*
    Purpose

    Defines shared Unicode text metadata used by:
        - ScreenCell
        - ScreenBuffer
        - renderer backends

    This file is intentionally small and platform-neutral.

    It does not perform:
        - text layout
        - grapheme segmentation
        - width calculation
        - encoding conversion
        - rendering

    It only defines the common metadata needed to describe
    logical text cells and backend text support.
*/

enum class CellKind : std::uint8_t
{
    Empty = 0,
    Glyph,
    WideTrailing,
    CombiningContinuation
};

enum class CellWidth : std::uint8_t
{
    Zero = 0,
    One = 1,
    Two = 2
};

struct TextBackendCapabilities
{
    bool supportsUtf16Output = true;
    bool supportsCombiningMarks = false;
    bool supportsEastAsianWide = true;
    bool supportsEmoji = false;
    bool supportsFontFallback = false;
};