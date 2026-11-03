#include "UI/Widgets/Button.h"

#include <algorithm>
#include <utility>

#include "Input/Command.h"
#include "Input/Event.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Surface.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Unicode/UnicodeConversion.h"

Button::Button()
    : Widget()
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::Button))
{
    setFocusable(true);
}

Button::Button(std::string label)
    : Widget()
    , m_label(std::move(label))
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::Button))
{
    setFocusable(true);
}

Button::Button(const Rect& bounds, std::string label, std::string commandId)
    : Widget(bounds)
    , m_label(std::move(label))
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::Button))
{
    if (!commandId.empty())
    {
        m_commandId = std::move(commandId);
    }

    setFocusable(true);
}

const std::string& Button::label() const
{
    return m_label;
}

void Button::setLabel(std::string_view label)
{
    m_label.assign(label.begin(), label.end());
}

void Button::clearLabel()
{
    m_label.clear();
}

bool Button::hasCommandId() const
{
    return m_commandId.has_value() && !m_commandId->empty();
}

const std::optional<std::string>& Button::commandId() const
{
    return m_commandId;
}

void Button::setCommandId(std::string_view commandId)
{
    if (commandId.empty())
    {
        clearCommandId();
        return;
    }

    m_commandId = std::string(commandId);
}

void Button::clearCommandId()
{
    m_commandId.reset();
}

const Style& Button::normalStyle() const
{
    return m_styleSet.normal;
}

void Button::setNormalStyle(const Style& style)
{
    m_styleSet.normal = style;
}

const Style& Button::focusedStyle() const
{
    return m_styleSet.focused;
}

void Button::setFocusedStyle(const Style& style)
{
    m_styleSet.focused = style;
}

const Style& Button::hoverStyle() const
{
    return m_styleSet.hovered;
}

void Button::setHoverStyle(const Style& style)
{
    m_styleSet.hovered = style;
}

const Style& Button::pressedStyle() const
{
    return m_styleSet.pressed;
}

void Button::setPressedStyle(const Style& style)
{
    m_styleSet.pressed = style;
}

const Style& Button::disabledStyle() const
{
    return m_styleSet.disabled;
}

void Button::setDisabledStyle(const Style& style)
{
    m_styleSet.disabled = style;
}

const WidgetStyles::StyleSet& Button::styleSet() const
{
    return m_styleSet;
}

void Button::setStyleSet(const WidgetStyles::StyleSet& styleSet)
{
    m_styleSet = styleSet;
}

void Button::setActivationCallback(ActivationCallback callback)
{
    m_activationCallback = std::move(callback);
}

void Button::clearActivationCallback()
{
    m_activationCallback = nullptr;
}

ButtonActivationResult Button::activate()
{
    ButtonActivationResult result;
    result.activated = isVisible() && isEnabled();
    result.label = m_label;

    if (m_commandId.has_value())
    {
        result.commandId = *m_commandId;
    }

    if (!result.activated)
    {
        return result;
    }

    m_lastActivationResult = result;

    // Navigation may destroy this button while its callback is running.
    // Keep the callable alive independently of the widget until it returns.
    const ActivationCallback callback = m_activationCallback;
    if (callback)
    {
        callback(result);
    }

    return result;
}

bool Button::hasPendingActivation() const
{
    return m_lastActivationResult.has_value();
}

std::optional<ButtonActivationResult> Button::lastActivationResult() const
{
    return m_lastActivationResult;
}

std::optional<ButtonActivationResult> Button::consumeActivationResult()
{
    std::optional<ButtonActivationResult> result = m_lastActivationResult;
    m_lastActivationResult.reset();
    return result;
}

void Button::draw(Surface& surface)
{
    if (!isVisible())
    {
        return;
    }

    const Rect buttonBounds = bounds();

    if (buttonBounds.size.width <= 0 || buttonBounds.size.height <= 0)
    {
        return;
    }

    const Style& renderStyle = resolveRenderStyle();

    surface.buffer().fillRect(buttonBounds, U' ', renderStyle);

    const std::string displayText = buildDisplayText(buttonBounds.size.width);
    if (displayText.empty())
    {
        return;
    }

    const int textWidth = measureUtf8Width(displayText);
    const int textX = buttonBounds.position.x
        + std::max(0, (buttonBounds.size.width - textWidth) / 2);
    const int textY = buttonBounds.position.y
        + std::max(0, buttonBounds.size.height / 2);

    surface.buffer().writeString(textX, textY, displayText, renderStyle);
}

bool Button::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        m_mouseHovered = false;
        m_leftMousePressedInside = false;
        return false;
    }

    if (const Input::MouseEvent* mouseEvent = event.asMouse())
    {
        const bool inside = bounds().contains(
            mouseEvent->position.x,
            mouseEvent->position.y);

        if (mouseEvent->isMove())
        {
            m_mouseHovered = inside;
            return inside;
        }

        if (mouseEvent->isDrag())
        {
            m_mouseHovered = inside;
            return m_leftMousePressedInside || inside;
        }

        if (mouseEvent->button != Input::MouseButton::Left)
        {
            return false;
        }

        if (mouseEvent->isPress())
        {
            m_mouseHovered = inside;
            m_leftMousePressedInside = inside;
            return inside;
        }

        if (mouseEvent->isRelease())
        {
            const bool shouldActivate = m_leftMousePressedInside && inside;

            m_mouseHovered = inside;
            m_leftMousePressedInside = false;

            if (!shouldActivate)
            {
                return false;
            }

            const ButtonActivationResult result = activate();
            return result.activated;
        }

        if (mouseEvent->isClick())
        {
            m_mouseHovered = inside;
            return inside;
        }

        return false;
    }

    const Input::CommandEvent* commandEvent = event.asCommand();
    if (!commandEvent)
    {
        return false;
    }

    if (commandEvent->command.code != Input::CommandCode::Confirm)
    {
        return false;
    }

    const ButtonActivationResult result = activate();
    return result.activated;
}

void Button::clearPointerState()
{
    m_mouseHovered = false;
    m_leftMousePressedInside = false;
}

const Style& Button::resolveRenderStyle() const
{
    if (!isEnabled())
    {
        return WidgetStyles::resolve(m_styleSet, WidgetStyles::State::Disabled);
    }

    if (m_leftMousePressedInside)
    {
        return WidgetStyles::resolve(m_styleSet, WidgetStyles::State::Pressed);
    }

    if (m_mouseHovered)
    {
        return WidgetStyles::resolve(m_styleSet, WidgetStyles::State::Hovered);
    }

    if (isFocused())
    {
        return WidgetStyles::resolve(m_styleSet, WidgetStyles::State::Focused);
    }

    return WidgetStyles::resolve(m_styleSet, WidgetStyles::State::Normal);
}

std::string Button::buildDisplayText(int availableWidth) const
{
    if (availableWidth <= 0)
    {
        return {};
    }

    return TextClip::clipUtf8Text(m_label, availableWidth);
}

int Button::measureUtf8Width(std::string_view text) const
{
    const std::u32string text32 = UnicodeConversion::utf8ToU32(text);
    return TextClip::measureDisplayWidth(text32);
}
