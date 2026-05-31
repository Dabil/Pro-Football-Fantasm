#pragma once

#include "App/CommandLineOptions.h"
#include "App/StartupConfig.h"

enum class StartupOptionSource
{
    Default = 0,
    StartupConfig,
    CommandLine
};

struct StartupOptions
{
    StartupHostPreference configuredHostPreference = StartupHostPreference::Auto;
    StartupRendererPreference configuredRendererPreference = StartupRendererPreference::Auto;
    StartupValidationScreenPreference configuredValidationScreenPreference =
        StartupValidationScreenPreference::ValidationStartFalse;

    StartupHostPreference hostPreference = StartupHostPreference::Auto;
    StartupRendererPreference rendererPreference = StartupRendererPreference::Auto;
    StartupValidationScreenPreference validationScreenPreference =
        StartupValidationScreenPreference::ValidationStartFalse;

    StartupOptionSource hostPreferenceSource = StartupOptionSource::Default;
    StartupOptionSource rendererPreferenceSource = StartupOptionSource::Default;
    StartupOptionSource validationScreenPreferenceSource = StartupOptionSource::Default;

    bool startupConfigFileFound = false;
    std::wstring startupConfigFilePath;

    CommandLineOptions commandLineOptions{};
};

class StartupOptionsResolver
{
public:
    static StartupOptions resolve(
        const StartupConfig& startupConfig,
        const CommandLineOptions& commandLineOptions);
};

inline const char* toString(StartupOptionSource source)
{
    switch (source)
    {
    case StartupOptionSource::StartupConfig:
        return "StartupConfig";

    case StartupOptionSource::CommandLine:
        return "CommandLine";

    case StartupOptionSource::Default:
    default:
        return "Default";
    }
}