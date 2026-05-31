#include "Input/WindowsConsoleKeyboardSource.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <sstream>
#include <vector>

namespace Input
{
    namespace
    {
        KeyModifiers readModifiers(DWORD controlKeyState)
        {
            KeyModifiers modifiers;
            modifiers.shift = (controlKeyState & SHIFT_PRESSED) != 0;
            modifiers.ctrl =
                (controlKeyState & LEFT_CTRL_PRESSED) != 0 ||
                (controlKeyState & RIGHT_CTRL_PRESSED) != 0;
            modifiers.alt =
                (controlKeyState & LEFT_ALT_PRESSED) != 0 ||
                (controlKeyState & RIGHT_ALT_PRESSED) != 0;

            return modifiers;
        }

        KeyModifiers readSgrModifiers(int buttonCode)
        {
            KeyModifiers modifiers;
            modifiers.shift = (buttonCode & 4) != 0;
            modifiers.alt = (buttonCode & 8) != 0;
            modifiers.ctrl = (buttonCode & 16) != 0;
            return modifiers;
        }

        MouseButton mapSgrButton(int buttonCode)
        {
            if ((buttonCode & 64) != 0)
            {
                return (buttonCode & 1) == 0
                    ? MouseButton::WheelUp
                    : MouseButton::WheelDown;
            }

            switch (buttonCode & 3)
            {
            case 0: return MouseButton::Left;
            case 1: return MouseButton::Middle;
            case 2: return MouseButton::Right;
            default: return MouseButton::None;
            }
        }

        MouseButtonState buttonStateFor(MouseButton button, bool pressed)
        {
            MouseButtonState state;

            if (!pressed)
            {
                return state;
            }

            switch (button)
            {
            case MouseButton::Left:
                state.left = true;
                break;
            case MouseButton::Right:
                state.right = true;
                break;
            case MouseButton::Middle:
                state.middle = true;
                break;
            default:
                break;
            }

            return state;
        }

        bool isSgrMouseTerminator(wchar_t ch)
        {
            return ch == L'M' || ch == L'm';
        }

        bool isCsiKeyboardTerminator(wchar_t ch)
        {
            return ch == L'A'
                || ch == L'B'
                || ch == L'C'
                || ch == L'D';
        }

        bool isLikelyEscapeSequenceCharacter(wchar_t ch)
        {
            return ch == L'\x1B'
                || ch == L'['
                || ch == L'<'
                || ch == L';'
                || ch == L'M'
                || ch == L'm'
                || ch == L'A'
                || ch == L'B'
                || ch == L'C'
                || ch == L'D'
                || ch == L'Z'
                || std::iswdigit(ch) != 0;
        }

        std::optional<int> parseInt(const std::wstring& text)
        {
            if (text.empty())
            {
                return std::nullopt;
            }

            int value = 0;
            for (wchar_t ch : text)
            {
                if (ch < L'0' || ch > L'9')
                {
                    return std::nullopt;
                }

                value = (value * 10) + static_cast<int>(ch - L'0');
            }

            return value;
        }
    }

    WindowsConsoleKeyboardSource::~WindowsConsoleKeyboardSource()
    {
        shutdown();
    }

    bool WindowsConsoleKeyboardSource::initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        {
            return false;
        }

        DWORD mode = 0;
        if (!GetConsoleMode(handle, &mode))
        {
            return false;
        }

        m_inputHandle = handle;
        m_originalMode = mode;

        DWORD rawMode = mode;

        rawMode &= ~ENABLE_LINE_INPUT;
        rawMode &= ~ENABLE_ECHO_INPUT;

        // Keep processed input enabled so normal console key behavior remains stable.
        // Do NOT clear ENABLE_PROCESSED_INPUT here.

        rawMode |= ENABLE_WINDOW_INPUT;
        rawMode |= ENABLE_MOUSE_INPUT;
        rawMode |= ENABLE_VIRTUAL_TERMINAL_INPUT;

        if (!SetConsoleMode(handle, rawMode))
        {
            m_inputHandle = nullptr;
            m_originalMode = 0;
            return false;
        }

        emitMouseReportingEnable();

