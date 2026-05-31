#include "App/TerminalLauncher.h"

#include "Rendering/Backends/TerminalHostInfo.h"

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

namespace
{
    constexpr wchar_t kLaunchedByWtFlag[] = L"--launched-by-wt";

    bool equalsIgnoreCase(const wchar_t* left, const wchar_t* right)
    {
        if (left == nullptr || right == nullptr)
        {
            return false;
        }

        return _wcsicmp(left, right) == 0;
    }

    bool hasArgument(int argc, wchar_t** argv, const wchar_t* value)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (equalsIgnoreCase(argv[i], value))
            {
                return true;
            }
        }

        return false;
    }

    std::wstring getModulePath()
    {
        std::wstring result;
        result.resize(32768);

        const DWORD length = GetModuleFileNameW(nullptr, &result[0], static_cast<DWORD>(result.size()));
        if (length == 0)
        {
            return std::wstring();
        }

        result.resize(length);
        return result;
    }

    std::wstring getCurrentDirectoryString()
    {
        const DWORD required = GetCurrentDirectoryW(0, nullptr);
        if (required == 0)
        {
            return std::wstring();
        }

        std::wstring result;
        result.resize(required);

        const DWORD length = GetCurrentDirectoryW(required, &result[0]);
        if (length == 0)
        {
            return std::wstring();
        }

        if (!result.empty() && result.back() == L'\0')
        {
            result.pop_back();
        }

        return result;
    }

    std::wstring quoteWindowsArgument(const std::wstring& value)
    {
        if (value.empty())
        {
            return L"\"\"";
        }

        bool needsQuotes = false;
        for (wchar_t ch : value)
        {
            if (ch == L' ' || ch == L'\t' || ch == L'"')
            {
                needsQuotes = true;
                break;
            }
        }

        if (!needsQuotes)
        {
            return value;
        }

        std::wstring result;
        result.push_back(L'"');

        int backslashCount = 0;

        for (wchar_t ch : value)
        {
            if (ch == L'\\')
            {
                ++backslashCount;
                continue;
            }

            if (ch == L'"')
            {
                result.append(backslashCount * 2 + 1, L'\\');
                result.push_back(L'"');
                backslashCount = 0;
                continue;
            }

            if (backslashCount > 0)
            {
                result.append(backslashCount, L'\\');
                backslashCount = 0;
            }

            result.push_back(ch);
        }

        if (backslashCount > 0)
        {
            result.append(backslashCount * 2, L'\\');
        }

        result.push_back(L'"');
        return result;
    }

    std::wstring buildWtCommandLine(int argc, wchar_t** argv)
    {
        const std::wstring exePath = getModulePath();
        if (exePath.empty())
        {
            return std::wstring();
        }

        const std::wstring currentDirectory = getCurrentDirectoryString();

        std::wstring commandLine;

        commandLine += L"wt.exe";
        commandLine += L" -w -1";
        commandLine += L" -M";
        commandLine += L" -d ";
        commandLine += quoteWindowsArgument(currentDirectory);
        commandLine += L" ";
        commandLine += quoteWindowsArgument(exePath);
        commandLine += L" ";
        commandLine += kLaunchedByWtFlag;

        for (int i = 1; i < argc; ++i)
        {
            if (equalsIgnoreCase(argv[i], kLaunchedByWtFlag))
            {
                continue;
            }

            commandLine += L" ";
            commandLine += quoteWindowsArgument(argv[i]);
        }

        return commandLine;
    }

    TerminalHostKind toRequestedHost(StartupHostPreference preference)
    {
        switch (preference)
        {
        case StartupHostPreference::Console:
            return TerminalHostKind::ClassicConsoleWindow;

        case StartupHostPreference::Terminal:
            return TerminalHostKind::WindowsTerminal;

        case StartupHostPreference::Auto:
        default:
            return TerminalHostKind::WindowsTerminal;
        }
    }

    std::string narrowAscii(const std::wstring& value)
    {
        std::string result;
        result.reserve(value.size());

        for (wchar_t ch : value)
        {
            result.push_back(ch >= 0 && ch <= 0x7F ? static_cast<char>(ch) : '?');
        }

        return result;
    }

    void addTraceEntry(
        StartupDiagnosticsContext& diagnosticsContext,
        RendererSelectionStage stage,
        const std::string& decision,
        const std::string& detail)
    {
        diagnosticsContext.selectionTrace.addEntry(stage, decision, detail);
    }

    std::string describeCommandLinePreference(
        const CommandLineOptions& commandLineOptions,
        bool hasPreference,
        const char* valueText)
    {
        if (!hasPreference)
        {
            return "not provided";
        }

        return valueText;
    }
}

