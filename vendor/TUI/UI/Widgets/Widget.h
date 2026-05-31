#pragma once

#include "Core/Rect.h"
#include "UI/Base/UIElement.h"

class Surface;

namespace Input
{
    class Event;
}

class Widget : public UIElement
{
public:
    Widget();
    explicit Widget(const Rect& bounds);
    ~Widget() override;

    bool isFocusable() const;
    void setFocusable(bool focusable);

    bool isFocused() const;
    void setFocused(bool focused);

    bool canReceiveFocus() const;

    void focus();
    void blur();

    virtual bool handleEvent(const Input::Event& event);

protected:
    virtual void onFocusGained();
    virtual void onFocusLost();

private:
    bool m_focusable = false;
    bool m_focused = false;
};