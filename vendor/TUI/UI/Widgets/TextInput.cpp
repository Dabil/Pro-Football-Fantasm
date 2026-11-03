#include "UI/Widgets/TextInput.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "Input/Command.h"
#include "Input/Event.h"
#include "Input/KeyEvent.h"
#include "Input/MouseEvent.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/UIThemes.h"
#include "Rendering/Styles/StyleMerge.h"
#include "Rendering/Surface.h"
#include "Rendering/Text/TextCluster.h"
#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    int clusterDisplayWidth(const TextCluster& cluster)
    {
        // A standalone zero-width cluster is unusual in authored input, but it
        // still needs a logical cursor position. Give it one layout cell so the
        // editor can scroll/click through it deterministically.
        return std::max(1, cluster.displayWidth);
    }

    std::u32string sanitizeSingleLineText(std::u32string text)
    {
        text.erase(
            std::remove_if(
                text.begin(),
                text.end(),
                [](char32_t codePoint)
                {
                    return codePoint == U'\n'
                        || codePoint == U'\r'
                        || codePoint == U'\t'
                        || codePoint == U'\0';
                }),
            text.end());

        for (char32_t& codePoint : text)
        {
            codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);
        }

        return text;
    }
}

TextInput::TextInput()
    : Widget()
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
    , m_placeholderNormalStyle(UIThemes::DisabledText)
    , m_placeholderFocusedStyle(UIThemes::DisabledText)
    , m_cursorStyle(UIThemes::Selection)
{
    setFocusable(true);
}

TextInput::TextInput(std::string text)
    : TextInput()
{
    setText(text);
}

TextInput::TextInput(const Rect& bounds, std::string text)
    : Widget(bounds)
    , m_styleSet(WidgetStyles::defaultStyleSet(WidgetStyles::Role::TextViewText))
    , m_placeholderNormalStyle(UIThemes::DisabledText)
    , m_placeholderFocusedStyle(UIThemes::DisabledText)
    , m_cursorStyle(UIThemes::Selection)
{
    setFocusable(true);
    setText(text);
}

std::string TextInput::text() const
{
    return UnicodeConversion::u32ToUtf8(m_text);
}

void TextInput::setText(std::string_view text)
{
    std::u32string newText = sanitizeSingleLineText(
        UnicodeConversion::utf8ToU32(text));

    if (m_maxLength > 0)
    {
        const std::vector<TextCluster> clusters =
            GraphemeSegmentation::segment(newText);

        if (clusters.size() > m_maxLength)
        {
            std::u32string truncated;

            for (std::size_t index = 0; index < m_maxLength; ++index)
            {
                truncated.append(clusters[index].codePoints);
            }

            newText = std::move(truncated);
        }
    }

    if (newText == m_text)
    {
        m_cursorCluster = clusterCount();
        ensureCursorVisible();
        return;
    }

    m_text = std::move(newText);
    m_cursorCluster = clusterCount();
    normalizeCursor();
    ensureCursorVisible();
    notifyChanged();
}

void TextInput::clear()
{
    if (m_text.empty())
    {
        m_cursorCluster = 0;
        m_firstVisibleCluster = 0;
        return;
    }

    m_text.clear();
    m_cursorCluster = 0;
    m_firstVisibleCluster = 0;
    notifyChanged();
}

bool TextInput::empty() const noexcept
{
    return m_text.empty();
}

std::size_t TextInput::length() const
{
    return clusterCount();
}

const std::string& TextInput::placeholder() const noexcept
{
    return m_placeholder;
}

void TextInput::setPlaceholder(std::string_view placeholder)
{
    m_placeholder.assign(placeholder.begin(), placeholder.end());
}

void TextInput::clearPlaceholder()
{
    m_placeholder.clear();
}

std::size_t TextInput::maxLength() const noexcept
{
    return m_maxLength;
}

void TextInput::setMaxLength(std::size_t maxLength)
{
    if (m_maxLength == maxLength)
    {
        return;
    }

    const std::u32string previousText = m_text;

    m_maxLength = maxLength;
    enforceMaxLength();
    normalizeCursor();
    ensureCursorVisible();

    if (m_text != previousText)
    {
        notifyChanged();
    }
}

std::size_t TextInput::cursorPosition() const noexcept
{
    return m_cursorCluster;
}

