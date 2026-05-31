#pragma once

#include <optional>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

class TextObjectBuilder
{
public:
    TextObjectBuilder() = default;
    TextObjectBuilder(int width, int height);

    void reset(int width, int height);
    void clear();

    bool isInitialized() const;
    int getWidth() const;
    int getHeight() const;

    bool inBounds(int x, int y) const;

    bool setCell(
        int x,
        int y,
        char32_t glyph,
        CellKind kind,
        CellWidth width,
        const std::optional<Style>& style = std::nullopt);

    bool setGlyph(
        int x,
        int y,
        char32_t glyph,
        const std::optional<Style>& style = std::nullopt);

    bool setWideGlyph(
        int x,
        int y,
        char32_t glyph,
        const std::optional<Style>& style = std::nullopt);

    bool setAuthoredSpace(
        int x,
        int y,
        const std::optional<Style>& style = std::nullopt);

    bool setTransparent(
        int x,
        int y,
        const std::optional<Style>& style = std::nullopt);

    void fill(
        char32_t glyph,
        CellKind kind = CellKind::Glyph,
        CellWidth width = CellWidth::One,
        const std::optional<Style>& style = std::nullopt);

    void fillAuthoredSpace(const std::optional<Style>& style = std::nullopt);
    void fillTransparent(const std::optional<Style>& style = std::nullopt);

    TextObject build() const;
    TextObject buildAndReset();

private:
    int index(int x, int y) const;

private:
    TextObject m_object;
};