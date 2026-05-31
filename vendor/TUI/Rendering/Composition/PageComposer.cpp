#include "Rendering/Composition/PageComposer.h"

#include <algorithm>
#include <stdexcept>

#include "Assets/AssetLibrary.h"
#include "Rendering/Objects/TextObjectBuilder.h"
#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    bool isLineBreak(char32_t ch)
    {
        return ch == U'\n' || ch == U'\r';
    }

    Rect makeRectValue(int x, int y, int width, int height)
    {
        return Composition::makeRect(x, y, std::max(0, width), std::max(0, height));
    }

    std::string makeGridCellName(std::string_view baseName, int row, int col)
    {
        return std::string(baseName) + ".r" + std::to_string(row) + ".c" + std::to_string(col);
    }

    Composition::PageComposer::PlacementSpec makeTopLeftPlacement(
        const Composition::PageComposer::PlacementSpec& placement)
    {
        Composition::PageComposer::PlacementSpec normalized = placement;
        normalized.alignment = Composition::Align::topLeft();
        return normalized;
    }

    TextObject captureBufferAsTextObject(const ScreenBuffer& buffer)
    {
        TextObjectBuilder builder(buffer.getWidth(), buffer.getHeight());

        for (int y = 0; y < buffer.getHeight(); ++y)
        {
            for (int x = 0; x < buffer.getWidth(); ++x)
            {
                const ScreenCell& cell = buffer.getCell(x, y);
                const std::optional<Style> style =
                    cell.hasStyle() ? std::optional<Style>(cell.style) : std::nullopt;

                builder.setCell(x, y, cell.glyph, cell.kind, cell.width, style);
            }
        }

        return builder.build();
    }
}

namespace Composition
{
    PageComposer::PlacementSpec PageComposer::PlacementSpec::at(int x, int y)
    {
        PlacementSpec spec;
        spec.mode = Mode::Point;
        spec.point = { x, y };
        return spec;
    }

    PageComposer::PlacementSpec PageComposer::PlacementSpec::inRegion(
        const Rect& regionValue,
        const Alignment& alignmentValue,
        bool clamp)
    {
        PlacementSpec spec;
        spec.mode = Mode::Region;
        spec.region = regionValue;
        spec.alignment = alignmentValue;
        spec.clampToRegion = clamp;
        return spec;
    }

    PageComposer::PlacementSpec PageComposer::PlacementSpec::inNamedRegion(
        std::string_view name,
        const Alignment& alignmentValue,
        bool clamp)
    {
        PlacementSpec spec;
        spec.mode = Mode::NamedRegion;
        spec.regionName = std::string(name);
        spec.alignment = alignmentValue;
        spec.clampToRegion = clamp;
        return spec;
    }

    PageComposer::PlacementSpec PageComposer::PlacementSpec::inFullScreen(
        const Alignment& alignmentValue,
        bool clamp)
    {
        PlacementSpec spec;
        spec.mode = Mode::FullScreen;
        spec.alignment = alignmentValue;
        spec.clampToRegion = clamp;
        return spec;
    }

    PageComposer::PageComposer()
    {
        m_frameContext.bounds = Rect{};
    }

    PageComposer::PageComposer(ScreenBuffer& target)
        : m_target(&target)
        , m_buffer(target)
    {
        m_frameContext.bounds = getFullScreenRegion();
    }