void TextInput::setCursorPosition(std::size_t position)
{
    m_cursorCluster = std::min(position, clusterCount());
    ensureCursorVisible();
}

void TextInput::moveCursorToStart()
{
    m_cursorCluster = 0;
    ensureCursorVisible();
}

void TextInput::moveCursorToEnd()
{
    m_cursorCluster = clusterCount();
    ensureCursorVisible();
}

bool TextInput::moveCursorLeft()
{
    if (m_cursorCluster == 0)
    {
        return false;
    }

    --m_cursorCluster;
    ensureCursorVisible();
    return true;
}

bool TextInput::moveCursorRight()
{
    const std::size_t count = clusterCount();

    if (m_cursorCluster >= count)
    {
        return false;
    }

    ++m_cursorCluster;
    ensureCursorVisible();
    return true;
}

bool TextInput::insertCharacter(char32_t character)
{
    if (!isAcceptedCharacter(character))
    {
        return false;
    }

    character = UnicodeConversion::sanitizeCodePoint(character);

    const std::vector<TextCluster> beforeClusters =
        GraphemeSegmentation::segment(m_text);

    if (m_maxLength > 0 && beforeClusters.size() >= m_maxLength)
    {
        // Combining marks and joiners can extend the previous grapheme without
        // increasing the visible character count. Allow the insertion and let
        // the post-insert cluster count decide whether it exceeded the limit.
        std::u32string trial = m_text;
        const std::size_t trialOffset = codePointOffsetForCursor();
        trial.insert(trial.begin() + static_cast<std::ptrdiff_t>(trialOffset), character);

        if (GraphemeSegmentation::segment(trial).size() > m_maxLength)
        {
            return false;
        }
    }

    const std::size_t insertionOffset = codePointOffsetForCursor();

    m_text.insert(
        m_text.begin() + static_cast<std::ptrdiff_t>(insertionOffset),
        character);

    // Re-segment after insertion. This handles combining marks and ZWJ
    // sequences correctly instead of assuming one code point == one cursor step.
    m_cursorCluster = cursorForCodePointOffset(insertionOffset + 1);

    enforceMaxLength();
    normalizeCursor();
    ensureCursorVisible();
    notifyChanged();
    return true;
}

bool TextInput::backspace()
{
    if (m_cursorCluster == 0 || m_text.empty())
    {
        return false;
    }

    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    if (m_cursorCluster > clusters.size())
    {
        normalizeCursor();
        return false;
    }

    const std::size_t eraseCluster = m_cursorCluster - 1;

    std::size_t eraseOffset = 0;
    for (std::size_t index = 0; index < eraseCluster; ++index)
    {
        eraseOffset += clusters[index].codePoints.size();
    }

    const std::size_t eraseCount =
        clusters[eraseCluster].codePoints.size();

    m_text.erase(eraseOffset, eraseCount);
    --m_cursorCluster;

    normalizeCursor();
    ensureCursorVisible();
    notifyChanged();
    return true;
}

bool TextInput::deleteForward()
{
    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    if (m_cursorCluster >= clusters.size())
    {
        return false;
    }

    std::size_t eraseOffset = 0;
    for (std::size_t index = 0; index < m_cursorCluster; ++index)
    {
        eraseOffset += clusters[index].codePoints.size();
    }

    const std::size_t eraseCount =
        clusters[m_cursorCluster].codePoints.size();

    m_text.erase(eraseOffset, eraseCount);

    normalizeCursor();
    ensureCursorVisible();
    notifyChanged();
    return true;
}

const WidgetStyles::StyleSet& TextInput::styleSet() const noexcept
{
    return m_styleSet;
}

void TextInput::setStyleSet(const WidgetStyles::StyleSet& styleSet)
{
    m_styleSet = styleSet;
}

const Style& TextInput::normalStyle() const
{
    return m_styleSet.normal;
}

void TextInput::setNormalStyle(const Style& style)
{
    m_styleSet.normal = style;
}

const Style& TextInput::focusedStyle() const
{
    return m_styleSet.focused;
}

void TextInput::setFocusedStyle(const Style& style)
{
    m_styleSet.focused = style;
}

const Style& TextInput::hoverStyle() const
{
    return m_styleSet.hovered;
}

void TextInput::setHoverStyle(const Style& style)
{
    m_styleSet.hovered = style;
}

