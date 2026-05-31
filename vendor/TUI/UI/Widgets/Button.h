#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "UI/Widgets/Widget.h"
#include "UI/Widgets/WidgetStyle.h"

class Surface;

namespace Input
{
    class Event;
}

struct ButtonActivationResult
{
    bool activated = false;
    std::string commandId;
    std::string label;
};

class Button : public Widget
{
public:
    using ActivationCallback = std::function<void(const ButtonActivationResult&)>;

    Button();
    explicit Button(std::string label);
    Button(const Rect& bounds, std::string label = {}, std::string commandId = {});

    const std::string& label() const;
    void setLabel(std::string_view label);
    void clearLabel();

    bool hasCommandId() const;
    const std::optional<std::string>& commandId() const;
    void setCommandId(std::string_view commandId);
    void clearCommandId();

    const Style& normalStyle() const;
    void setNormalStyle(const Style& style);

    const Style& focusedStyle() const;
    void setFocusedStyle(const Style& style);

    const Style& disabledStyle() const;
    void setDisabledStyle(const Style& style);

    const WidgetStyles::StyleSet& styleSet() const;
    void setStyleSet(const WidgetStyles::StyleSet& styleSet);

    void setActivationCallback(ActivationCallback callback);
    void clearActivationCallback();

    ButtonActivationResult activate();

    bool hasPendingActivation() const;
    std::optional<ButtonActivationResult> lastActivationResult() const;
    std::optional<ButtonActivationResult> consumeActivationResult();

    void draw(Surface& surface) override;
    bool handleEvent(const Input::Event& event) override;

private:
    const Style& resolveRenderStyle() const;
    std::string buildDisplayText(int availableWidth) const;
    int measureUtf8Width(std::string_view text) const;

private:
    std::string m_label;
    std::optional<std::string> m_commandId;

    WidgetStyles::StyleSet m_styleSet;

    ActivationCallback m_activationCallback;
    std::optional<ButtonActivationResult> m_lastActivationResult;

    bool m_leftMousePressedInside = false;
};