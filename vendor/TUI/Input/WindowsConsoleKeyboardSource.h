#pragma once

#include <deque>
#include <optional>
#include <string>

#include "Input/RawKeyboardSource.h"

namespace Input
{
    class WindowsConsoleKeyboardSource final : public IRawKeyboardSource
    {
    public:
        WindowsConsoleKeyboardSource() = default;
        ~WindowsConsoleKeyboardSource() override;

        bool initialize() override;
        void shutdown() override;

        std::optional<RawInputEvent> pollRawInput() override;

    private:
        std::optional<RawInputEvent> pollFromConsole();
        std::optional<MouseEvent> tryParseSgrMouseSequence(const std::wstring& sequence) const;
        std::optional<std::wstring> tryReadEscapeSequence(char32_t firstCharacter);
        void emitMouseReportingEnable() const;
        void emitMouseReportingDisable() const;

    private:
        void* m_inputHandle = nullptr;
        unsigned long m_originalMode = 0;
        bool m_initialized = false;
        std::deque<RawInputEvent> m_pendingEvents;
    };
}