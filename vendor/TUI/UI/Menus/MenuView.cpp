#include "UI/Menus/MenuView.h"

#include <algorithm>
#include <utility>

#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/UIThemes.h"
#include "Rendering/Surface.h"
#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    int displayWidth(const std::u32string& text)
    {
        int width = 0;

        for (const TextCluster& cluster : GraphemeSegmentation::segment(text))
        {
            width += std::max(0, cluster.displayWidth);
        }

        return width;
    }

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

namespace UI::Menus
{
    MenuView::MenuView()
        : UIElement()
        , m_itemStyle(UIThemes::Label)
        , m_selectedStyle(UIThemes::Selection)
        , m_focusedStyle(UIThemes::Focused)
        , m_disabledStyle(UIThemes::DisabledText)
    {
    }

    MenuView::MenuView(const Rect& bounds)
        : UIElement(bounds)
        , m_itemStyle(UIThemes::Label)
        , m_selectedStyle(UIThemes::Selection)
        , m_focusedStyle(UIThemes::Focused)
        , m_disabledStyle(UIThemes::DisabledText)
    {
    }

    MenuView::MenuView(const Rect& bounds, const MenuModel& model)
        : UIElement(bounds)
        , m_model(&model)
        , m_itemStyle(UIThemes::Label)
        , m_selectedStyle(UIThemes::Selection)
        , m_focusedStyle(UIThemes::Focused)
        , m_disabledStyle(UIThemes::DisabledText)
    {
    }

    const MenuModel* MenuView::model() const
    {
        return m_model;
    }

    void MenuView::setModel(const MenuModel* model)
    {
        m_model = model;
    }

    void MenuView::setModel(const MenuModel& model)
    {
        m_model = &model;
    }

    void MenuView::clearModel()
    {
        m_model = nullptr;
        m_selectedIndex.reset();
    }

    MenuOrientation MenuView::orientation() const
    {
        return m_orientation;
    }

    void MenuView::setOrientation(MenuOrientation orientation)
    {
        m_orientation = orientation;
    }

    std::optional<std::size_t> MenuView::selectedIndex() const
    {
        return m_selectedIndex;
    }

    void MenuView::setSelectedIndex(std::optional<std::size_t> index)
    {
        m_selectedIndex = index;
    }

    void MenuView::clearSelectedIndex()
    {
        m_selectedIndex.reset();
    }

    bool MenuView::hasFocus() const
    {
        return m_hasFocus;
    }

    void MenuView::setHasFocus(bool hasFocus)
    {
        m_hasFocus = hasFocus;
    }

    int MenuView::itemSpacing() const
    {
        return m_itemSpacing;
    }

    void MenuView::setItemSpacing(int spacing)
    {
        m_itemSpacing = std::max(0, spacing);
    }

    int MenuView::horizontalItemPadding() const
    {
        return m_horizontalItemPadding;
    }

    void MenuView::setHorizontalItemPadding(int padding)
    {
        m_horizontalItemPadding = std::max(0, padding);
    }

    bool MenuView::showCheckedIndicator() const
    {
        return m_showCheckedIndicator;
    }

    void MenuView::setShowCheckedIndicator(bool showIndicator)
    {
        m_showCheckedIndicator = showIndicator;
    }

    const Style& MenuView::itemStyle() const
    {
        return m_itemStyle;
    }

    void MenuView::setItemStyle(const Style& style)
    {
        m_itemStyle = style;
    }

    const Style& MenuView::selectedStyle() const
    {
        return m_selectedStyle;
    }

    void MenuView::setSelectedStyle(const Style& style)
    {
        m_selectedStyle = style;
    }

    const Style& MenuView::focusedStyle() const
    {
        return m_focusedStyle;
    }

    void MenuView::setFocusedStyle(const Style& style)
    {
        m_focusedStyle = style;
    }

    const Style& MenuView::disabledStyle() const
    {
        return m_disabledStyle;
    }

    void MenuView::setDisabledStyle(const Style& style)
    {
        m_disabledStyle = style;
    }

