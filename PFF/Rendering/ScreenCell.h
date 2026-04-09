#pragma once

#include <cstdint>

#include "Rendering/Styles/Style.h"
#include "Rendering/Text/TextTypes.h"

/*
    Purpose:

    Represents one logical cell in the screen buffer.

    For Phase 1 Unicode readiness, a cell carries enough metadata
    to distinguish:
        - normal visible glyph cells
        - wide trailing cells
        - combining continuation cells
        - empty cells

    This version also adds lightweight generic extension metadata
    for future rendering/composition features such as priority and flags,
    without changing the current ScreenCell role as a small value type.
*/

enum class ScreenCellFlags : std::uint32_t
{
    None = 0u
};

inline constexpr ScreenCellFlags operator|(ScreenCellFlags lhs, ScreenCellFlags rhs)
{
    return static_cast<ScreenCellFlags>(
        static_cast<std::uint32_t>(lhs) |
        static_cast<std::uint32_t>(rhs));
}

inline constexpr ScreenCellFlags operator&(ScreenCellFlags lhs, ScreenCellFlags rhs)
{
    return static_cast<ScreenCellFlags>(
        static_cast<std::uint32_t>(lhs) &
        static_cast<std::uint32_t>(rhs));
}

inline constexpr ScreenCellFlags operator^(ScreenCellFlags lhs, ScreenCellFlags rhs)
{
    return static_cast<ScreenCellFlags>(
        static_cast<std::uint32_t>(lhs) ^
        static_cast<std::uint32_t>(rhs));
}

inline constexpr ScreenCellFlags operator~(ScreenCellFlags value)
{
    return static_cast<ScreenCellFlags>(~static_cast<std::uint32_t>(value));
}

inline ScreenCellFlags& operator|=(ScreenCellFlags& lhs, ScreenCellFlags rhs)
{
    lhs = lhs | rhs;
    return lhs;
}

inline ScreenCellFlags& operator&=(ScreenCellFlags& lhs, ScreenCellFlags rhs)
{
    lhs = lhs & rhs;
    return lhs;
}

inline ScreenCellFlags& operator^=(ScreenCellFlags& lhs, ScreenCellFlags rhs)
{
    lhs = lhs ^ rhs;
    return lhs;
}

struct ScreenCellMetadata
{
    int priority = 0;
    ScreenCellFlags flags = ScreenCellFlags::None;

    bool hasPriority() const
    {
        return priority != 0;
    }

    bool hasFlags() const
    {
        return flags != ScreenCellFlags::None;
    }

    bool hasFlag(ScreenCellFlags flag) const
    {
        return (flags & flag) == flag;
    }

    void setFlag(ScreenCellFlags flag)
    {
        flags |= flag;
    }

    void clearFlag(ScreenCellFlags flag)
    {
        flags &= ~flag;
    }

    bool isDefault() const
    {
        return priority == 0 &&
            flags == ScreenCellFlags::None;
    }

    bool operator==(const ScreenCellMetadata& other) const
    {
        return priority == other.priority &&
            flags == other.flags;
    }

    bool operator!=(const ScreenCellMetadata& other) const
    {
        return !(*this == other);
    }
};

struct ScreenCell
{
    char32_t glyph = U' ';
    Style style{};
    CellKind kind = CellKind::Empty;
    CellWidth width = CellWidth::One;
    ScreenCellMetadata metadata{};

    bool isEmpty() const
    {
        return kind == CellKind::Empty;
    }

    bool isGlyph() const
    {
        return kind == CellKind::Glyph;
    }

    bool hasVisibleGlyph() const
    {
        return kind == CellKind::Glyph;
    }

    bool isWideTrailing() const
    {
        return kind == CellKind::WideTrailing;
    }

    bool isCombiningContinuation() const
    {
        return kind == CellKind::CombiningContinuation;
    }

    bool isContinuation() const
    {
        return kind == CellKind::WideTrailing ||
            kind == CellKind::CombiningContinuation;
    }

    bool isLeadingCell() const
    {
        return kind == CellKind::Glyph;
    }

    bool isDoubleWidthLeading() const
    {
        return kind == CellKind::Glyph &&
            width == CellWidth::Two;
    }

    bool hasStyle() const
    {
        return !style.isEmpty();
    }

    bool hasMetadata() const
    {
        return !metadata.isDefault();
    }

    bool hasPriority() const
    {
        return metadata.hasPriority();
    }

    int priority() const
    {
        return metadata.priority;
    }

    void setPriority(int value)
    {
        metadata.priority = value;
    }

    bool hasFlags() const
    {
        return metadata.hasFlags();
    }

    bool hasFlag(ScreenCellFlags flag) const
    {
        return metadata.hasFlag(flag);
    }

    void setFlag(ScreenCellFlags flag)
    {
        metadata.setFlag(flag);
    }

    void clearFlag(ScreenCellFlags flag)
    {
        metadata.clearFlag(flag);
    }

    void clearMetadata()
    {
        metadata = ScreenCellMetadata{};
    }

    bool operator==(const ScreenCell& other) const
    {
        return glyph == other.glyph &&
            style == other.style &&
            kind == other.kind &&
            width == other.width &&
            metadata == other.metadata;
    }

    bool operator!=(const ScreenCell& other) const
    {
        return !(*this == other);
    }
};