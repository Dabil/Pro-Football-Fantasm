#include "Rendering/Backends/TerminalHostClassifier.h"

#define NOMINMAX
#include <windows.h>

#include <cwctype>
#include <string>

namespace
{
    bool hasEnvironmentVariable(const wchar_t* name)
    {
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        return required > 0;
    }

    std::wstring getEnvironmentVariableString(const wchar_t* name)
    {
        const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
        if (required == 0)
        {
            return std::wstring();
        }

        std::wstring value;
        value.resize(required);

        const DWORD length = GetEnvironmentVariableW(name, &value[0], required);
        if (length == 0)
        {
            return std::wstring();
        }

        if (!value.empty() && value.back() == L'\0')
        {
            value.pop_back();
        }

        return value;
    }

    bool equalsIgnoreCase(const std::wstring& left, const wchar_t* right)
    {
        if (right == nullptr)
        {
            return false;
        }

        return _wcsicmp(left.c_str(), right) == 0;
    }

    bool containsIgnoreCase(const std::wstring& haystack, const wchar_t* needle)
    {
        if (needle == nullptr || *needle == L'\0')
        {
            return false;
        }

        std::wstring loweredHaystack = haystack;
        std::wstring loweredNeedle = needle;

        for (wchar_t& ch : loweredHaystack)
        {
            ch = static_cast<wchar_t>(towlower(ch));
        }

        for (wchar_t& ch : loweredNeedle)
        {
            ch = static_cast<wchar_t>(towlower(ch));
        }

        return loweredHaystack.find(loweredNeedle) != std::wstring::npos;
    }
}

TerminalHostKind TerminalHostClassifier::classifyCurrentHost()
{
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
    {
        return TerminalHostKind::Unknown;
    }

    const DWORD fileType = GetFileType(hOut);
    if (fileType != FILE_TYPE_CHAR)
    {
        return TerminalHostKind::RedirectedOrPipe;
    }

    if (hasEnvironmentVariable(L"WT_SESSION"))
    {
        return TerminalHostKind::WindowsTerminal;
    }

    const std::wstring termProgram = getEnvironmentVariableString(L"TERM_PROGRAM");
    if (equalsIgnoreCase(termProgram, L"vscode"))
    {
        return TerminalHostKind::VsCodeIntegratedTerminal;
    }

    if (hasEnvironmentVariable(L"ConEmuPID") || hasEnvironmentVariable(L"ConEmuANSI"))
    {
        return TerminalHostKind::ConEmu;
    }

    if (hasEnvironmentVariable(L"MSYSTEM") ||
        hasEnvironmentVariable(L"MINTTY_SHORTCUT") ||
        hasEnvironmentVariable(L"MSYS"))
    {
        return TerminalHostKind::Mintty;
    }

    const std::wstring term = getEnvironmentVariableString(L"TERM");
    if (containsIgnoreCase(term, L"xterm") ||
        containsIgnoreCase(term, L"ansi") ||
        containsIgnoreCase(term, L"cygwin"))
    {
        return TerminalHostKind::OtherTerminalHost;
    }

    return TerminalHostKind::ClassicConsoleWindow;
}

bool TerminalHostClassifier::shouldPreferTerminalRenderer(TerminalHostKind hostKind)
{
    switch (hostKind)
    {
    case TerminalHostKind::WindowsTerminal:
    case TerminalHostKind::VsCodeIntegratedTerminal:
    case TerminalHostKind::ConEmu:
    case TerminalHostKind::Mintty:
    case TerminalHostKind::OtherTerminalHost:
        return true;

    case TerminalHostKind::ClassicConsoleWindow:
    case TerminalHostKind::RedirectedOrPipe:
    case TerminalHostKind::Unknown:
    default:
        return false;
    }
}