    void MenuView::draw(Surface& surface)
    {
        if (!isVisible() || !hasValidModel())
        {
            return;
        }

        const Rect viewBounds = bounds();
        if (viewBounds.size.width <= 0 || viewBounds.size.height <= 0)
        {
            return;
        }

        if (m_orientation == MenuOrientation::Vertical)
        {
            drawVertical(surface);
        }
        else
        {
            drawHorizontal(surface);
        }
    }

    void MenuView::drawVertical(Surface& surface) const
    {
        const Rect viewBounds = bounds();
        const std::vector<MenuItem>& items = m_model->items();

        int y = viewBounds.position.y;

        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (y >= viewBounds.position.y + viewBounds.size.height)
            {
                break;
            }

            drawItem(
                surface,
                items[index],
                index,
                viewBounds.position.x,
                y,
                viewBounds.size.width,
                true);

            y += 1 + m_itemSpacing;
        }
    }

    void MenuView::drawHorizontal(Surface& surface) const
    {
        const Rect viewBounds = bounds();
        const std::vector<MenuItem>& items = m_model->items();

        int x = viewBounds.position.x;
        const int y = viewBounds.position.y;
        const int maxX = viewBounds.position.x + viewBounds.size.width;

        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (x >= maxX)
            {
                break;
            }

            const int remainingWidth = maxX - x;
            const int itemWidth = std::min(measuredItemWidth(items[index]), remainingWidth);

            drawItem(
                surface,
                items[index],
                index,
                x,
                y,
                itemWidth,
                isItemSelected(items[index], index));

            x += itemWidth + m_itemSpacing;
        }
    }

    void MenuView::drawItem(
        Surface& surface,
        const MenuItem& item,
        std::size_t index,
        int x,
        int y,
        int maxWidth,
        bool fillSelection) const
    {
        if (maxWidth <= 0)
        {
            return;
        }

        const Rect viewBounds = bounds();
        if (y < viewBounds.position.y || y >= viewBounds.position.y + viewBounds.size.height)
        {
            return;
        }

        ScreenBuffer& buffer = surface.buffer();
        const Style& itemRenderStyle = isEnabled()
            ? styleForItem(item, index)
            : m_disabledStyle;

        if (fillSelection && isItemSelected(item, index))
        {
            buffer.fillRect(Rect{ Point{ x, y }, Size{ maxWidth, 1 } }, U' ', itemRenderStyle);
        }

        const std::u32string text = buildItemText(item, maxWidth);
        if (text.empty())
        {
            return;
        }

        buffer.writeText(x, y, text, itemRenderStyle);
    }

    std::u32string MenuView::buildItemText(const MenuItem& item, int maxWidth) const
    {
        std::string text;

        if (m_showCheckedIndicator)
        {
            text += item.isChecked() ? "[x] " : "[ ] ";
        }

        if (m_orientation == MenuOrientation::Horizontal && m_horizontalItemPadding > 0)
        {
            text.append(static_cast<std::size_t>(m_horizontalItemPadding), ' ');
        }

        text += item.label();

        if (m_orientation == MenuOrientation::Horizontal && m_horizontalItemPadding > 0)
        {
            text.append(static_cast<std::size_t>(m_horizontalItemPadding), ' ');
        }

        return truncateToDisplayWidth(
            UnicodeConversion::utf8ToU32(text),
            maxWidth);
    }

    int MenuView::measuredItemWidth(const MenuItem& item) const
    {
        return displayWidth(buildItemText(item, 1000000));
    }

    const Style& MenuView::styleForItem(const MenuItem& item, std::size_t index) const
    {
        if (!item.isEnabled())
        {
            return m_disabledStyle;
        }

        if (isItemSelected(item, index))
        {
            return m_hasFocus ? m_selectedStyle : m_focusedStyle;
        }

        return m_itemStyle;
    }

    bool MenuView::isItemSelected(const MenuItem& item, std::size_t index) const
    {
        return item.isSelected()
            || (m_selectedIndex.has_value() && m_selectedIndex.value() == index);
    }

    bool MenuView::hasValidModel() const
    {
        return m_model != nullptr && !m_model->isEmpty();
    }
}