        m_initialized = true;
        return true;
    }

    void WindowsConsoleKeyboardSource::shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        emitMouseReportingDisable();

        HANDLE handle = static_cast<HANDLE>(m_inputHandle);
        if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
        {
            SetConsoleMode(handle, m_originalMode);
        }

        m_pendingEvents.clear();
        m_inputHandle = nullptr;
        m_originalMode = 0;
        m_initialized = false;
    }

    std::optional<RawInputEvent> WindowsConsoleKeyboardSource::pollRawInput()
    {
        if (!m_pendingEvents.empty())
        {
            RawInputEvent event = m_pendingEvents.front();
            m_pendingEvents.pop_front();
            return event;
        }

        return pollFromConsole();
    }

    std::optional<RawInputEvent> WindowsConsoleKeyboardSource::pollFromConsole()
    {
        if (!m_initialized)
        {
            return std::nullopt;
        }

        HANDLE handle = static_cast<HANDLE>(m_inputHandle);

        while (true)
        {
            DWORD eventCount = 0;
            if (!GetNumberOfConsoleInputEvents(handle, &eventCount) || eventCount == 0)
            {
                return std::nullopt;
            }

            INPUT_RECORD record{};
            DWORD recordsRead = 0;
            if (!ReadConsoleInputW(handle, &record, 1, &recordsRead) || recordsRead == 0)
            {
                return std::nullopt;
            }

            if (record.EventType != KEY_EVENT)
            {
                continue;
            }

            const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
            if (key.bKeyDown == FALSE)
            {
                continue;
            }

            const char32_t character = static_cast<char32_t>(key.uChar.UnicodeChar);

            // Only attempt SGR parsing when the sequence starts ESC [ <
            if (character == U'\x1B')
            {
                std::optional<std::wstring> sequence = tryReadEscapeSequence(character);

                if (sequence.has_value())
                {
                    const std::wstring& value = sequence.value();

                    // SGR mouse: ESC [ < b ; x ; y M/m
                    if (value.size() >= 3
                        && value[0] == L'\x1B'
                        && value[1] == L'['
                        && value[2] == L'<')
                    {
                        std::optional<MouseEvent> mouse = tryParseSgrMouseSequence(value);
                        if (mouse.has_value())
                        {
                            return RawInputEvent(mouse.value());
                        }

                        return std::nullopt;
                    }

                    // VT arrow keys generated when ENABLE_VIRTUAL_TERMINAL_INPUT is active.
                    if (value == L"\x1B[A")
                    {
                        RawKeyEvent raw;
                        raw.virtualKey = 0x26; // VK_UP
                        raw.character = U'\0';
                        raw.modifiers = readModifiers(key.dwControlKeyState);
                        raw.pressed = true;
                        raw.repeatCount = 1;
                        return RawInputEvent(raw);
                    }

                    if (value == L"\x1B[B")
                    {
                        RawKeyEvent raw;
                        raw.virtualKey = 0x28; // VK_DOWN
                        raw.character = U'\0';
                        raw.modifiers = readModifiers(key.dwControlKeyState);
                        raw.pressed = true;
                        raw.repeatCount = 1;
                        return RawInputEvent(raw);
                    }

                    if (value == L"\x1B[C")
                    {
                        RawKeyEvent raw;
                        raw.virtualKey = 0x27; // VK_RIGHT
                        raw.character = U'\0';
                        raw.modifiers = readModifiers(key.dwControlKeyState);
                        raw.pressed = true;
                        raw.repeatCount = 1;
                        return RawInputEvent(raw);
                    }

                    if (value == L"\x1B[D")
                    {
                        RawKeyEvent raw;
                        raw.virtualKey = 0x25; // VK_LEFT
                        raw.character = U'\0';
                        raw.modifiers = readModifiers(key.dwControlKeyState);
                        raw.pressed = true;
                        raw.repeatCount = 1;
                        return RawInputEvent(raw);
                    }

                    // VT Shift+Tab / Backtab: ESC [ Z
                    if (value == L"\x1B[Z")
                    {
                        RawKeyEvent raw;
                        raw.virtualKey = 0x09; // VK_TAB
                        raw.character = U'\0';
                        raw.modifiers = readModifiers(key.dwControlKeyState);
                        raw.modifiers.shift = true;
                        raw.pressed = true;
                        raw.repeatCount = 1;
                        return RawInputEvent(raw);
                    }

                    // Do not convert unknown CSI sequences into Escape.
                    // That was causing arrow keys to close the app.
                    if (value.size() > 1)
                    {
                        return std::nullopt;
                    }
                }

                RawKeyEvent raw;
                raw.virtualKey = 0x1B;
                raw.character = U'\x1B';
                raw.modifiers = readModifiers(key.dwControlKeyState);
                raw.pressed = true;
                raw.repeatCount = 1;
                return RawInputEvent(raw);
            }

            RawKeyEvent raw;
            raw.virtualKey = static_cast<std::uint16_t>(key.wVirtualKeyCode);
            raw.character = character;
            raw.modifiers = readModifiers(key.dwControlKeyState);
            raw.pressed = true;
            raw.repeatCount = static_cast<std::uint16_t>(key.wRepeatCount == 0 ? 1 : key.wRepeatCount);

            return RawInputEvent(raw);
        }
    }

    std::optional<std::wstring> WindowsConsoleKeyboardSource::tryReadEscapeSequence(char32_t firstCharacter)
    {
        std::wstring sequence;
        sequence.push_back(static_cast<wchar_t>(firstCharacter));

        HANDLE handle = static_cast<HANDLE>(m_inputHandle);

        while (sequence.size() < 32)
        {
            DWORD eventCount = 0;
            if (!GetNumberOfConsoleInputEvents(handle, &eventCount) || eventCount == 0)
            {
                break;
            }

            INPUT_RECORD peek{};
            DWORD recordsPeeked = 0;
            if (!PeekConsoleInputW(handle, &peek, 1, &recordsPeeked) || recordsPeeked == 0)
            {
                break;
            }

            if (peek.EventType != KEY_EVENT || peek.Event.KeyEvent.bKeyDown == FALSE)
            {
                break;
            }

            const wchar_t ch = peek.Event.KeyEvent.uChar.UnicodeChar;
            if (!isLikelyEscapeSequenceCharacter(ch))
            {
                break;
            }

            INPUT_RECORD consumed{};
            DWORD recordsRead = 0;
            if (!ReadConsoleInputW(handle, &consumed, 1, &recordsRead) || recordsRead == 0)
            {
                break;
            }

            sequence.push_back(ch);

            if (isSgrMouseTerminator(ch) || isCsiKeyboardTerminator(ch))
            {
                break;
            }
        }

        return sequence;
    }

    std::optional<MouseEvent> WindowsConsoleKeyboardSource::tryParseSgrMouseSequence(const std::wstring& sequence) const
    {
        // Expected SGR format:
        // ESC [ < b ; x ; y M
        // ESC [ < b ; x ; y m
        //
        // Notes:
        // - 1000 reports button press/release.
        // - 1002 reports motion while a button is pressed.
        // - 1003 reports any mouse motion, including passive hover movement.
        // - 1006 keeps the extended SGR coordinate format.
        //
        // Passive any-motion commonly arrives as:
        // ESC [ < 35 ; x ; y M
        //
        // In that case:
        // - bit 32 is set, meaning motion.
        // - button bits are 3, meaning no button.
        // - this should become MouseAction::Moved with MouseButton::None.

        if (sequence.size() < 9)
        {
            return std::nullopt;
        }

        if (sequence[0] != L'\x1B' || sequence[1] != L'[' || sequence[2] != L'<')
        {
            return std::nullopt;
        }

        const wchar_t terminator = sequence.back();
        if (!isSgrMouseTerminator(terminator))
        {
            return std::nullopt;
        }

        const std::wstring payload = sequence.substr(3, sequence.size() - 4);

        std::vector<std::wstring> parts;
        std::wstring current;

        for (wchar_t ch : payload)
        {
            if (ch == L';')
            {
                parts.push_back(current);
                current.clear();
            }
            else
            {
                current.push_back(ch);
            }
        }

        parts.push_back(current);

        if (parts.size() != 3)
        {
            return std::nullopt;
        }

        std::optional<int> buttonCode = parseInt(parts[0]);
        std::optional<int> x = parseInt(parts[1]);
        std::optional<int> y = parseInt(parts[2]);

        if (!buttonCode.has_value() || !x.has_value() || !y.has_value())
        {
            return std::nullopt;
        }

        if (x.value() <= 0 || y.value() <= 0)
        {
            return std::nullopt;
        }

        MouseEvent event;
        event.position.x = x.value() - 1;
        event.position.y = y.value() - 1;
        event.modifiers = readSgrModifiers(buttonCode.value());

        const bool release = terminator == L'm';
        const bool wheel = (buttonCode.value() & 64) != 0;
        const bool motion = (buttonCode.value() & 32) != 0;

        event.button = mapSgrButton(buttonCode.value());

        if (wheel)
        {
            event.action = MouseAction::Wheel;
            event.wheelDelta = event.button == MouseButton::WheelUp ? 1 : -1;
            event.buttons = MouseButtonState{};
            return event;
        }

        if (release)
        {
            event.action = MouseAction::Released;
            event.buttons = MouseButtonState{};
            return event;
        }

        if (motion)
        {
            event.action = event.button == MouseButton::None
                ? MouseAction::Moved
                : MouseAction::Dragged;

            event.buttons = buttonStateFor(event.button, event.button != MouseButton::None);
            return event;
        }

        event.action = MouseAction::Pressed;
        event.buttons = buttonStateFor(event.button, true);
        event.clickCount = 1;

        return event;
    }

    void WindowsConsoleKeyboardSource::emitMouseReportingEnable() const
    {
        // 1000: button press/release reporting
        // 1002: button-event motion reporting
        // 1003: any-motion reporting, including passive hover movement
        // 1006: SGR extended mouse format
        std::cout << "\x1b[?1000h"
            << "\x1b[?1002h"
            << "\x1b[?1003h"
            << "\x1b[?1006h"
            << std::flush;
    }

    void WindowsConsoleKeyboardSource::emitMouseReportingDisable() const
    {
        std::cout << "\x1b[?1003l"
            << "\x1b[?1002l"
            << "\x1b[?1000l"
            << "\x1b[?1006l"
            << std::flush;
    }
}