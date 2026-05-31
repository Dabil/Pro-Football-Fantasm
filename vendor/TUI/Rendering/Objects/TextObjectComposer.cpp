#include "Rendering/Objects/TextObjectComposer.h"

#include <algorithm>
#include <utility>

#include "Rendering/Objects/TextObjectBlitter.h"
#include "Rendering/Objects/TextObjectBuilder.h"
#include "Utilities/Unicode/UnicodeWidth.h"

namespace
{
    const char* kindBaseName(TextObjectComposer::EntryKind kind)
    {
        switch (kind)
        {
        case TextObjectComposer::EntryKind::Text:
            return "text";

        case TextObjectComposer::EntryKind::Glyph:
            return "glyph";

        case TextObjectComposer::EntryKind::Frame:
            return "frame";

        case TextObjectComposer::EntryKind::FilledRect:
            return "filledRect";

        case TextObjectComposer::EntryKind::Object:
        default:
            return "object";
        }
    }

    bool entryParticipatesInBuild(const TextObjectComposer::Entry& entry, bool visibleOnly)
    {
        if (entry.object.isEmpty())
        {
            return false;
        }

        if (visibleOnly && !entry.visible)
        {
            return false;
        }

        return true;
    }

    std::string makeLayerEntryBaseName(
        std::string_view namePrefix,
        const Rendering::TextObjectLayer& layer)
    {
        if (!namePrefix.empty())
        {
            std::string result(namePrefix);
            result += "_";
            result += layer.name.empty() ? "layer" : layer.name;
            return result;
        }

        if (!layer.name.empty())
        {
            return layer.name;
        }

        return "layer";
    }
}

void TextObjectComposer::clear()
{
    m_entries.clear();
}

bool TextObjectComposer::isEmpty() const
{
    return m_entries.empty();
}

std::size_t TextObjectComposer::getEntryCount() const
{
    return m_entries.size();
}

const std::vector<TextObjectComposer::Entry>& TextObjectComposer::getEntries() const
{
    return m_entries;
}

