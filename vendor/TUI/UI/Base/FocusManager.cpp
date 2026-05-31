#include "UI/Base/FocusManager.h"

#include <algorithm>

#include "UI/Base/UIElement.h"

FocusManager::FocusManager() = default;

FocusManager::~FocusManager() = default;

void FocusManager::addFocusTarget(UIElement& element)
{
    if (contains(element))
    {
        return;
    }

    m_focusTargets.push_back(&element);
}

void FocusManager::removeFocusTarget(UIElement& element)
{
    if (m_focused == &element)
    {
        clearFocus();
    }

    m_focusTargets.erase(
        std::remove(
            m_focusTargets.begin(),
            m_focusTargets.end(),
            &element),
        m_focusTargets.end());
}

void FocusManager::clearFocusTargets()
{
    clearFocus();
    m_focusTargets.clear();
}

bool FocusManager::contains(const UIElement& element) const
{
    return indexOf(element) >= 0;
}

std::size_t FocusManager::count() const
{
    return m_focusTargets.size();
}

bool FocusManager::empty() const
{
    return m_focusTargets.empty();
}

bool FocusManager::setFocus(UIElement& element)
{
    if (!contains(element) || !isFocusable(element))
    {
        return false;
    }

    m_focused = &element;
    return true;
}

void FocusManager::clearFocus()
{
    m_focused = nullptr;
}

UIElement* FocusManager::focused()
{
    return m_focused;
}

const UIElement* FocusManager::focused() const
{
    return m_focused;
}

bool FocusManager::hasFocus() const
{
    return m_focused != nullptr;
}

bool FocusManager::hasFocus(const UIElement& element) const
{
    return m_focused == &element;
}

bool FocusManager::focusNext()
{
    return moveFocus(1);
}

bool FocusManager::focusPrevious()
{
    return moveFocus(-1);
}

bool FocusManager::isFocusable(const UIElement& element) const
{
    return element.isVisible() && element.isEnabled();
}

int FocusManager::indexOf(const UIElement& element) const
{
    const auto it = std::find(
        m_focusTargets.begin(),
        m_focusTargets.end(),
        &element);

    if (it == m_focusTargets.end())
    {
        return -1;
    }

    return static_cast<int>(std::distance(m_focusTargets.begin(), it));
}

bool FocusManager::moveFocus(int direction)
{
    if (m_focusTargets.empty())
    {
        clearFocus();
        return false;
    }

    const int count = static_cast<int>(m_focusTargets.size());
    int currentIndex = -1;

    if (m_focused != nullptr)
    {
        currentIndex = indexOf(*m_focused);
    }

    int startIndex = 0;

    if (currentIndex >= 0)
    {
        startIndex = currentIndex;
    }
    else if (direction < 0)
    {
        startIndex = count;
    }
    else
    {
        startIndex = -1;
    }

    for (int offset = 1; offset <= count; ++offset)
    {
        int candidateIndex = startIndex + direction * offset;

        while (candidateIndex < 0)
        {
            candidateIndex += count;
        }

        candidateIndex %= count;

        UIElement* candidate = m_focusTargets[static_cast<std::size_t>(candidateIndex)];

        if (candidate != nullptr && isFocusable(*candidate))
        {
            m_focused = candidate;
            return true;
        }
    }

    clearFocus();
    return false;
}