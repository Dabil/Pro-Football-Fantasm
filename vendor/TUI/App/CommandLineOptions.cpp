#include "App/CommandLineOptions.h"

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cwctype>
#include <ostream>
#include <string>

namespace
{
    constexpr wchar_t kLaunchedByWtFlag[] = L"--launched-by-wt";

    std::wstring trim(const std::wstring& value)
    {
        std::size_t start = 0;
        while (start < value.size() && iswspace(value[start]))
        {
            ++start;
        }

        std::size_t end = value.size();
        while (end > start && iswspace(value[end - 1]))
        {
            --end;
        }

        return value.substr(start, end - start);
    }

    std::wstring toLower(const std::wstring& value)
    {
        std::wstring result = value;
        std::transform(
            result.begin(),
            result.end(),
            result.begin(),
            [](wchar_t ch)
            {
                return static_cast<wchar_t>(towlower(ch));
            });

        return result;
    }

    bool startsWithIgnoreCase(const std::wstring& value, const std::wstring& prefix)
    {
        if (value.size() < prefix.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < prefix.size(); ++i)
        {
            if (towlower(value[i]) != towlower(prefix[i]))
            {
                return false;
            }
        }

        return true;
    }

    bool equalsIgnoreCase(const std::wstring& left, const std::wstring& right)
    {
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
    }

    bool isHelpArgument(const std::wstring& argument)
    {
        return equalsIgnoreCase(argument, L"--help") ||
            equalsIgnoreCase(argument, L"-h") ||
            equalsIgnoreCase(argument, L"/?");
    }

    bool tryParseHostPreference(
        const std::wstring& rawValue,
        StartupHostPreference& outPreference)
    {
        const std::wstring value = toLower(trim(rawValue));

        if (value == L"console")
        {
            outPreference = StartupHostPreference::Console;
            return true;
        }

        if (value == L"terminal")
        {
            outPreference = StartupHostPreference::Terminal;
            return true;
        }

        return false;
    }

    bool tryParseRendererPreference(
        const std::wstring& rawValue,
        StartupRendererPreference& outPreference)
    {
        const std::wstring value = toLower(trim(rawValue));

        if (value == L"console")
        {
            outPreference = StartupRendererPreference::Console;
            return true;
        }

        if (value == L"terminal")
        {
            outPreference = StartupRendererPreference::Terminal;
            return true;
        }

        return false;
    }

    bool tryParseValidationScreenPreference(
        const std::wstring& rawValue,
        StartupValidationScreenPreference& outPreference)
    {
        const std::wstring value = toLower(trim(rawValue));

        if (value == L"true" || value == L"1" || value == L"yes" || value == L"on")
        {
            outPreference = StartupValidationScreenPreference::ValidationStartTrue;
            return true;
        }

        if (value == L"false" || value == L"0" || value == L"no" || value == L"off")
        {
            outPreference = StartupValidationScreenPreference::ValidationStartFalse;
            return true;
        }

        return false;
    }

    std::wstring extractValueAfterEquals(const std::wstring& argument)
    {
        const std::size_t equalsPos = argument.find(L'=');
        if (equalsPos == std::wstring::npos || equalsPos + 1 >= argument.size())
        {
            return std::wstring();
        }

        return argument.substr(equalsPos + 1);
    }
}

CommandLineOptions CommandLineOptionsParser::parseFromProcessCommandLine()
{
    CommandLineOptions options{};

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv == nullptr)
    {
        return options;
    }

    options = parse(argc, argv);
    LocalFree(argv);

    return options;
}

void CommandLineOptionsParser::writeHelpText(std::wostream& stream)
{
    stream
        << L"TUI startup options\n"
        << L"\n"
        << L"Usage:\n"
        << L"  TUI.exe [options]\n"
        << L"\n"
        << L"Help:\n"
        << L"  --help\n"
        << L"  -h\n"
        << L"  /?\n"
        << L"      Show this help text and exit.\n"
        << L"\n"
        << L"Options:\n"
        << L"  --host=console|terminal\n"
        << L"      Request the startup host.\n"
        << L"\n"
        << L"  --renderer=console|terminal\n"
        << L"      Request the renderer.\n"
        << L"\n"
        << L"  --validation-screen\n"
        << L"      Start with ValidationScreen enabled.\n"
        << L"\n"
        << L"  --validation-screen=true|false\n"
        << L"  --validation=true|false\n"
        << L"      Explicitly enable or disable ValidationScreen startup.\n"
        << L"\n"
        << L"Precedence:\n"
        << L"  1. Help/early-exit arguments\n"
        << L"  2. Built-in defaults\n"
        << L"  3. startup.config\n"
        << L"  4. Command-line options\n"
        << L"\n"
        << L"Examples:\n"
        << L"  TUI.exe --help\n"
        << L"  TUI.exe -h\n"
        << L"  TUI.exe /?\n"
        << L"  TUI.exe --renderer=terminal\n"
        << L"  TUI.exe --host=terminal --renderer=console\n"
        << L"  TUI.exe --validation-screen\n"
        << L"  TUI.exe --host=console --renderer=console --validation=false\n";
}

CommandLineOptions CommandLineOptionsParser::parse(int argc, wchar_t** argv)
{
    CommandLineOptions options{};

    for (int i = 1; i < argc; ++i)
    {
        const std::wstring argument = argv[i];
        const std::wstring lowered = toLower(argument);

        if (equalsIgnoreCase(argument, kLaunchedByWtFlag))
        {
            continue;
        }

        if (isHelpArgument(argument))
        {
            options.showHelp = true;
            continue;
        }

        if (startsWithIgnoreCase(lowered, L"--host="))
        {
            StartupHostPreference parsedPreference = StartupHostPreference::Auto;
            if (tryParseHostPreference(extractValueAfterEquals(argument), parsedPreference))
            {
                options.hasHostPreference = true;
                options.hostPreference = parsedPreference;
            }
            else
            {
                options.invalidArguments.push_back(argument);
            }

            continue;
        }

        if (startsWithIgnoreCase(lowered, L"--renderer="))
        {
            StartupRendererPreference parsedPreference = StartupRendererPreference::Auto;
            if (tryParseRendererPreference(extractValueAfterEquals(argument), parsedPreference))
            {
                options.hasRendererPreference = true;
                options.rendererPreference = parsedPreference;
            }
            else
            {
                options.invalidArguments.push_back(argument);
            }

            continue;
        }

        if (equalsIgnoreCase(argument, L"--validation-screen"))
        {
            options.hasValidationScreenPreference = true;
            options.validationScreenPreference =
                StartupValidationScreenPreference::ValidationStartTrue;
            continue;
        }

        if (startsWithIgnoreCase(lowered, L"--validation-screen=") ||
            startsWithIgnoreCase(lowered, L"--validation="))
        {
            StartupValidationScreenPreference parsedPreference =
                StartupValidationScreenPreference::ValidationStartFalse;

            if (tryParseValidationScreenPreference(extractValueAfterEquals(argument), parsedPreference))
            {
                options.hasValidationScreenPreference = true;
                options.validationScreenPreference = parsedPreference;
            }
            else
            {
                options.invalidArguments.push_back(argument);
            }

            continue;
        }

        if (startsWithIgnoreCase(lowered, L"--"))
        {
            options.unrecognizedArguments.push_back(argument);
            continue;
        }

        if (!argument.empty() && argument[0] == L'/')
        {
            options.unrecognizedArguments.push_back(argument);
        }
    }

    return options;
}