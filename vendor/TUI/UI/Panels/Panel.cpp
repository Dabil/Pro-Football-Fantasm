#include "UI/Panels/Panel.h"

#include <algorithm>
#include <utility>

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/AppThemes.h"
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

Panel::Panel()
    : Widget()
    , m_backgroundStyle(AppThemes::Panel)
    , m_borderStyle(UIThemes::Border)
    , m_titleStyle(UIThemes::PanelTitle)
{
    setFocusable(false);
}

Panel::Panel(const Rect& bounds)
    : Widget(bounds)
    , m_backgroundStyle(AppThemes::Panel)
    , m_borderStyle(UIThemes::Border)
    , m_titleStyle(UIThemes::PanelTitle)
{
    setFocusable(false);
}

Panel::Panel(const Rect& bounds, std::string title)
    : Widget(bounds)
    , m_title(std::move(title))
    , m_backgroundStyle(AppThemes::Panel)
    , m_borderStyle(UIThemes::Border)
    , m_titleStyle(UIThemes::PanelTitle)
{
    setFocusable(false);
}

const std::string& Panel::title() const
{
    return m_title;
}

void Panel::setTitle(std::string title)
{
    m_title = std::move(title);
}

bool Panel::hasTitle() const
{
    return !m_title.empty();
}

bool Panel::isBorderVisible() const
{
    return m_borderVisible;
}

void Panel::setBorderVisible(bool visible)
{
    m_borderVisible = visible;
}

const Style& Panel::backgroundStyle() const
{
    return m_backgroundStyle;
}

void Panel::setBackgroundStyle(const Style& style)
{
    m_backgroundStyle = style;
}

const Style& Panel::borderStyle() const
{
    return m_borderStyle;
}

void Panel::setBorderStyle(const Style& style)
{
    m_borderStyle = style;
}

const Style& Panel::titleStyle() const
{
    return m_titleStyle;
}

void Panel::setTitleStyle(const Style& style)
{
    m_titleStyle = style;
}

const ObjectFactory::BorderGlyphs& Panel::borderGlyphs() const
{
    return m_borderGlyphs;
}

void Panel::setBorderGlyphs(const ObjectFactory::BorderGlyphs& glyphs)
{
    m_borderGlyphs = glyphs;
}

Rect Panel::contentBounds() const
{
    const Rect panelBounds = bounds();

    if (!m_borderVisible)
    {
        return panelBounds;
    }

    if (panelBounds.size.width <= 2 || panelBounds.size.height <= 2)
    {
        return Rect{
            Point{ panelBounds.position.x, panelBounds.position.y },
            Size{ 0, 0 }
        };
    }

    return Rect{
        Point{ panelBounds.position.x + 1, panelBounds.position.y + 1 },
        Size{ panelBounds.size.width - 2, panelBounds.size.height - 2 }
    };
}

void Panel::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    const Rect panelBounds = bounds();

    if (panelBounds.size.width <= 0 || panelBounds.size.height <= 0)
    {
        return;
    }

    ScreenBuffer& buffer = surface.buffer();

    buffer.fillRect(panelBounds, U' ', m_backgroundStyle);

    if (m_borderVisible && panelBounds.size.width >= 2 && panelBounds.size.height >= 2)
    {
        const Style& frameRenderStyle = isEnabled()
            ? m_borderStyle
            : UIThemes::DisabledText;

        buffer.drawFrame(
            panelBounds,
            frameRenderStyle,
            m_borderGlyphs.topLeft,
            m_borderGlyphs.topRight,
            m_borderGlyphs.bottomLeft,
            m_borderGlyphs.bottomRight,
            m_borderGlyphs.horizontal,
            m_borderGlyphs.vertical);
    }

    if (m_borderVisible)
    {
        drawTitle(surface);
    }
}

void Panel::drawTitle(Surface& surface) const
{
    if (!hasTitle())
    {
        return;
    }

    const Rect panelBounds = bounds();

    if (panelBounds.size.width <= 4 || panelBounds.size.height <= 0)
    {
        return;
    }

    const int titleX = panelBounds.position.x + 2;
    const int titleY = panelBounds.position.y;
    const int maxTitleWidth = std::max(0, panelBounds.size.width - 4);

    std::u32string titleText = UnicodeConversion::utf8ToU32(m_title);
    titleText = truncateToDisplayWidth(titleText, maxTitleWidth);

    if (titleText.empty())
    {
        return;
    }

    const Style& titleRenderStyle = isEnabled()
        ? m_titleStyle
        : UIThemes::DisabledText;

    surface.buffer().writeText(titleX, titleY, titleText, titleRenderStyle);
}