    void PageComposer::setTarget(ScreenBuffer& target)
    {
        m_target = &target;

        if (m_buffer.getWidth() <= 0 || m_buffer.getHeight() <= 0)
        {
            m_buffer = target;
            m_lastSignature = 0;
            return;
        }

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::detachTarget()
    {
        m_target = nullptr;
    }

    void PageComposer::refreshFromTarget()
    {
        if (m_target != nullptr)
        {
            m_buffer = *m_target;
        }
    }

    bool PageComposer::hasTarget() const
    {
        return m_target != nullptr;
    }

    ScreenBuffer* PageComposer::tryGetTarget()
    {
        return m_target;
    }

    const ScreenBuffer* PageComposer::tryGetTarget() const
    {
        return m_target;
    }

    ScreenBuffer& PageComposer::getTarget()
    {
        if (m_target == nullptr)
        {
            throw std::runtime_error("PageComposer target is not attached.");
        }

        return *m_target;
    }

    const ScreenBuffer& PageComposer::getTarget() const
    {
        if (m_target == nullptr)
        {
            throw std::runtime_error("PageComposer target is not attached.");
        }

        return *m_target;
    }

    void PageComposer::resize(int width, int height)
    {
        m_buffer.resize(std::max(0, width), std::max(0, height));

        if (!m_frameContext.activeFrameName.has_value())
        {
            m_frameContext.bounds = getFullScreenRegion();
        }

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::clear(const Style& style)
    {
        m_buffer.clear(style);
        synchronizeTarget();
        m_lastSignature = 0;
    }

    int PageComposer::getWidth() const
    {
        return m_buffer.getWidth();
    }

    int PageComposer::getHeight() const
    {
        return m_buffer.getHeight();
    }

    TextObject PageComposer::captureBuffer() const
    {
        return captureBufferAsTextObject(m_buffer);
    }

    std::u32string PageComposer::renderToU32String() const
    {
        return m_buffer.renderToU32String();
    }

    std::string PageComposer::renderToUtf8String() const
    {
        return m_buffer.renderToUtf8String();
    }

    void PageComposer::createRegion(std::string_view name, const Rect& rect)
    {
        if (name.empty())
        {
            throw std::invalid_argument("PageComposer::createRegion requires a non-empty name.");
        }

        if (!m_regions.createRegion(NamedRegion{ std::string(name), rect }))
        {
            throw std::runtime_error("PageComposer::createRegion cannot create a duplicate region: " + std::string(name));
        }
    }

    void PageComposer::replaceRegion(std::string_view name, const Rect& rect)
    {
        if (name.empty())
        {
            throw std::invalid_argument("PageComposer::replaceRegion requires a non-empty name.");
        }

        m_regions.removeRegion(name);
        m_regions.createRegion(NamedRegion{ std::string(name), rect });
    }

    void PageComposer::removeRegion(std::string_view name)
    {
        m_regions.removeRegion(name);
    }

    void PageComposer::renameRegion(std::string_view from, std::string_view to)
    {
        if (from.empty() || to.empty())
        {
            throw std::invalid_argument("PageComposer::renameRegion requires non-empty names.");
        }

        if (from == to)
        {
            return;
        }

        const NamedRegion* region = m_regions.getRegion(from);
        if (region == nullptr)
        {
            throw std::runtime_error("PageComposer::renameRegion could not find source region: " + std::string(from));
        }

        if (m_regions.hasRegion(to))
        {
            throw std::runtime_error("PageComposer::renameRegion target already exists: " + std::string(to));
        }

        const Rect bounds = region->bounds;
        m_regions.removeRegion(from);
        m_regions.createRegion(NamedRegion{ std::string(to), bounds });
    }

    void PageComposer::clearRegions()
    {
        m_regions.clearRegions();
    }

    bool PageComposer::hasRegion(std::string_view name) const
    {
        return m_regions.hasRegion(name);
    }

    std::optional<Rect> PageComposer::tryGetRegion(std::string_view name) const
    {
        const NamedRegion* region = m_regions.getRegion(name);
        if (region == nullptr)
        {
            return std::nullopt;
        }

        return region->bounds;
    }

    Rect PageComposer::resolveRegion(std::string_view name) const
    {
        const std::optional<Rect> region = tryGetRegion(name);
        if (!region.has_value())
        {
            throw std::runtime_error("PageComposer::resolveRegion could not find region: " + std::string(name));
        }

        return *region;
    }

    Rect PageComposer::getFullScreenRegion() const
    {
        return makeRectValue(0, 0, m_buffer.getWidth(), m_buffer.getHeight());
    }

    void PageComposer::createFullScreenRegion(std::string_view name)
    {
        createRegion(name, getFullScreenRegion());
    }

    void PageComposer::createCenteredRegion(std::string_view name, int width, int height)
    {
        createCenteredRegion(name, Size{ width, height });
    }

    void PageComposer::createCenteredRegion(std::string_view name, const Size& size)
    {
        const Rect full = getFullScreenRegion();
        createRegion(
            name,
            makeRectValue(
                full.position.x + (full.size.width - size.width) / 2,
                full.position.y + (full.size.height - size.height) / 2,
                size.width,
                size.height));
    }

    void PageComposer::createInsetRegion(
        std::string_view name,
        std::string_view parent,
        int inset)
    {
        const Rect base = resolveRegion(parent);
        createRegion(
            name,
            makeRectValue(
                base.position.x + inset,
                base.position.y + inset,
                base.size.width - (inset * 2),
                base.size.height - (inset * 2)));
    }

    void PageComposer::createInsetRegion(
        std::string_view name,
        std::string_view parent,
        int left,
        int top,
        int right,
        int bottom)
    {
        const Rect parentRect = resolveRegion(parent);

        Rect inset{
            Point{ parentRect.position.x + left, parentRect.position.y + top },
            Size{
                std::max(0, parentRect.size.width - left - right),
                std::max(0, parentRect.size.height - top - bottom)
            }
        };

        createRegion(name, inset);
    }

    void PageComposer::createFramedScreenRegions(
        std::string_view outerName,
        std::string_view innerName,
        int thickness)
    {
        createFramedScreenRegions(outerName, innerName, thickness, thickness);
    }

    void PageComposer::createFramedScreenRegions(
        std::string_view outerName,
        std::string_view innerName,
        int horizontalThickness,
        int verticalThickness)
    {
        const Rect full = getFullScreenRegion();

        createRegion(outerName, full);
        createRegion(
            innerName,
            makeRectValue(
                full.position.x + horizontalThickness,
                full.position.y + verticalThickness,
                full.size.width - (horizontalThickness * 2),
                full.size.height - (verticalThickness * 2)));
    }

    void PageComposer::registerGrid(
        std::string_view baseName,
        const Rect& bounds,
        int rows,
        int cols)
    {
        if (baseName.empty())
        {
            throw std::invalid_argument("PageComposer::registerGrid requires a non-empty base name.");
        }

        if (rows <= 0 || cols <= 0)
        {
            throw std::invalid_argument("PageComposer::registerGrid requires positive rows and cols.");
        }

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                createRegion(
                    makeGridCellName(baseName, row, col),
                    gridCell(bounds, rows, cols, row, col));
            }
        }
    }

