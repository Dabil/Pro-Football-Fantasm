#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "App/StartupConfig.h"

struct CommandLineOptions
{
    bool showHelp = false;

    bool hasHostPreference = false;
    StartupHostPreference hostPreference = StartupHostPreference::Auto;

    bool hasRendererPreference = false;
    StartupRendererPreference rendererPreference = StartupRendererPreference::Auto;

    bool hasValidationScreenPreference = false;
    StartupValidationScreenPreference validationScreenPreference =
        StartupValidationScreenPreference::ValidationStartFalse;

    std::vector<std::wstring> invalidArguments;
    std::vector<std::wstring> unrecognizedArguments;
};

class CommandLineOptionsParser
{
public:
    static CommandLineOptions parseFromProcessCommandLine();
    static void writeHelpText(std::wostream& stream);

private:
    static CommandLineOptions parse(int argc, wchar_t** argv);
};