const Style& TextInput::disabledStyle() const
{
    return m_styleSet.disabled;
}

void TextInput::setDisabledStyle(const Style& style)
{
    m_styleSet.disabled = style;
}

const Style& TextInput::placeholderNormalStyle() const noexcept
{
    return m_placeholderNormalStyle;
}

void TextInput::setPlaceholderNormalStyle(const Style& style)
{
    m_placeholderNormalStyle = style;
}

const Style& TextInput::placeholderFocusedStyle() const noexcept
{
    return m_placeholderFocusedStyle;
}

void TextInput::setPlaceholderFocusedStyle(const Style& style)
{
    m_placeholderFocusedStyle = style;
}

const Style& TextInput::cursorStyle() const noexcept
{
    return m_cursorStyle;
}

void TextInput::setCursorStyle(const Style& style)
{
    m_cursorStyle = style;
}

void TextInput::setChangeCallback(ChangeCallback callback)
{
    m_changeCallback = std::move(callback);
}

void TextInput::clearChangeCallback()
{
    m_changeCallback = nullptr;
}

void TextInput::draw(Surface& surface)
{
    if (!isVisible())
    {
        m_mouseHovered = false;
        return;
    }
    if (!isEnabled()) m_mouseHovered = false;

    const Rect inputBounds = bounds();

    if (inputBounds.size.width <= 0 || inputBounds.size.height <= 0)
    {
        return;
    }

    ensureCursorVisible();

    const Style& renderStyle = resolveRenderStyle();
    surface.buffer().fillRect(inputBounds, U' ', renderStyle);

    const int textY = inputBounds.position.y
        + std::max(0, inputBounds.size.height / 2);

    if (m_text.empty() && !m_placeholder.empty())
    {
        const Style& hintTextStyle = isEnabled() && isFocused()
            ? m_placeholderFocusedStyle : m_placeholderNormalStyle;
        // A hint must never paint a different background over the input fill.
        const Style hintStyle = StyleMerge::merge(renderStyle,
            hintTextStyle.withoutBackground(), StyleMergeMode::MergePreserveDestination);
        const std::u32string placeholderText =
            UnicodeConversion::utf8ToU32(m_placeholder);

        int x = inputBounds.position.x;
        const std::vector<TextCluster> placeholderClusters =
            GraphemeSegmentation::segment(placeholderText);

        for (const TextCluster& cluster : placeholderClusters)
        {
            const int width = clusterDisplayWidth(cluster);

            if (x + width > inputBounds.position.x + inputBounds.size.width)
            {
                break;
            }

            surface.buffer().writeText(
                x,
                textY,
                cluster.codePoints,
                hintStyle);

            x += width;
        }
    }
    else
    {
        const std::vector<TextCluster> clusters =
            GraphemeSegmentation::segment(m_text);

        int x = inputBounds.position.x;

        for (std::size_t index = m_firstVisibleCluster;
             index < clusters.size();
             ++index)
        {
            const int width = clusterDisplayWidth(clusters[index]);

            if (x + width > inputBounds.position.x + inputBounds.size.width)
            {
                break;
            }

            surface.buffer().writeText(
                x,
                textY,
                clusters[index].codePoints,
                renderStyle);

            x += width;
        }
    }

    if (!isFocused() || !isEnabled())
    {
        return;
    }

    const int cursorX = cursorDisplayX();
    const int rightEdge = inputBounds.position.x + inputBounds.size.width;

    if (cursorX < inputBounds.position.x || cursorX >= rightEdge)
    {
        return;
    }

    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    if (m_cursorCluster < clusters.size())
    {
        surface.buffer().writeText(
            cursorX,
            textY,
            clusters[m_cursorCluster].codePoints,
            m_cursorStyle);
    }
    else
    {
        // At end-of-text there is no glyph to highlight, so render a styled
        // blank cell as the insertion cursor.
        surface.buffer().writeCodePoint(
            cursorX,
            textY,
            U' ',
            m_cursorStyle);
    }
}

bool TextInput::handleEvent(const Input::Event& event)
{
    if (!isVisible() || !isEnabled())
    {
        m_mouseHovered = false;
        return false;
    }

    if (const Input::MouseEvent* mouseEvent = event.asMouse())
    {
        return handleMouseEvent(*mouseEvent);
    }

    if (!isFocused())
    {
        return false;
    }

    if (const Input::KeyEvent* keyEvent = event.asKey())
    {
        return handleKeyEvent(*keyEvent);
    }

    return handleCommandEvent(event);
}