    void PageComposer::registerGridArea(
        std::string_view name,
        std::string_view baseName,
        int row,
        int col)
    {
        createRegion(name, resolveRegion(makeGridCellName(baseName, row, col)));
    }

    Rect PageComposer::peekTop(const Rect& base, int height) const
    {
        return makeRectValue(
            base.position.x,
            base.position.y,
            base.size.width,
            std::min(std::max(0, height), base.size.height));
    }

    Rect PageComposer::peekBottom(const Rect& base, int height) const
    {
        const int clampedHeight = std::min(std::max(0, height), base.size.height);
        return makeRectValue(
            base.position.x,
            base.position.y + base.size.height - clampedHeight,
            base.size.width,
            clampedHeight);
    }

    Rect PageComposer::peekLeft(const Rect& base, int width) const
    {
        return makeRectValue(
            base.position.x,
            base.position.y,
            std::min(std::max(0, width), base.size.width),
            base.size.height);
    }

    Rect PageComposer::peekRight(const Rect& base, int width) const
    {
        const int clampedWidth = std::min(std::max(0, width), base.size.width);
        return makeRectValue(
            base.position.x + base.size.width - clampedWidth,
            base.position.y,
            clampedWidth,
            base.size.height);
    }

    Rect PageComposer::remainderBelow(const Rect& base, int height) const
    {
        const int consumed = std::min(std::max(0, height), base.size.height);
        return makeRectValue(
            base.position.x,
            base.position.y + consumed,
            base.size.width,
            base.size.height - consumed);
    }

    Rect PageComposer::remainderAbove(const Rect& base, int height) const
    {
        const int consumed = std::min(std::max(0, height), base.size.height);
        return makeRectValue(
            base.position.x,
            base.position.y,
            base.size.width,
            base.size.height - consumed);
    }

    Rect PageComposer::remainderRightOf(const Rect& base, int width) const
    {
        const int consumed = std::min(std::max(0, width), base.size.width);
        return makeRectValue(
            base.position.x + consumed,
            base.position.y,
            base.size.width - consumed,
            base.size.height);
    }

    Rect PageComposer::remainderLeftOf(const Rect& base, int width) const
    {
        const int consumed = std::min(std::max(0, width), base.size.width);
        return makeRectValue(
            base.position.x,
            base.position.y,
            base.size.width - consumed,
            base.size.height);
    }

    std::pair<Rect, Rect> PageComposer::splitTop(const Rect& base, int height) const
    {
        return { peekTop(base, height), remainderBelow(base, height) };
    }

    std::pair<Rect, Rect> PageComposer::splitBottom(const Rect& base, int height) const
    {
        return { peekBottom(base, height), remainderAbove(base, height) };
    }

    std::pair<Rect, Rect> PageComposer::splitLeft(const Rect& base, int width) const
    {
        return { peekLeft(base, width), remainderRightOf(base, width) };
    }

    std::pair<Rect, Rect> PageComposer::splitRight(const Rect& base, int width) const
    {
        return { peekRight(base, width), remainderLeftOf(base, width) };
    }

