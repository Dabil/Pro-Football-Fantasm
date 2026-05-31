#include "UI/Widgets/ListBox.h"

#include <algorithm>
#include <utility>

#include "Input/Command.h"
#include "Input/Event.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"
#include "Utilities/Text/TextClip.h"

ListBox::ListBox()
    : Widget()
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::ListBoxItem))
{
    setFocusable(true);
    updateViewport();
}

ListBox::ListBox(std::vector<ListBoxItem> items)
    : Widget()
    , m_items(std::move(items))
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::ListBoxItem))
{
    setFocusable(true);
    normalizeSelection();
    updateViewport();
}

ListBox::ListBox(const Rect& bounds, std::vector<ListBoxItem> items)
    : Widget(bounds)
    , m_items(std::move(items))
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::ListBoxItem))
{
    setFocusable(true);
    normalizeSelection();
    updateViewport();
}

std::size_t ListBox::itemCount() const
{
    return m_items.size();
}

bool ListBox::empty() const
{
    return m_items.empty();
}

const std::vector<ListBoxItem>& ListBox::items() const
{
    return m_items;
}

const ListBoxItem* ListBox::itemAt(std::size_t index) const
{
    if (index >= m_items.size())
    {
        return nullptr;
    }

    return &m_items[index];
}

void ListBox::setItems(std::vector<ListBoxItem> items)
{
    m_items = std::move(items);
    normalizeSelection();
    updateViewport();
    ensureSelectedVisible();
}

void ListBox::addItem(std::string_view text, bool enabled)
{
    ListBoxItem item;
    item.text.assign(text.begin(), text.end());
    item.enabled = enabled;
    addItem(item);
}

void ListBox::addItem(const ListBoxItem& item)
{
    m_items.push_back(item);

    if (!m_selectedIndex.has_value() && item.enabled)
    {
        m_selectedIndex = m_items.size() - 1;
    }

    updateViewport();
    ensureSelectedVisible();
}

void ListBox::clearItems()
{
    m_items.clear();
    m_selectedIndex.reset();
    updateViewport();
}

bool ListBox::isItemEnabled(std::size_t index) const
{
    return index < m_items.size() && m_items[index].enabled;
}

bool ListBox::setItemEnabled(std::size_t index, bool enabled)
{
    if (index >= m_items.size())
    {
        return false;
    }

    m_items[index].enabled = enabled;

    if (m_selectedIndex == index && !enabled)
    {
        const std::optional<std::size_t> next = findNextEnabled(index + 1);
        m_selectedIndex = next.has_value() ? next : findPreviousEnabled(index);
    }
    else if (!m_selectedIndex.has_value() && enabled)
    {
        m_selectedIndex = index;
    }

    ensureSelectedVisible();
    return true;
}

std::optional<std::size_t> ListBox::selectedIndex() const
{
    return m_selectedIndex;
}

const ListBoxItem* ListBox::selectedItem() const
{
    if (!m_selectedIndex.has_value())
    {
        return nullptr;
    }

    return itemAt(*m_selectedIndex);
}

const std::string* ListBox::selectedText() const
{
    const ListBoxItem* item = selectedItem();
    return item != nullptr ? &item->text : nullptr;
}

bool ListBox::selectIndex(std::size_t index)
{
    if (!isItemEnabled(index))
    {
        return false;
    }

    if (m_selectedIndex == index)
    {
        ensureSelectedVisible();
        return true;
    }

    m_selectedIndex = index;
    ensureSelectedVisible();
    return true;
}

void ListBox::clearSelection()
{
    m_selectedIndex.reset();
}

bool ListBox::moveNext()
{
    if (m_items.empty())
    {
        return false;
    }

    const std::size_t startIndex = m_selectedIndex.has_value()
        ? *m_selectedIndex + 1
        : 0;

    const std::optional<std::size_t> next = findNextEnabled(startIndex);
    if (!next.has_value())
    {
        return false;
    }

    return selectIndex(*next);
}

bool ListBox::movePrevious()
{
    if (m_items.empty())
    {
        return false;
    }

    if (!m_selectedIndex.has_value())
    {
        const std::optional<std::size_t> last = lastEnabledIndex();
        return last.has_value() && selectIndex(*last);
    }

    const std::optional<std::size_t> previous = findPreviousEnabled(*m_selectedIndex);
    if (!previous.has_value())
    {
        return false;
    }

    return selectIndex(*previous);
}

