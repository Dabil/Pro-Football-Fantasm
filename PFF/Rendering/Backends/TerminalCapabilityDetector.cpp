#include "Rendering/Backends/TerminalCapabilityDetector.h"

#include "Rendering/Backends/RendererCapabilityDetector.h"

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

        const DWORD written = GetEnvironmentVariableW(name, &value[0], required);
        if (written == 0)
        {
            return std::wstring();
        }

        if (!value.empty() && value.back() == L'\0')
        {
            value.pop_back();
        }

        return value;
    }

    std::wstring toLower(std::wstring value)
    {
        for (wchar_t& ch : value)
        {
            ch = static_cast<wchar_t>(towlower(ch));
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

        const std::wstring loweredHaystack = toLower(haystack);
        const std::wstring loweredNeedle = toLower(std::wstring(needle));
        return loweredHaystack.find(loweredNeedle) != std::wstring::npos;
    }

    bool isWindowsTerminalEnvironment()
    {
        return hasEnvironmentVariable(L"WT_SESSION");
    }

    bool isVsCodeTerminalEnvironment()
    {
        const std::wstring termProgram = getEnvironmentVariableString(L"TERM_PROGRAM");
        return equalsIgnoreCase(termProgram, L"vscode");
    }

    bool isConEmuEnvironment()
    {
        return hasEnvironmentVariable(L"ConEmuPID") || hasEnvironmentVariable(L"ConEmuANSI");
    }

    bool isTrueColorEnvironment()
    {
        const std::wstring colorTerm = getEnvironmentVariableString(L"COLORTERM");

        if (equalsIgnoreCase(colorTerm, L"truecolor") ||
            equalsIgnoreCase(colorTerm, L"24bit"))
        {
            return true;
        }

        if (isWindowsTerminalEnvironment())
        {
            return true;
        }

        if (isVsCodeTerminalEnvironment())
        {
            return true;
        }

        return false;
    }

    bool isIndexed256Environment()
    {
        const std::wstring term = getEnvironmentVariableString(L"TERM");
        if (containsIgnoreCase(term, L"256color"))
        {
            return true;
        }

        if (isConEmuEnvironment())
        {
            return true;
        }

        return false;
    }

    bool isXtermLikeEnvironment()
    {
        const std::wstring term = getEnvironmentVariableString(L"TERM");

        return containsIgnoreCase(term, L"xterm") ||
            containsIgnoreCase(term, L"screen") ||
            containsIgnoreCase(term, L"tmux") ||
            containsIgnoreCase(term, L"rxvt") ||
            containsIgnoreCase(term, L"vt100") ||
            containsIgnoreCase(term, L"vt220");
    }

    ColorSupport detectTerminalColorTier()
    {
        if (isTrueColorEnvironment())
        {
            return ColorSupport::Rgb24;
        }

        if (isIndexed256Environment())
        {
            return ColorSupport::Indexed256;
        }

        return ColorSupport::Basic16;
    }

    RendererFeatureSupport detectNativeStrikeSupport()
    {
        if (isWindowsTerminalEnvironment() || isVsCodeTerminalEnvironment())
        {
            return RendererFeatureSupport::Supported;
        }

        if (isXtermLikeEnvironment())
        {
            return RendererFeatureSupport::Supported;
        }

        return RendererFeatureSupport::Unsupported;
    }

    RendererFeatureSupport detectBlinkSupport()
    {
        if (isXtermLikeEnvironment())
        {
            return RendererFeatureSupport::Supported;
        }

        return RendererFeatureSupport::Emulated;
    }

    RendererCapabilities buildTerminalCapabilities()
    {
        RendererCapabilities capabilities = RendererCapabilities::VirtualTerminal();
        capabilities.colorTier = detectTerminalColorTier();

        capabilities.bold = RendererFeatureSupport::Supported;
        capabilities.dim = RendererFeatureSupport::Supported;
        capabilities.underline = RendererFeatureSupport::Supported;
        capabilities.reverse = RendererFeatureSupport::Supported;
        capabilities.invisible = RendererFeatureSupport::Supported;
        capabilities.strike = detectNativeStrikeSupport();
        capabilities.slowBlink = detectBlinkSupport();
        capabilities.fastBlink = detectBlinkSupport();

        return capabilities;
    }
}

TerminalCapabilityDetectionResult TerminalCapabilityDetector::detectAndConfigure(
    HANDLE hOut,
    HANDLE hIn,
    DWORD originalOutputMode,
    DWORD originalInputMode,
    bool haveOriginalOutputMode,
    bool haveOriginalInputMode)
{
    TerminalCapabilityDetectionResult result;

    const RendererCapabilityProbeResult probe =
        RendererCapabilityDetector::detectAndConfigure(
            hOut,
            hIn,
            originalOutputMode,
            originalInputMode,
            haveOriginalOutputMode,
            haveOriginalInputMode,
            true,
            true);

    result.virtualTerminalWasEnabled = probe.virtualTerminalWasEnabled;
    result.virtualTerminalEnableAttempted = probe.virtualTerminalEnableAttempted;
    result.virtualTerminalEnableSucceeded = probe.virtualTerminalEnableSucceeded;
    result.configuredOutputMode = probe.configuredOutputMode;
    result.configuredInputMode = probe.configuredInputMode;
    result.hasConfiguredOutputMode = probe.hasConfiguredOutputMode;
    result.hasConfiguredInputMode = probe.hasConfiguredInputMode;

    if (!probe.virtualTerminalWasEnabled)
    {
        return result;
    }

    result.capabilities = buildTerminalCapabilities();
    result.terminalOutputReady = true;
    return result;
}
