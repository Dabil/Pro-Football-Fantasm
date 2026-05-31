#pragma once

#include <cstddef>
#include <vector>

class UIElement;

class FocusManager
{
public:
    FocusManager();
    ~FocusManager();

    void addFocusTarget(UIElement& element);
    void removeFocusTarget(UIElement& element);
    void clearFocusTargets();

    bool contains(const UIElement& element) const;
    std::size_t count() const;
    bool empty() const;

    bool setFocus(UIElement& element);
    void clearFocus();

    UIElement* focused();
    const UIElement* focused() const;

    bool hasFocus() const;
    bool hasFocus(const UIElement& element) const;

    bool focusNext();
    bool focusPrevious();

private:
    bool isFocusable(const UIElement& element) const;
    int indexOf(const UIElement& element) const;
    bool moveFocus(int direction);

private:
    std::vector<UIElement*> m_focusTargets;
    UIElement* m_focused = nullptr;
};