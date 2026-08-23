#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "UI/Base/Viewport.h"
#include "UI/Scrolling/Scrollbar.h"
#include "UI/Widgets/Widget.h"
#include "UI/Widgets/WidgetStyle.h"

class Surface;

namespace Input
{
    class Event;
}

struct ListBoxItem
{
    std::string text;
    bool enabled = true;
};

class ListBox : public Widget
{
public:
    ListBox();
    explicit ListBox(std::vector<ListBoxItem> items);
    ListBox(const Rect& bounds, std::vector<ListBoxItem> items = {});

    std::size_t itemCount() const;
    bool empty() const;

    const std::vector<ListBoxItem>& items() const;
    const ListBoxItem* itemAt(std::size_t index) const;

    void setItems(std::vector<ListBoxItem> items);
    void addItem(std::string_view text, bool enabled = true);
    void addItem(const ListBoxItem& item);
    void clearItems();

    bool isItemEnabled(std::size_t index) const;
    bool setItemEnabled(std::size_t index, bool enabled);

    std::optional<std::size_t> selectedIndex() const;
    const ListBoxItem* selectedItem() const;
    const std::string* selectedText() const;

    bool selectIndex(std::size_t index);
    void clearSelection();

    bool moveNext();
    bool movePrevious();
    bool moveToFirst();
    bool moveToLast();
    bool pageDown();
    bool pageUp();

    const Viewport& viewport() const;
    Viewport& viewport();

    bool isVerticalScrollbarVisible() const;
    void setVerticalScrollbarVisible(bool visible);

    bool reservesVerticalScrollbarColumn() const;
    void setReserveVerticalScrollbarColumn(bool reserveColumn);

    const UI::Scrolling::VerticalScrollbarStyle& verticalScrollbarStyle() const;
    void setVerticalScrollbarStyle(const UI::Scrolling::VerticalScrollbarStyle& style);

    const Style& normalStyle() const;
    void setNormalStyle(const Style& style);

    const Style& focusedStyle() const;
    void setFocusedStyle(const Style& style);

    const Style& selectedStyle() const;
    void setSelectedStyle(const Style& style);

    const Style& selectedFocusedStyle() const;
    void setSelectedFocusedStyle(const Style& style);

    const Style& disabledStyle() const;
    void setDisabledStyle(const Style& style);

    const WidgetStyles::StyleSet& styleSet() const;
    void setStyleSet(const WidgetStyles::StyleSet& styleSet);

    void draw(Surface& surface) override;
    bool handleEvent(const Input::Event& event) override;

private:
    void normalizeSelection();
    void updateViewport();
    void ensureSelectedVisible();

    Rect itemViewportBounds() const;
    Rect scrollbarBounds() const;
    bool shouldDrawVerticalScrollbar() const;
    void drawScrollbarIfNeeded(Surface& surface);

    bool scrollByLines(int lines);

    std::optional<std::size_t> itemIndexFromMousePosition(Point position) const;
    std::optional<std::size_t> findNextEnabled(std::size_t startIndex) const;
    std::optional<std::size_t> findPreviousEnabled(std::size_t startIndex) const;
    std::optional<std::size_t> firstEnabledIndex() const;
    std::optional<std::size_t> lastEnabledIndex() const;

    const Style& resolveItemStyle(std::size_t index) const;

private:
    std::vector<ListBoxItem> m_items;
    std::optional<std::size_t> m_selectedIndex;
    Viewport m_viewport;

    bool m_verticalScrollbarVisible = false;
    bool m_reserveVerticalScrollbarColumn = true;
    UI::Scrolling::VerticalScrollbarStyle m_verticalScrollbarStyle;

    WidgetStyles::StyleSet m_styleSet;
};