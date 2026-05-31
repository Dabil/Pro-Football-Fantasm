#include "UI/Panels/StatusBar.h"

#include <algorithm>

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/UIThemes.h"
#include "Rendering/Surface.h"
#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    std::u32string truncateToDisplayWidth(
        const std::u32string& text,
        int maxWidth)
    {
        if (maxWidth <= 0 || text.empty())
        {
            return {};
        }

        std::u32string result;
        int width = 0;

        for (const TextCluster& cluster : GraphemeSegmentation::segment(text))
        {
            if (cluster.displayWidth <= 0)
            {
                result += cluster.codePoints;
                continue;
            }

            if (width + cluster.displayWidth > maxWidth)
            {
                break;
            }

            result += cluster.codePoints;
            width += cluster.displayWidth;
        }

        return result;
    }
}

StatusBar::StatusBar()
    : Panel()
    , m_instructionStyle(UIThemes::StatusBar)
    , m_hintStyle(UIThemes::StatusBar)
{
    setBackgroundStyle(UIThemes::StatusBar);
    setBorderStyle(UIThemes::StatusBar);
    setTitleStyle(UIThemes::StatusBar);
}

StatusBar::StatusBar(const Rect& bounds)
    : Panel(bounds)
    , m_instructionStyle(UIThemes::StatusBar)
    , m_hintStyle(UIThemes::StatusBar)
{
    setBackgroundStyle(UIThemes::StatusBar);
    setBorderStyle(UIThemes::StatusBar);
    setTitleStyle(UIThemes::StatusBar);
}

const std::string& StatusBar::instructions() const
{
    return m_instructions;
}

void StatusBar::setInstructions(std::string instructions)
{
    m_instructions = std::move(instructions);
}

void StatusBar::clearInstructions()
{
    m_instructions.clear();
}

const std::vector<std::string>& StatusBar::commandHints() const
{
    return m_commandHints;
}

void StatusBar::setCommandHints(std::vector<std::string> hints)
{
    m_commandHints = std::move(hints);
}

void StatusBar::addCommandHint(std::string hint)
{
    if (!hint.empty())
    {
        m_commandHints.push_back(std::move(hint));
    }
}

void StatusBar::clearCommandHints()
{
    m_commandHints.clear();
}

const std::string& StatusBar::separator() const
{
    return m_separator;
}

void StatusBar::setSeparator(std::string separator)
{
    m_separator = std::move(separator);
}

const Style& StatusBar::instructionStyle() const
{
    return m_instructionStyle;
}

void StatusBar::setInstructionStyle(const Style& style)
{
    m_instructionStyle = style;
}

const Style& StatusBar::hintStyle() const
{
    return m_hintStyle;
}

void StatusBar::setHintStyle(const Style& style)
{
    m_hintStyle = style;
}

void StatusBar::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    const Rect statusBounds = bounds();

    if (statusBounds.size.width <= 0 || statusBounds.size.height <= 0)
    {
        return;
    }

    ScreenBuffer& buffer = surface.buffer();

    const Style& baseStyle = isEnabled()
        ? backgroundStyle()
        : UIThemes::DisabledText;

    buffer.fillRect(statusBounds, U' ', baseStyle);

    const int textWidth = std::max(0, statusBounds.size.width - 2);
    if (textWidth <= 0)
    {
        return;
    }

    const int textX = statusBounds.position.x + 1;
    const int textY = statusBounds.position.y + (statusBounds.size.height / 2);

    const std::u32string text = buildDisplayText(textWidth);
    if (text.empty())
    {
        return;
    }

    const Style& textStyle = isEnabled()
        ? m_instructionStyle
        : UIThemes::DisabledText;

    buffer.writeText(textX, textY, text, textStyle);
}

std::u32string StatusBar::buildDisplayText(int maxWidth) const
{
    std::string combined = m_instructions;

    for (const std::string& hint : m_commandHints)
    {
        if (hint.empty())
        {
            continue;
        }

        if (!combined.empty())
        {
            combined += m_separator;
        }

        combined += hint;
    }

    return truncateToDisplayWidth(
        UnicodeConversion::utf8ToU32(combined),
        maxWidth);
}