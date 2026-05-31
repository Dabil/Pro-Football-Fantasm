#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Objects/LayeredTextObject.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/TextObjectFactory.h"
#include "Rendering/Styles/Style.h"

class TextObjectComposer
{
public:
    enum class EntryKind
    {
        Object,
        Text,
        Glyph,
        Frame,
        FilledRect
    };

    struct Entry
    {
        EntryKind kind = EntryKind::Object;
        std::string name;
        int offsetX = 0;
        int offsetY = 0;
        int zIndex = 0;
        bool visible = true;
        std::optional<Composition::WritePolicy> writePolicy;
        TextObject object;
    };

    struct BuildTextObjectOptions
    {
        bool visibleOnly = true;
        std::optional<Style> overrideStyle;

        // Fallback policy used when an entry does not specify its own write policy.
        // Default is authoredOnly() so composed TextObjects preserve authored cells
        // while keeping Empty cells transparent.
        Composition::WritePolicy writePolicy = Composition::WritePresets::authoredObject();
    };

    struct BuildLayeredTextObjectOptions
    {
        bool includeHidden = true;
    };

public:
    TextObjectComposer() = default;

    void clear();

    bool isEmpty() const;
    std::size_t getEntryCount() const;

    const std::vector<Entry>& getEntries() const;

    TextObjectComposer& addObject(
        const TextObject& object,
        int x,
        int y,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addObject(
        const TextObject& object,
        int x,
        int y,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addSolidObject(
        const TextObject& object,
        int x,
        int y,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addVisibleObject(
        const TextObject& object,
        int x,
        int y,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addLayeredObject(
        const Rendering::LayeredTextObject& object,
        int x,
        int y,
        int zIndexBias = 0,
        std::string_view namePrefix = {},
        bool visible = true);

    TextObjectComposer& addLayeredObject(
        const Rendering::LayeredTextObject& object,
        int x,
        int y,
        const Composition::WritePolicy& writePolicy,
        int zIndexBias = 0,
        std::string_view namePrefix = {},
        bool visible = true);

    TextObjectComposer& addText(
        std::u32string_view text,
        int x,
        int y,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addText(
        std::u32string_view text,
        int x,
        int y,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addText(
        std::u32string_view text,
        int x,
        int y,
        const Style& style,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addText(
        std::u32string_view text,
        int x,
        int y,
        const Style& style,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addTextUtf8(
        std::string_view text,
        int x,
        int y,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addTextUtf8(
        std::string_view text,
        int x,
        int y,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addTextUtf8(
        std::string_view text,
        int x,
        int y,
        const Style& style,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addTextUtf8(
        std::string_view text,
        int x,
        int y,
        const Style& style,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addGlyph(
        char32_t glyph,
        int x,
        int y,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addGlyph(
        char32_t glyph,
        int x,
        int y,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addGlyph(
        char32_t glyph,
        int x,
        int y,
        const Style& style,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addGlyph(
        char32_t glyph,
        int x,
        int y,
        const Style& style,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFrame(
        int x,
        int y,
        int width,
        int height,
        const ObjectFactory::BorderGlyphs& border = ObjectFactory::asciiBorder(),
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFrame(
        int x,
        int y,
        int width,
        int height,
        const ObjectFactory::BorderGlyphs& border,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFrame(
        int x,
        int y,
        int width,
        int height,
        const Style& style,
        const ObjectFactory::BorderGlyphs& border = ObjectFactory::asciiBorder(),
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFrame(
        int x,
        int y,
        int width,
        int height,
        const Style& style,
        const ObjectFactory::BorderGlyphs& border,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addSolidFrame(
        int x,
        int y,
        int width,
        int height,
        const Style& style,
        const ObjectFactory::BorderGlyphs& border = ObjectFactory::asciiBorder(),
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addVisibleFrame(
        int x,
        int y,
        int width,
        int height,
        const ObjectFactory::BorderGlyphs& border = ObjectFactory::asciiBorder(),
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFilledRect(
        int x,
        int y,
        int width,
        int height,
        char32_t fillGlyph = U' ',
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFilledRect(
        int x,
        int y,
        int width,
        int height,
        char32_t fillGlyph,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFilledRect(
        int x,
        int y,
        int width,
        int height,
        char32_t fillGlyph,
        const Style& style,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObjectComposer& addFilledRect(
        int x,
        int y,
        int width,
        int height,
        char32_t fillGlyph,
        const Style& style,
        const Composition::WritePolicy& writePolicy,
        int zIndex = 0,
        std::string_view name = {},
        bool visible = true);

    TextObject buildTextObject() const;
    TextObject buildTextObject(const BuildTextObjectOptions& options) const;

    Rendering::LayeredTextObject buildLayeredTextObject() const;
    Rendering::LayeredTextObject buildLayeredTextObject(const BuildLayeredTextObjectOptions& options) const;

private:
    struct BuildBounds
    {
        bool valid = false;
        int minX = 0;
        int minY = 0;
        int maxX = 0;
        int maxY = 0;

        int width() const
        {
            return valid ? (maxX - minX) : 0;
        }

        int height() const
        {
            return valid ? (maxY - minY) : 0;
        }
    };

private:
    TextObjectComposer& addResolvedEntry(
        EntryKind kind,
        const TextObject& object,
        int x,
        int y,
        int zIndex,
        std::string_view name,
        bool visible,
        const std::optional<Composition::WritePolicy>& writePolicy);

    std::string makeUniqueEntryName(std::string_view requestedName, EntryKind kind) const;

    static TextObject makeGlyphObject(char32_t glyph, const std::optional<Style>& style);
    static TextObject makeTransparentFrameObject(
        int width,
        int height,
        const ObjectFactory::BorderGlyphs& border,
        const std::optional<Style>& style);
    static BuildBounds computeBuildBounds(
        const std::vector<Entry>& entries,
        bool visibleOnly);
    static std::vector<const Entry*> collectBuildEntries(
        const std::vector<Entry>& entries,
        bool visibleOnly);

private:
    std::vector<Entry> m_entries;
};