    std::vector<Rect> PageComposer::splitGrid(const Rect& base, int rows, int cols) const
    {
        std::vector<Rect> cells;

        if (rows <= 0 || cols <= 0)
        {
            return cells;
        }

        cells.reserve(static_cast<std::size_t>(rows * cols));
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                cells.push_back(gridCell(base, rows, cols, row, col));
            }
        }

        return cells;
    }

    Rect PageComposer::gridCell(const Rect& base, int rows, int cols, int row, int col) const
    {
        if (rows <= 0 || cols <= 0 || row < 0 || row >= rows || col < 0 || col >= cols)
        {
            return Rect{};
        }

        const int x = base.position.x + ((col * base.size.width) / cols);
        const int nextX = base.position.x + (((col + 1) * base.size.width) / cols);
        const int y = base.position.y + ((row * base.size.height) / rows);
        const int nextY = base.position.y + (((row + 1) * base.size.height) / rows);
        return makeRectValue(x, y, nextX - x, nextY - y);
    }

    Rect PageComposer::gridRow(const Rect& base, int rows, int row) const
    {
        if (rows <= 0 || row < 0 || row >= rows)
        {
            return Rect{};
        }

        const int y = base.position.y + ((row * base.size.height) / rows);
        const int nextY = base.position.y + (((row + 1) * base.size.height) / rows);
        return makeRectValue(base.position.x, y, base.size.width, nextY - y);
    }

    Rect PageComposer::gridColumn(const Rect& base, int cols, int col) const
    {
        if (cols <= 0 || col < 0 || col >= cols)
        {
            return Rect{};
        }

        const int x = base.position.x + ((col * base.size.width) / cols);
        const int nextX = base.position.x + (((col + 1) * base.size.width) / cols);
        return makeRectValue(x, base.position.y, nextX - x, base.size.height);
    }

    Rect PageComposer::resolvePlacementBounds(
        const PlacementSpec& placement,
        const Size& contentSize) const
    {
        const PlacementResult result = resolvePlacementResult(placement, contentSize);
        return makeRectValue(
            result.origin.x,
            result.origin.y,
            std::max(0, result.contentSize.width),
            std::max(0, result.contentSize.height));
    }

    void PageComposer::placeSource(
        const ObjectSource& source,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        const ResolvedObjectSource resolved = resolveObjectSource(source, m_assetLibrary);
        if (!resolved.hasObject())
        {
            return;
        }

        writeObject(resolved.object, placement, policy);
    }

    void PageComposer::writeObject(
        const TextObject& object,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        if (!object.isLoaded())
        {
            return;
        }

        refreshFromTarget();

        const PlacementResult result = resolvePlacementResult(
            placement,
            Size{ object.getWidth(), object.getHeight() });

        object.draw(m_buffer, result.origin.x, result.origin.y, policy);
        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::writeText(
        std::string_view text,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        const std::u32string firstLine = extractFirstLine(UnicodeConversion::utf8ToU32(text));
        writeObject(TextObject::fromU32(firstLine), makeTopLeftPlacement(placement), policy);
    }

    void PageComposer::writeTextBlock(
        std::string_view text,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        writeObject(TextObject::fromUtf8(text), makeTopLeftPlacement(placement), policy);
    }

    void PageComposer::writeAlignedText(
        std::string_view text,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        const std::u32string firstLine = extractFirstLine(UnicodeConversion::utf8ToU32(text));
        writeObject(TextObject::fromU32(firstLine), placement, policy);
    }

    void PageComposer::writeAlignedTextBlock(
        std::string_view text,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        writeObject(TextObject::fromUtf8(text), placement, policy);
    }

    void PageComposer::writeWrappedText(
        std::string_view text,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        const std::u32string u32Text = UnicodeConversion::utf8ToU32(text);
        const Rect targetRegion = resolvePlacementRegion(placement);

        if (targetRegion.size.width <= 0)
        {
            writeObject(TextObject::fromU32(extractFirstLine(u32Text)), placement, policy);
            return;
        }

        const std::vector<std::u32string> wrappedLines = wrapTextToLines(u32Text, targetRegion.size.width);
        writeObject(TextObject::fromU32([&]() {
            std::u32string joined;
            for (std::size_t i = 0; i < wrappedLines.size(); ++i)
            {
                if (i > 0)
                {
                    joined.push_back(U'\n');
                }
                joined += wrappedLines[i];
            }
            return joined;
            }()), placement, policy);
    }

    void PageComposer::writeAt(
        const TextObject& object,
        int x,
        int y,
        const WritePolicy& policy)
    {
        writeObject(object, PlacementSpec::at(x, y), policy);
    }

    void PageComposer::writeInRegion(
        const TextObject& object,
        std::string_view regionName,
        const Alignment& alignment,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeObject(
            object,
            PlacementSpec::inNamedRegion(regionName, alignment, clampToRegion),
            policy);
    }

    void PageComposer::writeObjectInRegion(
        const TextObject& object,
        std::string_view regionName,
        const Alignment& alignment,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeInRegion(
            object,
            regionName,
            alignment,
            policy,
            clampToRegion);
    }

    void PageComposer::writeAligned(
        const TextObject& object,
        const Alignment& alignment,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeObject(
            object,
            PlacementSpec::inFullScreen(alignment, clampToRegion),
            policy);
    }

    void PageComposer::writeVisible(
        const TextObject& object,
        const PlacementSpec& placement)
    {
        writeObject(object, placement, WritePresets::authoredObject());
    }

    void PageComposer::writeSolid(
        const TextObject& object,
        const PlacementSpec& placement)
    {
        writeObject(object, placement, WritePresets::solidObject());
    }

    void PageComposer::drawPanel(
        std::string_view regionName,
        const Style& fillStyle,
        const Style& frameStyle,
        const ObjectFactory::BorderGlyphs& glyphs)
    {
        const Rect rect = resolveRegion(regionName);

        if (rect.size.width <= 0 || rect.size.height <= 0)
        {
            return;
        }

        refreshFromTarget();

        m_buffer.fillRect(rect, U' ', fillStyle);

        m_buffer.drawFrame(
            rect,
            frameStyle,
            glyphs.topLeft,
            glyphs.topRight,
            glyphs.bottomLeft,
            glyphs.bottomRight,
            glyphs.horizontal,
            glyphs.vertical);

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::writeTextInRegion(
        std::string_view text,
        std::string_view regionName,
        const Alignment& alignment,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeAlignedText(
            text,
            PlacementSpec::inNamedRegion(regionName, alignment, clampToRegion),
            policy);
    }

    void PageComposer::writeTextBlockInRegion(
        std::string_view text,
        std::string_view regionName,
        const Alignment& alignment,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeAlignedTextBlock(
            text,
            PlacementSpec::inNamedRegion(regionName, alignment, clampToRegion),
            policy);
    }

    void PageComposer::writeWrappedTextInRegion(
        std::string_view text,
        std::string_view regionName,
        const Alignment& alignment,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeWrappedText(
            text,
            PlacementSpec::inNamedRegion(regionName, alignment, clampToRegion),
            policy);
    }

    void PageComposer::writeCenteredText(
        std::string_view text,
        const WritePolicy& policy,
        bool clampToRegion)
    {
        writeAlignedText(
            text,
            PlacementSpec::inFullScreen(Align::center(), clampToRegion),
            policy);
    }

    void PageComposer::writePanelTitle(
        std::string_view regionName,
        std::string_view text,
        const Style& style,
        int insetX,
        int insetY)
    {
        const Rect rect = resolveRegion(regionName);

        if (rect.size.width <= 0 || rect.size.height <= 0)
        {
            return;
        }

        const int x = rect.position.x + insetX;
        const int y = rect.position.y + insetY;

        refreshFromTarget();

        m_buffer.writeText(x, y, UnicodeConversion::utf8ToU32(text), style);

        synchronizeTarget();
        m_lastSignature = 0;
    }

    /*
        writeLine(...): write one line of text at a specific row inside a region

        so it's easier than writing: 

        region.position.x
        region.position.y + y
        etc...
    */

    void PageComposer::writeLine(
        const Rect& region,
        int y,
        std::string_view text,
        const Style& style)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        refreshFromTarget();

        m_buffer.writeText(
            region.position.x,
            region.position.y + y,
            UnicodeConversion::utf8ToU32(text),
            style);

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::writeLine(
        std::string_view regionName,
        int y,
        std::string_view text,
        const Style& style)
    {
        writeLine(resolveRegion(regionName), y, text, style);
    }

    /*
        writeLines(...): multi-line version of writeLine

        example:

        composer.writeLines(
            region,
            1,
            {
                "FPS: 60",
                "Mode: ASCII",
                "Triangles: 842"
            },
            style);
    */

    void PageComposer::writeLines(
        const Rect& region,
        int startY,
        const std::vector<std::string>& lines,
        const Style& style)
    {
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            writeLine(region, startY + static_cast<int>(index), lines[index], style);
        }
    }

    void PageComposer::writeLines(
        std::string_view regionName,
        int startY,
        const std::vector<std::string>& lines,
        const Style& style)
    {
        writeLines(resolveRegion(regionName), startY, lines, style);
    }

    /*
        writeRight(...): right-align one line of text inside a region row
    */

    void PageComposer::writeRight(
        const Rect& region,
        int y,
        std::string_view text,
        const Style& style)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        const int textWidth = measureDisplayWidth(UnicodeConversion::utf8ToU32(text));
        const int x = std::max(region.position.x, region.position.x + region.size.width - textWidth);

        refreshFromTarget();

        m_buffer.writeText(
            x,
            region.position.y + y,
            UnicodeConversion::utf8ToU32(text),
            style);

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::writeRight(
        std::string_view regionName,
        int y,
        std::string_view text,
        const Style& style)
    {
        writeRight(resolveRegion(regionName), y, text, style);
    }

    /*
        drawNameValue: This method automates:
            1. left-side label placement
            2. right-side value placement
            3. spacing/gap logic
            4. alignment consistency

        pattern:
            Name          Value

        examples:
            FPS           60
            Triangles     842
            Renderer      ASCII
            CPU Usage     73%
            Resolution    80x25
    */

    void PageComposer::drawNameValue(
        const Rect& region,
        int y,
        std::string_view name,
        std::string_view value,
        const Style& nameStyle,
        const Style& valueStyle,
        int gap)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        const std::u32string nameText = UnicodeConversion::utf8ToU32(name);
        const std::u32string valueText = UnicodeConversion::utf8ToU32(value);
        const int nameWidth = measureDisplayWidth(nameText);
        const int safeGap = std::max(0, gap);

        refreshFromTarget();

        const int rowY = region.position.y + y;
        m_buffer.writeText(region.position.x, rowY, nameText, nameStyle);

        const int valueX = region.position.x + nameWidth + safeGap;
        if (valueX < region.position.x + region.size.width)
        {
            m_buffer.writeText(valueX, rowY, valueText, valueStyle);
        }

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::drawNameValue(
        std::string_view regionName,
        int y,
        std::string_view name,
        std::string_view value,
        const Style& nameStyle,
        const Style& valueStyle,
        int gap)
    {
        drawNameValue(resolveRegion(regionName), y, name, value, nameStyle, valueStyle, gap);
    }

    /*
        drawHorizontalRule: draw a horizontal line across a region row
    */

    void PageComposer::drawHorizontalRule(
        const Rect& region,
        int y,
        const Style& style,
        char32_t glyph)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        refreshFromTarget();

        m_buffer.writeText(
            region.position.x,
            region.position.y + y,
            std::u32string(static_cast<std::size_t>(region.size.width), glyph),
            style);

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::drawHorizontalRule(
        std::string_view regionName,
        int y,
        const Style& style,
        char32_t glyph)
    {
        drawHorizontalRule(resolveRegion(regionName), y, style, glyph);
    }

    /*
        drawSectionHeader(...): draws a section title in the style of:
            - ──────── Render Stats ────────
            - === Diagnostics ===
            - Performance ------------------

        So it:
        1. writes the section title
        2. draws a horizontal rule around/after it
        3. has spacing/alignment math
    */

    void PageComposer::drawSectionHeader(
        const Rect& region,
        int y,
        std::string_view text,
        const Style& textStyle,
        const Style& ruleStyle,
        char32_t ruleGlyph)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        drawHorizontalRule(region, y, ruleStyle, ruleGlyph);
        writeLine(region, y, text, textStyle);
    }

    void PageComposer::drawSectionHeader(
        std::string_view regionName,
        int y,
        std::string_view text,
        const Style& textStyle,
        const Style& ruleStyle,
        char32_t ruleGlyph)
    {
        drawSectionHeader(resolveRegion(regionName), y, text, textStyle, ruleStyle, ruleGlyph);
    }

    /*
        drawMeter(...) convenience helper for drawing ASCII/Unicode 
        progress bars inside a composed region.

        default is: 
        filledGlyph = U'█'
        emptyGlyph  = U'░'
    */

    void PageComposer::drawMeter(
        const Rect& region,
        int y,
        std::string_view label,
        int value,
        int maxValue,
        const Style& labelStyle,
        const Style& filledStyle,
        const Style& emptyStyle, 
        int meterWidth)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        const std::u32string labelText = UnicodeConversion::utf8ToU32(label);
        const int labelWidth = measureDisplayWidth(labelText);
        const int meterX = region.position.x + labelWidth + (labelText.empty() ? 0 : 1);
        const int availableWidth = region.position.x + region.size.width - meterX;

        if (availableWidth <= 0)
        {
            writeLine(region, y, label, labelStyle);
            return;
        }

        const int resolvedMeterWidth =
            meterWidth > 0 ? std::min(meterWidth, availableWidth) : availableWidth;

        const int safeMax = std::max(1, maxValue);
        const int clampedValue = std::max(0, std::min(value, safeMax));
        const int filledWidth = (resolvedMeterWidth * clampedValue) / safeMax;
        const int emptyWidth = resolvedMeterWidth - filledWidth;

        refreshFromTarget();

        const int rowY = region.position.y + y;

        if (!labelText.empty())
        {
            m_buffer.writeText(region.position.x, rowY, labelText, labelStyle);
        }

        if (filledWidth > 0)
        {
            m_buffer.writeText(
                meterX,
                rowY,
                std::u32string(static_cast<std::size_t>(filledWidth), U'█'),
                filledStyle);
        }

        if (emptyWidth > 0)
        {
            m_buffer.writeText(
                meterX + filledWidth,
                rowY,
                std::u32string(static_cast<std::size_t>(emptyWidth), U'░'),
                emptyStyle);
        }

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::drawMeter(
        std::string_view regionName,
        int y,
        std::string_view label,
        int value,
        int maxValue,
        const Style& labelStyle,
        const Style& filledStyle,
        const Style& emptyStyle,
        int meterWidth)
    {
        drawMeter(
            resolveRegion(regionName),
            y,
            label,
            value,
            maxValue,
            labelStyle,
            filledStyle,
            emptyStyle,
            meterWidth);
    }

    /*
        drawKeyHints(...): small convenience method for drawing footer-style shortcut hints like:
        [F1] Help   [D] Dock   [Esc] Back
    */

    void PageComposer::drawKeyHints(
        const Rect& region,
        int y,
        const std::vector<KeyHint>& hints,
        const Style& keyStyle,
        const Style& labelStyle,
        int gap)
    {
        if (region.size.width <= 0 || region.size.height <= 0)
        {
            return;
        }

        if (y < 0 || y >= region.size.height)
        {
            return;
        }

        const int safeGap = std::max(0, gap);
        int x = region.position.x;
        const int rowY = region.position.y + y;
        const int maxX = region.position.x + region.size.width;

        refreshFromTarget();

        for (const KeyHint& hint : hints)
        {
            const std::u32string keyText = UnicodeConversion::utf8ToU32(hint.key);
            const std::u32string labelText = UnicodeConversion::utf8ToU32(hint.label);
            const int keyWidth = measureDisplayWidth(keyText);
            const int labelWidth = measureDisplayWidth(labelText);
            const int pairWidth = keyWidth + (labelText.empty() ? 0 : 1 + labelWidth);

            if (x + pairWidth > maxX)
            {
                break;
            }

            m_buffer.writeText(x, rowY, keyText, keyStyle);
            x += keyWidth;

            if (!labelText.empty())
            {
                m_buffer.writeText(x, rowY, U" ", labelStyle);
                ++x;
                m_buffer.writeText(x, rowY, labelText, labelStyle);
                x += labelWidth;
            }

            x += safeGap;
        }

        synchronizeTarget();
        m_lastSignature = 0;
    }

    void PageComposer::drawKeyHints(
        std::string_view regionName,
        int y,
        const std::vector<KeyHint>& hints,
        const Style& keyStyle,
        const Style& labelStyle,
        int gap)
    {
        drawKeyHints(resolveRegion(regionName), y, hints, keyStyle, labelStyle, gap);
    }

    void PageComposer::setAssetLibrary(Assets::AssetLibrary* library)
    {
        m_assetLibrary = library;
    }

    void PageComposer::detachAssetLibrary()
    {
        m_assetLibrary = nullptr;
    }

    bool PageComposer::hasAssetLibrary() const
    {
        return m_assetLibrary != nullptr;
    }

    Assets::AssetLibrary* PageComposer::tryGetAssetLibrary()
    {
        return m_assetLibrary;
    }

    const Assets::AssetLibrary* PageComposer::tryGetAssetLibrary() const
    {
        return m_assetLibrary;
    }

    void PageComposer::setScreenTemplateLoader(ScreenTemplateLoader loader)
    {
        m_templateLoader = std::move(loader);
    }

    std::optional<TextObject> PageComposer::loadScreenTemplate(std::string_view name) const
    {
        if (!m_templateLoader)
        {
            return std::nullopt;
        }

        return m_templateLoader(name, m_assetLibrary);
    }

    void PageComposer::loadScreen(
        const TextObject& screen,
        const PlacementSpec& placement,
        const WritePolicy& policy)
    {
        writeObject(screen, placement, policy);
    }

    void PageComposer::setFrames(const std::vector<Frame>& frames)
    {
        m_frames = frames;
    }

    void PageComposer::clearFrames()
    {
        m_frames.clear();
    }

    std::size_t PageComposer::getFrameCount() const
    {
        return m_frames.size();
    }

    bool PageComposer::hasFrame(std::string_view name) const
    {
        return tryGetFrame(name) != nullptr;
    }

    const PageComposer::Frame* PageComposer::tryGetFrame(std::string_view name) const
    {
        for (const Frame& frame : m_frames)
        {
            if (frame.name == name)
            {
                return &frame;
            }
        }

        return nullptr;
    }

    void PageComposer::beginFrame(std::string_view name)
    {
        m_frameContext.activeFrameName = std::string(name);
        const Frame* frame = tryGetFrame(name);
        m_frameContext.bounds = (frame != nullptr) ? frame->bounds : getFullScreenRegion();
        m_lastSignature = 0;
    }

    void PageComposer::endFrame()
    {
        clearFrameContext();
    }

    void PageComposer::clearFrameContext()
    {
        m_frameContext.activeFrameName.reset();
        m_frameContext.bounds = getFullScreenRegion();
        m_lastSignature = 0;
    }

    const PageComposer::FrameContext& PageComposer::getFrameContext() const
    {
        return m_frameContext;
    }

    void PageComposer::setDiagnostics(DiagnosticsContext* ctx)
    {
        m_diagnostics = ctx;
    }

    void PageComposer::detachDiagnostics()
    {
        m_diagnostics = nullptr;
    }

    bool PageComposer::hasDiagnostics() const
    {
        return m_diagnostics != nullptr;
    }

    PageComposer::DiagnosticsContext* PageComposer::tryGetDiagnostics()
    {
        return m_diagnostics;
    }

    const PageComposer::DiagnosticsContext* PageComposer::tryGetDiagnostics() const
    {
        return m_diagnostics;
    }

    std::uint64_t PageComposer::computeDeterministicSignature() const
    {
        const PageCompositionDiagnostics::FrameContext diagnosticsFrameContext = [&]() {
            PageCompositionDiagnostics::FrameContext ctx;
            if (m_frameContext.activeFrameName.has_value())
            {
                ctx.tag = *m_frameContext.activeFrameName;
            }
            return ctx;
            }();

        m_lastSignature = PageCompositionDiagnostics::computeStableSignature(
            m_buffer.getWidth(),
            m_buffer.getHeight(),
            m_buffer.renderToU32String(),
            diagnosticsFrameContext);

        if (m_diagnostics != nullptr)
        {
            m_diagnostics->setLastDeterministicSignature(m_lastSignature);
        }

        return m_lastSignature;
    }

    bool PageComposer::verifyDeterministicSignature(std::uint64_t expected) const
    {
        return computeDeterministicSignature() == expected;
    }

    std::uint64_t PageComposer::getLastDeterministicSignature() const
    {
        if (m_lastSignature == 0)
        {
            return computeDeterministicSignature();
        }

        return m_lastSignature;
    }

    Rect PageComposer::resolveRegionOrFullScreen(std::string_view name) const
    {
        if (name.empty())
        {
            return getFullScreenRegion();
        }

        const std::optional<Rect> region = tryGetRegion(name);
        return region.has_value() ? *region : getFullScreenRegion();
    }

    Rect PageComposer::resolvePlacementRegion(const PlacementSpec& placement) const
    {
        switch (placement.mode)
        {
        case PlacementSpec::Mode::Point:
            return makeRectValue(
                placement.point.x,
                placement.point.y,
                std::max(0, m_buffer.getWidth() - placement.point.x),
                std::max(0, m_buffer.getHeight() - placement.point.y));

        case PlacementSpec::Mode::Region:
            return placement.region;

        case PlacementSpec::Mode::NamedRegion:
            return resolveRegionOrFullScreen(placement.regionName);

        case PlacementSpec::Mode::FullScreen:
            return getFullScreenRegion();
        }

        return Rect{};
    }

    PlacementResult PageComposer::resolvePlacementResult(
        const PlacementSpec& placement,
        const Size& contentSize) const
    {
        if (placement.mode == PlacementSpec::Mode::Point)
        {
            PlacementResult result;
            result.origin = placement.point;
            result.region = makeRectValue(placement.point.x, placement.point.y, contentSize.width, contentSize.height);
            result.contentSize = contentSize;
            result.alignment = Align::topLeft();
            return result;
        }

        const Rect region = resolvePlacementRegion(placement);
        return resolvePlacement(makePlacementRequest(
            region,
            contentSize,
            placement.alignment,
            placement.clampToRegion));
    }

    void PageComposer::ensureBufferInitialized() const
    {
        if (m_target == nullptr && m_buffer.getWidth() <= 0 && m_buffer.getHeight() <= 0)
        {
            throw std::runtime_error(
                "PageComposer has no attached target and no initialized internal buffer.");
        }
    }

    void PageComposer::synchronizeTarget()
    {
        if (m_target != nullptr)
        {
            *m_target = m_buffer;
        }
    }

    std::u32string PageComposer::extractFirstLine(std::u32string_view text)
    {
        std::u32string line;

        for (char32_t ch : text)
        {
            if (isLineBreak(ch))
            {
                break;
            }

            line.push_back(ch);
        }

        return line;
    }

    std::vector<std::u32string> PageComposer::splitLines(std::u32string_view text)
    {
        std::vector<std::u32string> lines;
        std::u32string current;

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char32_t ch = text[i];

            if (ch == U'\r')
            {
                lines.push_back(current);
                current.clear();

                if ((i + 1) < text.size() && text[i + 1] == U'\n')
                {
                    ++i;
                }

                continue;
            }

            if (ch == U'\n')
            {
                lines.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(ch);
        }

        lines.push_back(current);
        return lines;
    }

    bool PageComposer::isWrapWhitespaceText(std::u32string_view text)
    {
        if (text.empty())
        {
            return false;
        }

        for (char32_t ch : text)
        {
            if (ch != U' ' && ch != U'\t' && ch != U'\v' && ch != U'\f')
            {
                return false;
            }
        }

        return true;
    }

    std::u32string PageComposer::trimTrailingWrapWhitespace(std::u32string_view text)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(text);
        if (clusters.empty())
        {
            return std::u32string(text);
        }

        std::size_t end = clusters.size();
        while (end > 0 && isWrapWhitespaceText(clusters[end - 1].codePoints))
        {
            --end;
        }

        std::u32string result;
        for (std::size_t i = 0; i < end; ++i)
        {
            result += clusters[i].codePoints;
        }

        return result;
    }

    std::vector<std::u32string> PageComposer::wrapSingleLineToLines(
        std::u32string_view text,
        int maxWidth)
    {
        std::vector<std::u32string> lines;

        if (maxWidth <= 0)
        {
            return lines;
        }

        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(text);
        if (clusters.empty())
        {
            lines.push_back(U"");
            return lines;
        }

        auto buildClusterRange =
            [&](std::size_t startIndex,
                std::size_t endIndexExclusive,
                bool trimTrailingWhitespace) -> std::u32string
            {
                std::u32string result;
                for (std::size_t i = startIndex; i < endIndexExclusive; ++i)
                {
                    result += clusters[i].codePoints;
                }

                if (trimTrailingWhitespace)
                {
                    result = trimTrailingWrapWhitespace(result);
                }

                return result;
            };

        std::size_t lineStart = 0;

        while (lineStart < clusters.size())
        {
            while (lineStart < clusters.size() && isWrapWhitespaceText(clusters[lineStart].codePoints))
            {
                ++lineStart;
            }

            if (lineStart >= clusters.size())
            {
                lines.push_back(U"");
                break;
            }

            std::size_t index = lineStart;
            std::size_t lastBreak = static_cast<std::size_t>(-1);
            int currentWidth = 0;

            while (index < clusters.size())
            {
                const int clusterWidth = std::max(0, static_cast<int>(clusters[index].displayWidth));
                const bool breakableWhitespace = isWrapWhitespaceText(clusters[index].codePoints);

                if (currentWidth > 0 && (currentWidth + clusterWidth) > maxWidth)
                {
                    break;
                }

                currentWidth += clusterWidth;
                if (breakableWhitespace)
                {
                    lastBreak = index;
                }

                ++index;
            }

            if (index >= clusters.size())
            {
                lines.push_back(buildClusterRange(lineStart, clusters.size(), true));
                break;
            }

            if (currentWidth == 0)
            {
                lines.push_back(buildClusterRange(lineStart, lineStart + 1, false));
                lineStart = lineStart + 1;
                continue;
            }

            if (lastBreak != static_cast<std::size_t>(-1) && lastBreak >= lineStart)
            {
                lines.push_back(buildClusterRange(lineStart, lastBreak, true));
                lineStart = lastBreak + 1;
                continue;
            }

            lines.push_back(buildClusterRange(lineStart, index, true));
            lineStart = index;
        }

        if (lines.empty())
        {
            lines.push_back(U"");
        }

        return lines;
    }

    std::vector<std::u32string> PageComposer::wrapTextToLines(
        std::u32string_view text,
        int maxWidth)
    {
        std::vector<std::u32string> wrappedLines;
        if (maxWidth <= 0)
        {
            return wrappedLines;
        }

        const std::vector<std::u32string> sourceLines = splitLines(text);
        for (const std::u32string& sourceLine : sourceLines)
        {
            std::vector<std::u32string> paragraphLines = wrapSingleLineToLines(sourceLine, maxWidth);
            if (paragraphLines.empty())
            {
                wrappedLines.push_back(U"");
                continue;
            }

            wrappedLines.insert(
                wrappedLines.end(),
                paragraphLines.begin(),
                paragraphLines.end());
        }

        return wrappedLines;
    }

    int PageComposer::measureDisplayWidth(std::u32string_view text)
    {
        int width = 0;
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(text);
        for (const TextCluster& cluster : clusters)
        {
            width += std::max(0, static_cast<int>(cluster.displayWidth));
        }

        return width;
    }
}
