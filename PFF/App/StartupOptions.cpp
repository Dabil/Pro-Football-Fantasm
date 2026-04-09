// App/StartupOptions.cpp
#include "App/StartupOptions.h"

StartupOptions StartupOptionsResolver::resolve(
    const StartupConfig& startupConfig,
    const CommandLineOptions& commandLineOptions)
{
    StartupOptions options{};

    options.configuredHostPreference = startupConfig.hostPreference;
    options.configuredRendererPreference = startupConfig.rendererPreference;
    options.configuredValidationScreenPreference = startupConfig.validationScreenPreference;

    options.hostPreference = StartupHostPreference::Auto;
    options.rendererPreference = StartupRendererPreference::Auto;
    options.validationScreenPreference = StartupValidationScreenPreference::ValidationStartFalse;

    options.hostPreferenceSource = StartupOptionSource::Default;
    options.rendererPreferenceSource = StartupOptionSource::Default;
    options.validationScreenPreferenceSource = StartupOptionSource::Default;

    options.startupConfigFileFound = startupConfig.configFileFound;
    options.startupConfigFilePath = startupConfig.configFilePath;
    options.commandLineOptions = commandLineOptions;

    if (startupConfig.configFileFound)
    {
        options.hostPreference = startupConfig.hostPreference;
        options.rendererPreference = startupConfig.rendererPreference;
        options.validationScreenPreference = startupConfig.validationScreenPreference;

        options.hostPreferenceSource = StartupOptionSource::StartupConfig;
        options.rendererPreferenceSource = StartupOptionSource::StartupConfig;
        options.validationScreenPreferenceSource = StartupOptionSource::StartupConfig;
    }

    if (commandLineOptions.hasHostPreference)
    {
        options.hostPreference = commandLineOptions.hostPreference;
        options.hostPreferenceSource = StartupOptionSource::CommandLine;
    }

    if (commandLineOptions.hasRendererPreference)
    {
        options.rendererPreference = commandLineOptions.rendererPreference;
        options.rendererPreferenceSource = StartupOptionSource::CommandLine;
    }

    if (commandLineOptions.hasValidationScreenPreference)
    {
        options.validationScreenPreference = commandLineOptions.validationScreenPreference;
        options.validationScreenPreferenceSource = StartupOptionSource::CommandLine;
    }

    return options;
}