TextObjectComposer& TextObjectComposer::addObject(
    const TextObject& object,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Object,
        object,
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addObject(
    const TextObject& object,
    int x,
    int y,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Object,
        object,
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addSolidObject(
    const TextObject& object,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addObject(
        object,
        x,
        y,
        Composition::WritePresets::solidObject(),
        zIndex,
        name,
        visible);
}

TextObjectComposer& TextObjectComposer::addVisibleObject(
    const TextObject& object,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addObject(
        object,
        x,
        y,
        Composition::WritePresets::visibleObject(),
        zIndex,
        name,
        visible);
}

TextObjectComposer& TextObjectComposer::addLayeredObject(
    const Rendering::LayeredTextObject& object,
    int x,
    int y,
    int zIndexBias,
    std::string_view namePrefix,
    bool visible)
{
    for (const Rendering::TextObjectLayer& layer : object.getLayers())
    {
        addResolvedEntry(
            EntryKind::Object,
            layer.object,
            x + layer.offsetX,
            y + layer.offsetY,
            zIndexBias + layer.zIndex,
            makeLayerEntryBaseName(namePrefix, layer),
            visible && layer.visible,
            std::nullopt);
    }

    return *this;
}

TextObjectComposer& TextObjectComposer::addLayeredObject(
    const Rendering::LayeredTextObject& object,
    int x,
    int y,
    const Composition::WritePolicy& writePolicy,
    int zIndexBias,
    std::string_view namePrefix,
    bool visible)
{
    for (const Rendering::TextObjectLayer& layer : object.getLayers())
    {
        addResolvedEntry(
            EntryKind::Object,
            layer.object,
            x + layer.offsetX,
            y + layer.offsetY,
            zIndexBias + layer.zIndex,
            makeLayerEntryBaseName(namePrefix, layer),
            visible && layer.visible,
            writePolicy);
    }

    return *this;
}

TextObjectComposer& TextObjectComposer::addText(
    std::u32string_view text,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromU32(text),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addText(
    std::u32string_view text,
    int x,
    int y,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromU32(text),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addText(
    std::u32string_view text,
    int x,
    int y,
    const Style& style,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromU32(text, style),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addText(
    std::u32string_view text,
    int x,
    int y,
    const Style& style,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromU32(text, style),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addTextUtf8(
    std::string_view text,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromUtf8(text),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addTextUtf8(
    std::string_view text,
    int x,
    int y,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromUtf8(text),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addTextUtf8(
    std::string_view text,
    int x,
    int y,
    const Style& style,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromUtf8(text, style),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addTextUtf8(
    std::string_view text,
    int x,
    int y,
    const Style& style,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Text,
        TextObject::fromUtf8(text, style),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addGlyph(
    char32_t glyph,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Glyph,
        makeGlyphObject(glyph, std::nullopt),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addGlyph(
    char32_t glyph,
    int x,
    int y,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Glyph,
        makeGlyphObject(glyph, std::nullopt),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addGlyph(
    char32_t glyph,
    int x,
    int y,
    const Style& style,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Glyph,
        makeGlyphObject(glyph, std::optional<Style>(style)),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addGlyph(
    char32_t glyph,
    int x,
    int y,
    const Style& style,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Glyph,
        makeGlyphObject(glyph, std::optional<Style>(style)),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addFrame(
    int x,
    int y,
    int width,
    int height,
    const ObjectFactory::BorderGlyphs& border,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Frame,
        makeTransparentFrameObject(width, height, border, std::nullopt),
        x,
        y,
        zIndex,
        name,
        visible,
        Composition::WritePresets::visibleObject());
}

TextObjectComposer& TextObjectComposer::addFrame(
    int x,
    int y,
    int width,
    int height,
    const ObjectFactory::BorderGlyphs& border,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Frame,
        makeTransparentFrameObject(width, height, border, std::nullopt),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addFrame(
    int x,
    int y,
    int width,
    int height,
    const Style& style,
    const ObjectFactory::BorderGlyphs& border,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Frame,
        ObjectFactory::makeFrame(width, height, style, border),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addFrame(
    int x,
    int y,
    int width,
    int height,
    const Style& style,
    const ObjectFactory::BorderGlyphs& border,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::Frame,
        ObjectFactory::makeFrame(width, height, style, border),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addSolidFrame(
    int x,
    int y,
    int width,
    int height,
    const Style& style,
    const ObjectFactory::BorderGlyphs& border,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addFrame(
        x,
        y,
        width,
        height,
        style,
        border,
        Composition::WritePresets::solidObject(),
        zIndex,
        name,
        visible);
}

TextObjectComposer& TextObjectComposer::addVisibleFrame(
    int x,
    int y,
    int width,
    int height,
    const ObjectFactory::BorderGlyphs& border,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addFrame(
        x,
        y,
        width,
        height,
        border,
        Composition::WritePresets::visibleObject(),
        zIndex,
        name,
        visible);
}

TextObjectComposer& TextObjectComposer::addFilledRect(
    int x,
    int y,
    int width,
    int height,
    char32_t fillGlyph,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::FilledRect,
        ObjectFactory::makeFilledRect(width, height, fillGlyph),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addFilledRect(
    int x,
    int y,
    int width,
    int height,
    char32_t fillGlyph,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::FilledRect,
        ObjectFactory::makeFilledRect(width, height, fillGlyph),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObjectComposer& TextObjectComposer::addFilledRect(
    int x,
    int y,
    int width,
    int height,
    char32_t fillGlyph,
    const Style& style,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::FilledRect,
        ObjectFactory::makeFilledRect(width, height, fillGlyph, style),
        x,
        y,
        zIndex,
        name,
        visible,
        std::nullopt);
}

TextObjectComposer& TextObjectComposer::addFilledRect(
    int x,
    int y,
    int width,
    int height,
    char32_t fillGlyph,
    const Style& style,
    const Composition::WritePolicy& writePolicy,
    int zIndex,
    std::string_view name,
    bool visible)
{
    return addResolvedEntry(
        EntryKind::FilledRect,
        ObjectFactory::makeFilledRect(width, height, fillGlyph, style),
        x,
        y,
        zIndex,
        name,
        visible,
        writePolicy);
}

TextObject TextObjectComposer::buildTextObject() const
{
    return buildTextObject(BuildTextObjectOptions{});
}

TextObject TextObjectComposer::buildTextObject(const BuildTextObjectOptions& options) const
{
    const BuildBounds bounds = computeBuildBounds(m_entries, options.visibleOnly);
    if (!bounds.valid || bounds.width() <= 0 || bounds.height() <= 0)
    {
        return TextObject();
    }

    TextObjectBuilder builder(bounds.width(), bounds.height());
    builder.fillTransparent();

    const std::vector<const Entry*> buildEntries = collectBuildEntries(m_entries, options.visibleOnly);

    for (const Entry* entry : buildEntries)
    {
        if (entry == nullptr)
        {
            continue;
        }

        TextObjectBlitter::BlitOptions blitOptions;
        blitOptions.overrideStyle = options.overrideStyle;
        blitOptions.writePolicy = entry->writePolicy.has_value()
            ? *entry->writePolicy
            : options.writePolicy;
        blitOptions.skipStructuralContinuationCells = false;

        TextObjectBlitter::blitToBuilder(
            builder,
            entry->object,
            entry->offsetX - bounds.minX,
            entry->offsetY - bounds.minY,
            blitOptions);
    }

    return builder.build();
}

Rendering::LayeredTextObject TextObjectComposer::buildLayeredTextObject() const
{
    return buildLayeredTextObject(BuildLayeredTextObjectOptions{});
}

Rendering::LayeredTextObject TextObjectComposer::buildLayeredTextObject(
    const BuildLayeredTextObjectOptions& options) const
{
    const BuildBounds bounds = computeBuildBounds(m_entries, !options.includeHidden);
    if (!bounds.valid || bounds.width() <= 0 || bounds.height() <= 0)
    {
        return Rendering::LayeredTextObject();
    }

    Rendering::LayeredTextObject layered(bounds.width(), bounds.height());
    const std::vector<const Entry*> buildEntries = collectBuildEntries(m_entries, !options.includeHidden);

    for (const Entry* entry : buildEntries)
    {
        if (entry == nullptr)
        {
            continue;
        }

        Rendering::TextObjectLayer layer;
        layer.name = entry->name;
        layer.zIndex = entry->zIndex;
        layer.visible = entry->visible;
        layer.offsetX = entry->offsetX - bounds.minX;
        layer.offsetY = entry->offsetY - bounds.minY;
        layer.object = entry->object;

        layered.addLayer(std::move(layer));
    }

    return layered;
}

TextObjectComposer& TextObjectComposer::addResolvedEntry(
    EntryKind kind,
    const TextObject& object,
    int x,
    int y,
    int zIndex,
    std::string_view name,
    bool visible,
    const std::optional<Composition::WritePolicy>& writePolicy)
{
    Entry entry;
    entry.kind = kind;
    entry.name = makeUniqueEntryName(name, kind);
    entry.offsetX = x;
    entry.offsetY = y;
    entry.zIndex = zIndex;
    entry.visible = visible;
    entry.writePolicy = writePolicy;
    entry.object = object;

    m_entries.push_back(std::move(entry));
    return *this;
}

std::string TextObjectComposer::makeUniqueEntryName(std::string_view requestedName, EntryKind kind) const
{
    const std::string baseName = requestedName.empty() ? kindBaseName(kind) : std::string(requestedName);

    auto nameExists = [&](std::string_view candidate) -> bool
        {
            return std::any_of(
                m_entries.begin(),
                m_entries.end(),
                [&](const Entry& entry)
                {
                    return entry.name == candidate;
                });
        };

    if (!nameExists(baseName))
    {
        return baseName;
    }

    std::size_t suffix = 2;
    while (true)
    {
        const std::string candidate = baseName + std::string("_") + std::to_string(suffix);
        if (!nameExists(candidate))
        {
            return candidate;
        }

        ++suffix;
    }
}

TextObject TextObjectComposer::makeGlyphObject(char32_t glyph, const std::optional<Style>& style)
{
    const CellWidth width = UnicodeWidth::measureCodePointWidth(glyph);

    if (width == CellWidth::Zero)
    {
        return TextObject();
    }

    if (width == CellWidth::Two)
    {
        TextObjectBuilder builder(2, 1);
        builder.fillTransparent();
        builder.setWideGlyph(0, 0, glyph, style);
        return builder.build();
    }

    TextObjectBuilder builder(1, 1);
    builder.fillTransparent();
    builder.setGlyph(0, 0, glyph, style);
    return builder.build();
}

TextObject TextObjectComposer::makeTransparentFrameObject(
    int width,
    int height,
    const ObjectFactory::BorderGlyphs& border,
    const std::optional<Style>& style)
{
    if (width <= 0 || height <= 0)
    {
        return TextObject();
    }

    TextObjectBuilder builder(width, height);
    builder.fillTransparent();

    if (width == 1 && height == 1)
    {
        builder.setGlyph(0, 0, border.topLeft, style);
        return builder.build();
    }

    if (height == 1)
    {
        builder.setGlyph(0, 0, border.topLeft, style);
        for (int x = 1; x < width - 1; ++x)
        {
            builder.setGlyph(x, 0, border.horizontal, style);
        }

        if (width > 1)
        {
            builder.setGlyph(width - 1, 0, border.topRight, style);
        }

        return builder.build();
    }

    if (width == 1)
    {
        builder.setGlyph(0, 0, border.topLeft, style);
        for (int y = 1; y < height - 1; ++y)
        {
            builder.setGlyph(0, y, border.vertical, style);
        }

        builder.setGlyph(0, height - 1, border.bottomLeft, style);
        return builder.build();
    }

    builder.setGlyph(0, 0, border.topLeft, style);
    builder.setGlyph(width - 1, 0, border.topRight, style);
    builder.setGlyph(0, height - 1, border.bottomLeft, style);
    builder.setGlyph(width - 1, height - 1, border.bottomRight, style);

    for (int x = 1; x < width - 1; ++x)
    {
        builder.setGlyph(x, 0, border.horizontal, style);
        builder.setGlyph(x, height - 1, border.horizontal, style);
    }

    for (int y = 1; y < height - 1; ++y)
    {
        builder.setGlyph(0, y, border.vertical, style);
        builder.setGlyph(width - 1, y, border.vertical, style);
    }

    return builder.build();
}

TextObjectComposer::BuildBounds TextObjectComposer::computeBuildBounds(
    const std::vector<Entry>& entries,
    bool visibleOnly)
{
    BuildBounds bounds;

    for (const Entry& entry : entries)
    {
        if (!entryParticipatesInBuild(entry, visibleOnly))
        {
            continue;
        }

        const int entryMinX = entry.offsetX;
        const int entryMinY = entry.offsetY;
        const int entryMaxX = entry.offsetX + entry.object.getWidth();
        const int entryMaxY = entry.offsetY + entry.object.getHeight();

        if (!bounds.valid)
        {
            bounds.valid = true;
            bounds.minX = entryMinX;
            bounds.minY = entryMinY;
            bounds.maxX = entryMaxX;
            bounds.maxY = entryMaxY;
            continue;
        }

        bounds.minX = std::min(bounds.minX, entryMinX);
        bounds.minY = std::min(bounds.minY, entryMinY);
        bounds.maxX = std::max(bounds.maxX, entryMaxX);
        bounds.maxY = std::max(bounds.maxY, entryMaxY);
    }

    return bounds;
}

std::vector<const TextObjectComposer::Entry*> TextObjectComposer::collectBuildEntries(
    const std::vector<Entry>& entries,
    bool visibleOnly)
{
    std::vector<const Entry*> sortedEntries;
    sortedEntries.reserve(entries.size());

    for (const Entry& entry : entries)
    {
        if (!entryParticipatesInBuild(entry, visibleOnly))
        {
            continue;
        }

        sortedEntries.push_back(&entry);
    }

    std::stable_sort(
        sortedEntries.begin(),
        sortedEntries.end(),
        [](const Entry* left, const Entry* right)
        {
            return left->zIndex < right->zIndex;
        });

    return sortedEntries;
}