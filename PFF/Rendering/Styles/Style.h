#pragma once

#include <optional>

#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/ThemeColor.h"

class Style
{
public:
    using AttributeState = std::optional<bool>;

    class StyleColorValue
    {
    public:
        StyleColorValue() = default;
        StyleColorValue(const Color& color);
        StyleColorValue(const ThemeColor& themeColor);

        bool hasConcreteColor() const;
        bool hasThemeColor() const;

        const std::optional<Color>& concreteColor() const;
        const std::optional<ThemeColor>& themeColor() const;

        bool operator==(const StyleColorValue& other) const;
        bool operator!=(const StyleColorValue& other) const;

    private:
        std::optional<Color> m_concreteColor;
        std::optional<ThemeColor> m_themeColor;
    };

public:
    Style();

    const std::optional<Color>& foreground() const;
    const std::optional<Color>& background() const;

    const std::optional<ThemeColor>& foregroundThemeColor() const;
    const std::optional<ThemeColor>& backgroundThemeColor() const;

    const std::optional<StyleColorValue>& foregroundColorValue() const;
    const std::optional<StyleColorValue>& backgroundColorValue() const;

    bool bold() const;
    bool dim() const;
    bool underline() const;
    bool slowBlink() const;
    bool fastBlink() const;
    bool reverse() const;
    bool invisible() const;
    bool strike() const;

    const AttributeState& boldState() const;
    const AttributeState& dimState() const;
    const AttributeState& underlineState() const;
    const AttributeState& slowBlinkState() const;
    const AttributeState& fastBlinkState() const;
    const AttributeState& reverseState() const;
    const AttributeState& invisibleState() const;
    const AttributeState& strikeState() const;

    bool hasForeground() const;
    bool hasBackground() const;

    bool hasForegroundThemeColor() const;
    bool hasBackgroundThemeColor() const;

    bool hasForegroundColorValue() const;
    bool hasBackgroundColorValue() const;

    bool hasBold() const;
    bool hasDim() const;
    bool hasUnderline() const;
    bool hasSlowBlink() const;
    bool hasFastBlink() const;
    bool hasReverse() const;
    bool hasInvisible() const;
    bool hasStrike() const;

    bool isEmpty() const;

    Style withForeground(const Color& color) const;
    Style withForeground(const ThemeColor& themeColor) const;

    Style withBackground(const Color& color) const;
    Style withBackground(const ThemeColor& themeColor) const;

    Style withoutForeground() const;
    Style withoutBackground() const;

    Style withBold(bool value = true) const;
    Style withDim(bool value = true) const;
    Style withUnderline(bool value = true) const;
    Style withSlowBlink(bool value = true) const;
    Style withFastBlink(bool value = true) const;
    Style withReverse(bool value = true) const;
    Style withInvisible(bool value = true) const;
    Style withStrike(bool value = true) const;

    Style withoutBold() const;
    Style withoutDim() const;
    Style withoutUnderline() const;
    Style withoutSlowBlink() const;
    Style withoutFastBlink() const;
    Style withoutReverse() const;
    Style withoutInvisible() const;
    Style withoutStrike() const;

    bool operator==(const Style& other) const;
    bool operator!=(const Style& other) const;

private:
    // Compatibility storage for existing code that still expects
    // direct concrete/theme accessors.
    std::optional<Color> m_foreground;
    std::optional<Color> m_background;

    std::optional<ThemeColor> m_foregroundThemeColor;
    std::optional<ThemeColor> m_backgroundThemeColor;

    // Authoritative authored-intent storage.
    std::optional<StyleColorValue> m_foregroundColorValue;
    std::optional<StyleColorValue> m_backgroundColorValue;

    AttributeState m_bold;
    AttributeState m_dim;
    AttributeState m_underline;
    AttributeState m_slowBlink;
    AttributeState m_fastBlink;
    AttributeState m_reverse;
    AttributeState m_invisible;
    AttributeState m_strike;
};