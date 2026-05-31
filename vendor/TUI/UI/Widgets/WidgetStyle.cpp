#include "UI/Widgets/WidgetStyle.h"

#include "Rendering/Styles/UIThemes.h"

namespace WidgetStyles
{
    StyleSet defaultStyleSet(Role role)
    {
        switch (role)
        {
        case Role::Label:
            return StyleSet{
                UIThemes::WidgetLabelNormal,
                UIThemes::WidgetLabelFocused,
                UIThemes::WidgetLabelDisabled,
                UIThemes::WidgetLabelSelected,
                UIThemes::WidgetLabelSelectedFocused
            };

        case Role::Button:
            return StyleSet{
                UIThemes::WidgetButtonNormal,
                UIThemes::WidgetButtonFocused,
                UIThemes::WidgetButtonDisabled,
                UIThemes::WidgetButtonSelected,
                UIThemes::WidgetButtonSelectedFocused
            };

        case Role::ListBoxItem:
            return StyleSet{
                UIThemes::WidgetListItemNormal,
                UIThemes::WidgetListItemFocused,
                UIThemes::WidgetListItemDisabled,
                UIThemes::WidgetListItemSelected,
                UIThemes::WidgetListItemSelectedFocused
            };

        case Role::TextViewText:
            return StyleSet{
                UIThemes::WidgetTextNormal,
                UIThemes::WidgetTextFocused,
                UIThemes::WidgetTextDisabled,
                UIThemes::WidgetTextSelected,
                UIThemes::WidgetTextSelectedFocused
            };
        }

        return StyleSet{};
    }

    const Style& resolve(const StyleSet& styleSet, State state)
    {
        switch (state)
        {
        case State::Disabled:
            return styleSet.disabled;

        case State::SelectedFocused:
            return styleSet.selectedFocused;

        case State::Selected:
            return styleSet.selected;

        case State::Focused:
            return styleSet.focused;

        case State::Normal:
        default:
            return styleSet.normal;
        }
    }

    Style resolve(Role role, State state)
    {
        return resolve(defaultStyleSet(role), state);
    }

    State stateFor(bool enabled, bool focused, bool selected)
    {
        if (!enabled)
        {
            return State::Disabled;
        }

        if (selected && focused)
        {
            return State::SelectedFocused;
        }

        if (selected)
        {
            return State::Selected;
        }

        if (focused)
        {
            return State::Focused;
        }

        return State::Normal;
    }
}