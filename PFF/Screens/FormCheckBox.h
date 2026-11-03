#pragma once
#include "UI/Widgets/Button.h"
#include "Input/Event.h"
#include <functional>

// PFF form checkbox: uses the established Button focus/pointer behavior.
// Brackets are intentional checkbox text, never added to other buttons.
class FormCheckBox final : public Button
{
  public:
    explicit FormCheckBox(std::string caption) : m_caption(std::move(caption))
    {
        setChecked(false);
        setActivationCallback([this](const ButtonActivationResult& r) {
            if (!r.activated)
                return;
            setChecked(!m_checked);
            const auto changed = m_changed;
            if (changed)
                changed(m_checked);
        });
    }
    bool checked() const
    {
        return m_checked;
    }
    void setChecked(bool checked)
    {
        m_checked = checked;
        setLabel(std::string(checked ? "[x] " : "[ ] ") + m_caption);
    }
    void setChangeCallback(std::function<void(bool)> changed)
    {
        m_changed = std::move(changed);
    }
    bool handleEvent(const Input::Event& event) override
    {
        if (const auto* key = event.asKey())
            if (isEnabled() && isVisible() && isFocused() && key->pressed && !key->modifiers.ctrl &&
                !key->modifiers.alt && (key->code == Input::KeyCode::Space || key->character == U' '))
            {
                activate();
                return true;
            }
        return Button::handleEvent(event);
    }

  private:
    std::string m_caption;
    bool m_checked = false;
    std::function<void(bool)> m_changed;
};
