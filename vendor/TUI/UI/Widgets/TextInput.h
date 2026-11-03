#pragma once

#include <cstddef>
#include <functional>
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
    struct KeyEvent;
    struct MouseEvent;
}

/*
    TextInput

    A reusable single-line text editor widget.

    Design goals for the first implementation:
        - integrate with the existing Widget / ContainerWidget focus model
        - consume normal text editing locally rather than teaching screens how
          to edit strings
        - preserve UTF-8 at the public API boundary
        - edit by grapheme cluster so cursor movement and deletion do not split
          combining sequences or joined emoji
        - scroll horizontally when the text is wider than the widget

    Intentionally deferred:
        - text selection
        - clipboard operations
        - undo / redo
        - multiline editing
*/
class TextInput : public Widget
{
public:
    using ChangeCallback = std::function<void(const std::string&)>;

    TextInput();
    explicit TextInput(std::string text);
    TextInput(const Rect& bounds, std::string text = {});

    // UTF-8 public text API. Internally the widget stores UTF-32 so editing can
    // operate on Unicode code points and grapheme-cluster boundaries safely.
    [[nodiscard]] std::string text() const;
    void setText(std::string_view text);
    void clear();
    [[nodiscard]] bool empty() const noexcept;

    // Number of user-visible grapheme clusters, not UTF-8 bytes.
    [[nodiscard]] std::size_t length() const;

    const std::string& placeholder() const noexcept;
    void setPlaceholder(std::string_view placeholder);
    void clearPlaceholder();

    // 0 means unlimited. Length is measured in grapheme clusters.
    [[nodiscard]] std::size_t maxLength() const noexcept;
    void setMaxLength(std::size_t maxLength);

    // Cursor position is expressed as a grapheme-cluster boundary in the range
    // [0, length()]. A position equal to length() means "after the last item".
    [[nodiscard]] std::size_t cursorPosition() const noexcept;
    void setCursorPosition(std::size_t position);
    void moveCursorToStart();
    void moveCursorToEnd();
    bool moveCursorLeft();
    bool moveCursorRight();

    bool insertCharacter(char32_t character);
    bool backspace();
    bool deleteForward();

    const WidgetStyles::StyleSet& styleSet() const noexcept;
    void setStyleSet(const WidgetStyles::StyleSet& styleSet);

    const Style& normalStyle() const;
    void setNormalStyle(const Style& style);

    const Style& focusedStyle() const;
    void setFocusedStyle(const Style& style);

    const Style& hoverStyle() const;
    void setHoverStyle(const Style& style);

    const Style& disabledStyle() const;
    void setDisabledStyle(const Style& style);

    // Hint text uses state-specific styling. Its background always comes from
    // the input's current normal/hovered/focused/disabled style, never the hint style.
    const Style& placeholderNormalStyle() const noexcept;
    void setPlaceholderNormalStyle(const Style& style);
    const Style& placeholderFocusedStyle() const noexcept;
    void setPlaceholderFocusedStyle(const Style& style);

    const Style& cursorStyle() const noexcept;
    void setCursorStyle(const Style& style);

    void setChangeCallback(ChangeCallback callback);
    void clearChangeCallback();

    void draw(Surface& surface) override;
    bool handleEvent(const Input::Event& event) override;
    void clearPointerState() override;

protected:
    void onFocusGained() override;

private:
    bool handleKeyEvent(const Input::KeyEvent& event);
    bool handleCommandEvent(const Input::Event& event);
    bool handleMouseEvent(const Input::MouseEvent& event);

    [[nodiscard]] std::size_t clusterCount() const;
    [[nodiscard]] std::size_t codePointOffsetForCursor() const;
    [[nodiscard]] std::size_t cursorForCodePointOffset(std::size_t codePointOffset) const;

    void normalizeCursor();
    void enforceMaxLength();
    void ensureCursorVisible();
    void notifyChanged();

    [[nodiscard]] int cursorDisplayX() const;
    void setCursorFromMouseX(int absoluteX);

    [[nodiscard]] const Style& resolveRenderStyle() const;

    static bool isAcceptedCharacter(char32_t character);

private:
    std::u32string m_text;
    std::string m_placeholder;

    std::size_t m_maxLength = 0;
    std::size_t m_cursorCluster = 0;
    std::size_t m_firstVisibleCluster = 0;

    WidgetStyles::StyleSet m_styleSet;
    Style m_placeholderNormalStyle;
    Style m_placeholderFocusedStyle;
    Style m_cursorStyle;
    bool m_mouseHovered = false;

    ChangeCallback m_changeCallback;
};
