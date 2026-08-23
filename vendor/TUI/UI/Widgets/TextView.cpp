#include "UI/Widgets/TextView.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "Core/Rect.h"
#include "Input/Command.h"
#include "Input/Event.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"
#include "UI/Scrolling/ScrollBehavior.h"

TextView::TextView()
    : ScrollablePanel()
    , m_textStyleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
{
    setFocusable(true);
}

TextView::TextView(std::string title)
    : ScrollablePanel()
    , m_textStyleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
{
    setTitle(std::move(title));
    setFocusable(true);
}

TextView::TextView(const Rect& bounds, std::string title)
    : ScrollablePanel(bounds, std::move(title))
    , m_textStyleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
{
    setFocusable(true);
}

void TextView::setText(std::string_view text)
{
    m_lines = splitLines(text);
    updateContentSizeFromLines();
}

void TextView::setLines(const std::vector<std::string>& lines)
{
    m_lines = lines;
    updateContentSizeFromLines();
}

void TextView::setLines(std::vector<std::string>&& lines)
{
    m_lines = std::move(lines);
    updateContentSizeFromLines();
}

void TextView::appendLine(std::string line)
{
    m_lines.push_back(std::move(line));
    updateContentSizeFromLines();
}

void TextView::clear()
{
    m_lines.clear();
    updateContentSizeFromLines();
}

const std::vector<std::string>& TextView::lines() const
{
    return m_lines;
}

bool TextView::empty() const
{
    return m_lines.empty();
}

std::size_t TextView::lineCount() const
{
    return m_lines.size();
}

void TextView::setTextStyle(const Style& style)
{
    m_textStyleSet.normal = style;
}

const Style& TextView::textStyle() const
{
    return m_textStyleSet.normal;
}

const WidgetStyles::StyleSet& TextView::textStyleSet() const
{
    return m_textStyleSet;
}

void TextView::setTextStyleSet(const WidgetStyles::StyleSet& styleSet)
{
    m_textStyleSet = styleSet;
}

bool TextView::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        return false;
    }

    updateContentSizeFromLines();

    return ScrollablePanel::handleEvent(event);
}

void TextView::drawScrollableContent(
    Surface& surface,
    const Rect& visibleContentRect)
{
    updateContentSizeFromLines();

    if (visibleContentRect.size.width <= 0 || visibleContentRect.size.height <= 0)
    {
        return;
    }

    ScreenBuffer& buffer = surface.buffer();

    const Style& renderStyle = WidgetStyles::resolve(
        m_textStyleSet,
        WidgetStyles::stateFor(isEnabled(), isFocused()));

    for (int viewY = 0; viewY < visibleContentRect.size.height; ++viewY)
    {
        const int lineIndex = visibleContentRect.position.y + viewY;

        if (lineIndex < 0 || lineIndex >= static_cast<int>(m_lines.size()))
        {
            continue;
        }

        const std::string& sourceLine = m_lines[static_cast<std::size_t>(lineIndex)];

        const std::string visibleText = sliceUtf8ByDisplayColumns(
            sourceLine,
            visibleContentRect.position.x,
            visibleContentRect.size.width);

        if (visibleText.empty())
        {
            continue;
        }

        buffer.writeString(
            0,
            viewY,
            visibleText,
            renderStyle);
    }
}

void TextView::updateContentSizeFromLines()
{
    int maxWidth = 0;

    for (const std::string& line : m_lines)
    {
        maxWidth = std::max(
            maxWidth,
            TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(line)));
    }

    viewport().setContentSize(maxWidth, static_cast<int>(m_lines.size()));
}

std::vector<std::string> TextView::splitLines(std::string_view text)
{
    std::vector<std::string> result;
    std::string current;

    for (char ch : text)
    {
        if (ch == '\r')
        {
            continue;
        }

        if (ch == '\n')
        {
            result.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    result.push_back(current);
    return result;
}

std::string TextView::sliceUtf8ByDisplayColumns(
    std::string_view text,
    int startColumn,
    int maxColumns)
{
    if (maxColumns <= 0)
    {
        return {};
    }

    const int safeStartColumn = std::max(0, startColumn);
    const std::u32string text32 = UnicodeConversion::utf8ToU32(text);

    std::u32string sliced;
    int currentColumn = 0;
    int usedColumns = 0;

    for (const TextCluster& cluster : GraphemeSegmentation::segment(text32))
    {
        const int clusterWidth = std::max(0, static_cast<int>(cluster.displayWidth));

        if (clusterWidth == 0)
        {
            if (!sliced.empty())
            {
                sliced += cluster.codePoints;
            }

            continue;
        }

        if (currentColumn + clusterWidth <= safeStartColumn)
        {
            currentColumn += clusterWidth;
            continue;
        }

        if (usedColumns + clusterWidth > maxColumns)
        {
            break;
        }

        sliced += cluster.codePoints;
        usedColumns += clusterWidth;
        currentColumn += clusterWidth;
    }

    return UnicodeConversion::u32ToUtf8(sliced);
}