void TextInput::onFocusGained()
{
    ensureCursorVisible();
}

bool TextInput::handleKeyEvent(const Input::KeyEvent& event)
{
    if (!event.pressed)
    {
        return false;
    }

    const std::uint16_t repeatCount = std::max<std::uint16_t>(1, event.repeatCount);

    switch (event.code)
    {
    case Input::KeyCode::Character:
        if (event.modifiers.ctrl || event.modifiers.alt)
        {
            // Preserve application shortcuts such as Ctrl+Q / Ctrl+R.
            return false;
        }

        if (!isAcceptedCharacter(event.character))
        {
            return false;
        }

        for (std::uint16_t repeat = 0; repeat < repeatCount; ++repeat)
        {
            if (!insertCharacter(event.character))
            {
                break;
            }
        }

        return true;

    case Input::KeyCode::Space:
        if (event.modifiers.ctrl || event.modifiers.alt)
        {
            return false;
        }

        for (std::uint16_t repeat = 0; repeat < repeatCount; ++repeat)
        {
            if (!insertCharacter(U' '))
            {
                break;
            }
        }

        return true;

    case Input::KeyCode::Backspace:
        for (std::uint16_t repeat = 0; repeat < repeatCount; ++repeat)
        {
            if (!backspace())
            {
                break;
            }
        }

        // Consume Backspace even at the beginning of the field.
        return true;

    case Input::KeyCode::Delete:
        for (std::uint16_t repeat = 0; repeat < repeatCount; ++repeat)
        {
            if (!deleteForward())
            {
                break;
            }
        }

        return true;

    default:
        // Navigation is handled from CommandEvent so the raw key and mapped
        // command do not move the cursor twice.
        return false;
    }
}

bool TextInput::handleCommandEvent(const Input::Event& event)
{
    const Input::CommandEvent* commandEvent = event.asCommand();

    if (!commandEvent)
    {
        return false;
    }

    switch (commandEvent->command.code)
    {
    case Input::CommandCode::MoveLeft:
        (void)moveCursorLeft();
        return true;

    case Input::CommandCode::MoveRight:
        (void)moveCursorRight();
        return true;

    case Input::CommandCode::MoveHome:
        moveCursorToStart();
        return true;

    case Input::CommandCode::MoveEnd:
        moveCursorToEnd();
        return true;

    case Input::CommandCode::Confirm:
        // A focused text editor owns Confirm so an unrelated Button in the
        // same ContainerWidget cannot activate. A form/screen that wants Enter
        // to submit should intercept Confirm before forwarding it to the root.
        return true;

    default:
        return false;
    }
}

bool TextInput::handleMouseEvent(const Input::MouseEvent& event)
{
    const bool inside = bounds().contains(event.position.x, event.position.y);
    m_mouseHovered = inside;
    if (event.isMove() || event.isDrag()) return inside;

    if (event.button != Input::MouseButton::Left)
    {
        return false;
    }

    if (!inside)
    {
        return false;
    }

    if (event.isPress() || event.isClick() || event.isRelease())
    {
        setCursorFromMouseX(event.position.x);
        return true;
    }

    return false;
}

std::size_t TextInput::clusterCount() const
{
    return GraphemeSegmentation::segment(m_text).size();
}

std::size_t TextInput::codePointOffsetForCursor() const
{
    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    const std::size_t cursor =
        std::min(m_cursorCluster, clusters.size());

    std::size_t offset = 0;

    for (std::size_t index = 0; index < cursor; ++index)
    {
        offset += clusters[index].codePoints.size();
    }

    return offset;
}

std::size_t TextInput::cursorForCodePointOffset(
    std::size_t codePointOffset) const
{
    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    std::size_t consumed = 0;

    for (std::size_t index = 0; index < clusters.size(); ++index)
    {
        consumed += clusters[index].codePoints.size();

        if (consumed >= codePointOffset)
        {
            return index + 1;
        }
    }

    return clusters.size();
}

void TextInput::normalizeCursor()
{
    const std::size_t count = clusterCount();
    m_cursorCluster = std::min(m_cursorCluster, count);
    m_firstVisibleCluster = std::min(m_firstVisibleCluster, count);
}

