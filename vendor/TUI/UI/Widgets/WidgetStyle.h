#pragma once

#include "Rendering/Styles/Style.h"

namespace WidgetStyles
{
    enum class Role
    {
        Label,
        Button,
        ListBoxItem,
        TextViewText
    };

    enum class State
    {
        Normal,
        Focused,
        Disabled,
        Selected,
        SelectedFocused
    };

    struct StyleSet
    {
        Style normal;
        Style focused;
        Style disabled;
        Style selected;
        Style selectedFocused;
    };

    StyleSet defaultStyleSet(Role role);

    const Style& resolve(const StyleSet& styleSet, State state);
    Style resolve(Role role, State state);

    State stateFor(bool enabled, bool focused, bool selected = false);
}