bool ListBox::moveToFirst()
{
    const std::optional<std::size_t> first = firstEnabledIndex();
    return first.has_value() && selectIndex(*first);
}

bool ListBox::moveToLast()
{
    const std::optional<std::size_t> last = lastEnabledIndex();
    return last.has_value() && selectIndex(*last);
}

bool ListBox::pageDown()
{
    if (!m_selectedIndex.has_value())
    {
        return moveToFirst();
    }

    const int pageSize = std::max(1, bounds().size.height);
    const std::size_t target = std::min(
        m_items.size() - 1,
        *m_selectedIndex + static_cast<std::size_t>(pageSize));

    if (isItemEnabled(target))
    {
        return selectIndex(target);
    }

    const std::optional<std::size_t> next = findNextEnabled(target);
    if (next.has_value())
    {
        return selectIndex(*next);
    }

    const std::optional<std::size_t> previous = findPreviousEnabled(target + 1);
    return previous.has_value() && previous != m_selectedIndex && selectIndex(*previous);
}

bool ListBox::pageUp()
{
    if (!m_selectedIndex.has_value())
    {
        return moveToFirst();
    }

    const int pageSize = std::max(1, bounds().size.height);
    const std::size_t target = *m_selectedIndex > static_cast<std::size_t>(pageSize)
        ? *m_selectedIndex - static_cast<std::size_t>(pageSize)
        : 0;

    if (isItemEnabled(target))
    {
        return selectIndex(target);
    }

    const std::optional<std::size_t> previous = findPreviousEnabled(target + 1);
    if (previous.has_value())
    {
        return selectIndex(*previous);
    }

    const std::optional<std::size_t> next = findNextEnabled(target);
    return next.has_value() && next != m_selectedIndex && selectIndex(*next);
}

const Viewport& ListBox::viewport() const
{
    return m_viewport;
}

Viewport& ListBox::viewport()
{
    return m_viewport;
}

const Style& ListBox::normalStyle() const
{
    return m_styleSet.normal;
}

void ListBox::setNormalStyle(const Style& style)
{
    m_styleSet.normal = style;
}

const Style& ListBox::focusedStyle() const
{
    return m_styleSet.focused;
}

void ListBox::setFocusedStyle(const Style& style)
{
    m_styleSet.focused = style;
}

const Style& ListBox::selectedStyle() const
{
    return m_styleSet.selected;
}

void ListBox::setSelectedStyle(const Style& style)
{
    m_styleSet.selected = style;
}

const Style& ListBox::selectedFocusedStyle() const
{
    return m_styleSet.selectedFocused;
}

void ListBox::setSelectedFocusedStyle(const Style& style)
{
    m_styleSet.selectedFocused = style;
}

const Style& ListBox::disabledStyle() const
{
    return m_styleSet.disabled;
}

void ListBox::setDisabledStyle(const Style& style)
{
    m_styleSet.disabled = style;
}

const WidgetStyles::StyleSet& ListBox::styleSet() const
{
    return m_styleSet;
}

void ListBox::setStyleSet(const WidgetStyles::StyleSet& styleSet)
{
    m_styleSet = styleSet;
}

void ListBox::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    const Rect listBounds = bounds();
    if (listBounds.size.width <= 0 || listBounds.size.height <= 0)
    {
        return;
    }

    updateViewport();
    surface.buffer().fillRect(listBounds, U' ', m_styleSet.normal);

    const int firstVisibleItem = m_viewport.scrollY();
    const int visibleRows = std::min(
        listBounds.size.height,
        static_cast<int>(m_items.size()) - firstVisibleItem);

    for (int row = 0; row < visibleRows; ++row)
    {
        const std::size_t itemIndex = static_cast<std::size_t>(firstVisibleItem + row);
        const Style& itemStyle = resolveItemStyle(itemIndex);

        const Rect rowRect{
            Point{ listBounds.position.x, listBounds.position.y + row },
            Size{ listBounds.size.width, 1 }
        };

        surface.buffer().fillRect(rowRect, U' ', itemStyle);

        const std::string clippedText = TextClip::clipUtf8Text(
            m_items[itemIndex].text,
            listBounds.size.width);

        if (!clippedText.empty())
        {
            surface.buffer().writeString(
                listBounds.position.x,
                listBounds.position.y + row,
                clippedText,
                itemStyle);
        }
    }
}