void TextInput::enforceMaxLength()
{
    if (m_maxLength == 0)
    {
        return;
    }

    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    if (clusters.size() <= m_maxLength)
    {
        return;
    }

    std::u32string truncated;

    for (std::size_t index = 0; index < m_maxLength; ++index)
    {
        truncated.append(clusters[index].codePoints);
    }

    m_text = std::move(truncated);
}

void TextInput::ensureCursorVisible()
{
    normalizeCursor();

    const int availableWidth = bounds().size.width;

    if (availableWidth <= 0)
    {
        m_firstVisibleCluster = 0;
        return;
    }

    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    if (m_cursorCluster < m_firstVisibleCluster)
    {
        m_firstVisibleCluster = m_cursorCluster;
    }

    auto widthBeforeCursor = [&]()
        {
            int width = 0;

            for (std::size_t index = m_firstVisibleCluster;
                 index < m_cursorCluster && index < clusters.size();
                 ++index)
            {
                width += clusterDisplayWidth(clusters[index]);
            }

            return width;
        };

    // Keep one visible cell for the insertion cursor. This also handles the
    // end-of-text cursor, which otherwise lands exactly one cell past the edge.
    int visibleWidth = widthBeforeCursor();
    while (m_firstVisibleCluster < m_cursorCluster && visibleWidth >= availableWidth)
    {
        visibleWidth -= clusterDisplayWidth(clusters[m_firstVisibleCluster]);
        ++m_firstVisibleCluster;
    }
    // Layout may grow after setText, or deletion may shorten the value. Fill
    // the newly available space instead of retaining an off-screen/end offset.
    while (m_firstVisibleCluster > 0)
    {
        const int previousWidth = clusterDisplayWidth(clusters[m_firstVisibleCluster - 1]);
        if (visibleWidth + previousWidth >= availableWidth) break;
        --m_firstVisibleCluster;
        visibleWidth += previousWidth;
    }

}

void TextInput::notifyChanged()
{
    if (!m_changeCallback)
    {
        return;
    }

    const std::string utf8 = text();
    m_changeCallback(utf8);
}

int TextInput::cursorDisplayX() const
{
    const Rect inputBounds = bounds();
    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    int x = inputBounds.position.x;

    for (std::size_t index = m_firstVisibleCluster;
         index < m_cursorCluster && index < clusters.size();
         ++index)
    {
        x += clusterDisplayWidth(clusters[index]);
    }

    return x;
}

void TextInput::setCursorFromMouseX(int absoluteX)
{
    const std::vector<TextCluster> clusters =
        GraphemeSegmentation::segment(m_text);

    const int localX = std::max(0, absoluteX - bounds().position.x);

    int currentX = 0;
    std::size_t cursor = m_firstVisibleCluster;

    for (std::size_t index = m_firstVisibleCluster;
         index < clusters.size();
         ++index)
    {
        const int width = clusterDisplayWidth(clusters[index]);

        if (localX < currentX + width)
        {
            cursor = index;
            setCursorPosition(cursor);
            return;
        }

        currentX += width;
        cursor = index + 1;

        if (currentX >= bounds().size.width)
        {
            break;
        }
    }

    setCursorPosition(cursor);
}

void TextInput::clearPointerState()
{
    m_mouseHovered = false;
}

const Style& TextInput::resolveRenderStyle() const
{
    if (!isEnabled())
    {
        return WidgetStyles::resolve(
            m_styleSet,
            WidgetStyles::State::Disabled);
    }

    if (m_mouseHovered)
    {
        return WidgetStyles::resolve(m_styleSet, WidgetStyles::State::Hovered);
    }

    if (isFocused())
    {
        return WidgetStyles::resolve(
            m_styleSet,
            WidgetStyles::State::Focused);
    }

    return WidgetStyles::resolve(
        m_styleSet,
        WidgetStyles::State::Normal);
}

bool TextInput::isAcceptedCharacter(char32_t character)
{
    if (character == U'\0' ||
        character == U'\n' ||
        character == U'\r' ||
        character == U'\t')
    {
        return false;
    }

    // Reject C0/C1 controls from a single-line editor. Normal space begins at
    // U+0020 and is accepted.
    if (character < U' ' ||
        (character >= 0x7F && character <= 0x9F))
    {
        return false;
    }

    return true;
}
