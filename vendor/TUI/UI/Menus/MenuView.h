#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "UI/Base/UIElement.h"
#include "UI/Menus/MenuModel.h"

class Surface;

namespace UI::Menus
{
    enum class MenuOrientation
    {
        Vertical,
        Horizontal
    };

    class MenuView : public UIElement
    {
    public:
        MenuView();
        explicit MenuView(const Rect& bounds);
        MenuView(const Rect& bounds, const MenuModel& model);

        const MenuModel* model() const;
        void setModel(const MenuModel* model);
        void setModel(const MenuModel& model);
        void clearModel();

        MenuOrientation orientation() const;
        void setOrientation(MenuOrientation orientation);

        std::optional<std::size_t> selectedIndex() const;
        void setSelectedIndex(std::optional<std::size_t> index);
        void clearSelectedIndex();

        bool hasFocus() const;
        void setHasFocus(bool hasFocus);

        int itemSpacing() const;
        void setItemSpacing(int spacing);

        int horizontalItemPadding() const;
        void setHorizontalItemPadding(int padding);

        bool showCheckedIndicator() const;
        void setShowCheckedIndicator(bool showIndicator);

        const Style& itemStyle() const;
        void setItemStyle(const Style& style);

        const Style& selectedStyle() const;
        void setSelectedStyle(const Style& style);

        const Style& focusedStyle() const;
        void setFocusedStyle(const Style& style);

        const Style& disabledStyle() const;
        void setDisabledStyle(const Style& style);

        void draw(Surface& surface) override;

    private:
        void drawVertical(Surface& surface) const;
        void drawHorizontal(Surface& surface) const;

        void drawItem(
            Surface& surface,
            const MenuItem& item,
            std::size_t index,
            int x,
            int y,
            int maxWidth,
            bool fillSelection) const;

        std::u32string buildItemText(const MenuItem& item, int maxWidth) const;
        int measuredItemWidth(const MenuItem& item) const;
        const Style& styleForItem(const MenuItem& item, std::size_t index) const;
        bool isItemSelected(const MenuItem& item, std::size_t index) const;
        bool hasValidModel() const;

    private:
        const MenuModel* m_model = nullptr;
        MenuOrientation m_orientation = MenuOrientation::Vertical;
        std::optional<std::size_t> m_selectedIndex;

        bool m_hasFocus = false;
        bool m_showCheckedIndicator = true;

        int m_itemSpacing = 1;
        int m_horizontalItemPadding = 1;

        Style m_itemStyle;
        Style m_selectedStyle;
        Style m_focusedStyle;
        Style m_disabledStyle;
    };
}