bool ListBox::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        return false;
    }

    if (const Input::MouseEvent* mouseEvent = event.asMouse())
    {
        if (mouseEvent->button != Input::MouseButton::Left)
        {
            return false;
        }

        if (!mouseEvent->isPress() && !mouseEvent->isClick())
        {
            return false;
        }

        const std::optional<std::size_t> index =
            itemIndexFromMousePosition(mouseEvent->position);

        if (!index.has_value())
        {
            return false;
        }

        return selectIndex(*index);
    }

    const Input::CommandEvent* commandEvent = event.asCommand();
    if (!commandEvent)
    {
        return false;
    }

    switch (commandEvent->command.code)
    {
    case Input::CommandCode::MoveUp:
        return movePrevious();

    case Input::CommandCode::MoveDown:
        return moveNext();

    case Input::CommandCode::MoveHome:
        return moveToFirst();

    case Input::CommandCode::MoveEnd:
        return moveToLast();

    case Input::CommandCode::PageUp:
        return pageUp();

    case Input::CommandCode::PageDown:
        return pageDown();

    default:
        return false;
    }
}

void ListBox::normalizeSelection()
{
    if (m_selectedIndex.has_value() && isItemEnabled(*m_selectedIndex))
    {
        return;
    }

    m_selectedIndex = firstEnabledIndex();
}

void ListBox::updateViewport()
{
    const Rect listBounds = bounds();
    m_viewport.setContentSize(1, static_cast<int>(m_items.size()));
    m_viewport.setViewSize(1, std::max(0, listBounds.size.height));
}

void ListBox::ensureSelectedVisible()
{
    updateViewport();

    if (!m_selectedIndex.has_value())
    {
        return;
    }

    const int selected = static_cast<int>(*m_selectedIndex);
    const int top = m_viewport.scrollY();
    const int bottom = top + std::max(0, m_viewport.viewSize().height) - 1;

    if (selected < top)
    {
        m_viewport.scrollTo(0, selected);
    }
    else if (selected > bottom)
    {
        m_viewport.scrollTo(0, selected - std::max(0, m_viewport.viewSize().height) + 1);
    }
}

std::optional<std::size_t> ListBox::itemIndexFromMousePosition(Point position) const
{
    const Rect listBounds = bounds();

    if (!listBounds.contains(position.x, position.y))
    {
        return std::nullopt;
    }

    const int localRow = position.y - listBounds.position.y;
    const int itemIndex = m_viewport.scrollY() + localRow;

    if (itemIndex < 0 || itemIndex >= static_cast<int>(m_items.size()))
    {
        return std::nullopt;
    }

    const std::size_t index = static_cast<std::size_t>(itemIndex);

    if (!isItemEnabled(index))
    {
        return std::nullopt;
    }

    return index;
}

std::optional<std::size_t> ListBox::findNextEnabled(std::size_t startIndex) const
{
    for (std::size_t index = startIndex; index < m_items.size(); ++index)
    {
        if (m_items[index].enabled)
        {
            return index;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> ListBox::findPreviousEnabled(std::size_t startIndex) const
{
    if (m_items.empty())
    {
        return std::nullopt;
    }

    std::size_t index = std::min(startIndex, m_items.size());
    while (index > 0)
    {
        --index;
        if (m_items[index].enabled)
        {
            return index;
        }
    }

    return std::nullopt;
}

std::optional<std::size_t> ListBox::firstEnabledIndex() const
{
    return findNextEnabled(0);
}

std::optional<std::size_t> ListBox::lastEnabledIndex() const
{
    return findPreviousEnabled(m_items.size());
}

const Style& ListBox::resolveItemStyle(std::size_t index) const
{
    const bool selected = m_selectedIndex.has_value() && *m_selectedIndex == index;

    return WidgetStyles::resolve(
        m_styleSet,
        WidgetStyles::stateFor(isEnabled() && isItemEnabled(index), isFocused(), selected));
}