bool TerminalLauncher::tryRelaunchInWindowsTerminal()
{
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr)
    {
        return false;
    }

    const bool alreadyRelaunched = hasArgument(argc, argv, kLaunchedByWtFlag);
    const TerminalHostInfo hostInfo = TerminalHostInfoDetector::detectCurrentHostInfo();

    if (alreadyRelaunched || hostInfo.windowsTerminalSessionHint)
    {
        LocalFree(argv);
        return false;
    }

    const std::wstring commandLine = buildWtCommandLine(argc, argv);
    LocalFree(argv);

    if (commandLine.empty())
    {
        return false;
    }

    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo{};
    const BOOL created = CreateProcessW(
        nullptr,
        mutableCommandLine.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    if (!created)
    {
        return false;
    }

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

StartupLaunchDecision TerminalLauncher::prepareStartup(const StartupOptions& options)
{
    StartupLaunchDecision decision{};

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    const bool launchedByWtFlag =
        (argv != nullptr) &&
        hasArgument(argc, argv, kLaunchedByWtFlag);

    if (argv != nullptr)
    {
        LocalFree(argv);
    }

    const TerminalHostInfo hostInfo = TerminalHostInfoDetector::detectCurrentHostInfo();

    StartupDiagnosticsContext diagnosticsContext;
    diagnosticsContext.configuredHostPreference = options.configuredHostPreference;
    diagnosticsContext.configuredRendererPreference = options.configuredRendererPreference;
    diagnosticsContext.startupConfigFileFound = options.startupConfigFileFound;
    diagnosticsContext.startupConfigSource = "startup.config";
    diagnosticsContext.requestedHost = toRequestedHost(options.hostPreference);
    diagnosticsContext.actualHost = hostInfo.hostKind;
    diagnosticsContext.launchedByWindowsTerminalFlag = launchedByWtFlag;
    diagnosticsContext.windowsTerminalSessionHint = hostInfo.windowsTerminalSessionHint;

    addTraceEntry(
        diagnosticsContext,
        RendererSelectionStage::StartupConfiguration,
        "Captured startup sources",
        std::string("configFileFound=") + (options.startupConfigFileFound ? "true" : "false") +
        ", configHost=" + toString(options.configuredHostPreference) +
        ", configRenderer=" + toString(options.configuredRendererPreference) +
        ", configValidation=" + toString(options.configuredValidationScreenPreference));

    addTraceEntry(
        diagnosticsContext,
        RendererSelectionStage::StartupConfiguration,
        "Applied startup precedence",
        std::string("effectiveHost=") + toString(options.hostPreference) +
        " (" + toString(options.hostPreferenceSource) + ")" +
        ", effectiveRenderer=" + toString(options.rendererPreference) +
        " (" + toString(options.rendererPreferenceSource) + ")" +
        ", effectiveValidation=" + toString(options.validationScreenPreference) +
        " (" + toString(options.validationScreenPreferenceSource) + ")");

    if (!options.commandLineOptions.invalidArguments.empty())
    {
        std::string detail = "invalid=";
        for (std::size_t i = 0; i < options.commandLineOptions.invalidArguments.size(); ++i)
        {
            if (i > 0)
            {
                detail += ", ";
            }

            detail += narrowAscii(options.commandLineOptions.invalidArguments[i]);
        }

        addTraceEntry(
            diagnosticsContext,
            RendererSelectionStage::StartupConfiguration,
            "Ignored invalid command-line arguments",
            detail);
    }

    bool shouldAttemptTerminalHost = false;

    switch (options.hostPreference)
    {
    case StartupHostPreference::Console:
        shouldAttemptTerminalHost = false;
        diagnosticsContext.requestedHost = TerminalHostKind::ClassicConsoleWindow;
        break;

    case StartupHostPreference::Terminal:
        shouldAttemptTerminalHost = true;
        diagnosticsContext.requestedHost = TerminalHostKind::WindowsTerminal;
        break;

    case StartupHostPreference::Auto:
    default:
        shouldAttemptTerminalHost = true;
        diagnosticsContext.requestedHost = TerminalHostKind::WindowsTerminal;
        break;
    }

    addTraceEntry(
        diagnosticsContext,
        RendererSelectionStage::HostPreferenceEvaluation,
        "Inspected requested host vs detected host",
        std::string("requestedHost=") + toString(diagnosticsContext.requestedHost) +
        ", actualHost=" + toString(diagnosticsContext.actualHost) +
        ", launchedByWtFlag=" + (launchedByWtFlag ? "true" : "false") +
        ", wtSessionHint=" + (hostInfo.windowsTerminalSessionHint ? "true" : "false"));

    const bool alreadyInsideRequestedTerminalHost =
        launchedByWtFlag ||
        hostInfo.windowsTerminalSessionHint ||
        hostInfo.hostKind == TerminalHostKind::WindowsTerminal;

    if (shouldAttemptTerminalHost && !alreadyInsideRequestedTerminalHost)
    {
        diagnosticsContext.relaunchAttempted = true;

        addTraceEntry(
            diagnosticsContext,
            RendererSelectionStage::RelaunchDecision,
            "Attempting relaunch into Windows Terminal",
            std::string("requestedHost=") + toString(diagnosticsContext.requestedHost));

        if (tryRelaunchInWindowsTerminal())
        {
            diagnosticsContext.relaunchPerformed = true;
            decision.relaunchPerformed = true;

            addTraceEntry(
                diagnosticsContext,
                RendererSelectionStage::RelaunchDecision,
                "Relaunch into Windows Terminal succeeded",
                "Startup will exit so the relaunched process can continue.");

            addTraceEntry(
                diagnosticsContext,
                RendererSelectionStage::FinalSelection,
                "Startup selection finalized",
                std::string("host=") + toString(TerminalHostKind::WindowsTerminal) +
                ", renderer=" + toString(RendererKind::Unknown) +
                ", relaunchPerformed=true");

            decision.diagnosticsContext = diagnosticsContext;
            return decision;
        }

        addTraceEntry(
            diagnosticsContext,
            RendererSelectionStage::RelaunchDecision,
            "Relaunch attempt failed or was declined by the environment",
            "Startup continues in the current host because the Windows Terminal relaunch did not complete.");
    }
    else
    {
        addTraceEntry(
            diagnosticsContext,
            RendererSelectionStage::RelaunchDecision,
            "No relaunch was needed",
            std::string("shouldAttemptTerminalHost=") + (shouldAttemptTerminalHost ? "true" : "false") +
            ", alreadyInsideRequestedTerminalHost=" + (alreadyInsideRequestedTerminalHost ? "true" : "false"));
    }

    switch (options.rendererPreference)
    {
    case StartupRendererPreference::Console:
        decision.rendererSelection = StartupRendererSelection::Console;
        diagnosticsContext.requestedRenderer = RendererKind::ConsoleRenderer;
        diagnosticsContext.actualRenderer = RendererKind::ConsoleRenderer;

        addTraceEntry(
            diagnosticsContext,
            RendererSelectionStage::RendererPreferenceEvaluation,
            "Renderer preference resolved to Console",
            std::string("requestedRenderer=") + toString(diagnosticsContext.requestedRenderer) +
            ", actualRenderer=" + toString(diagnosticsContext.actualRenderer));
        break;

    case StartupRendererPreference::Terminal:
        decision.rendererSelection = StartupRendererSelection::Terminal;
        diagnosticsContext.requestedRenderer = RendererKind::TerminalRenderer;
        diagnosticsContext.actualRenderer = RendererKind::TerminalRenderer;

        addTraceEntry(
            diagnosticsContext,
            RendererSelectionStage::RendererPreferenceEvaluation,
            "Renderer preference resolved to Terminal",
            std::string("requestedRenderer=") + toString(diagnosticsContext.requestedRenderer) +
            ", actualRenderer=" + toString(diagnosticsContext.actualRenderer));
        break;

    case StartupRendererPreference::Auto:
    default:
        if (hostInfo.prefersTerminalRenderer)
        {
            decision.rendererSelection = StartupRendererSelection::Terminal;
            diagnosticsContext.requestedRenderer = RendererKind::TerminalRenderer;
            diagnosticsContext.actualRenderer = RendererKind::TerminalRenderer;

            addTraceEntry(
                diagnosticsContext,
                RendererSelectionStage::RendererPreferenceEvaluation,
                "Renderer preference resolved to Terminal",
                std::string("requestedRenderer=") + toString(diagnosticsContext.requestedRenderer) +
                ", actualRenderer=" + toString(diagnosticsContext.actualRenderer) +
                ", prefersTerminalRenderer=true");
        }
        else
        {
            decision.rendererSelection = StartupRendererSelection::Console;
            diagnosticsContext.requestedRenderer = RendererKind::ConsoleRenderer;
            diagnosticsContext.actualRenderer = RendererKind::ConsoleRenderer;

            addTraceEntry(
                diagnosticsContext,
                RendererSelectionStage::RendererPreferenceEvaluation,
                "Renderer preference resolved to Console",
                std::string("requestedRenderer=") + toString(diagnosticsContext.requestedRenderer) +
                ", actualRenderer=" + toString(diagnosticsContext.actualRenderer) +
                ", prefersTerminalRenderer=false");
        }

        break;
    }

    addTraceEntry(
        diagnosticsContext,
        RendererSelectionStage::FinalSelection,
        "Launcher startup selection finalized",
        std::string("host=") + toString(diagnosticsContext.actualHost) +
        ", requestedRenderer=" + toString(diagnosticsContext.requestedRenderer) +
        ", relaunchPerformed=" + (diagnosticsContext.relaunchPerformed ? "true" : "false"));

    decision.diagnosticsContext = diagnosticsContext;
    return decision;
}