#pragma once

#include <string>

enum class StartupHostPreference
{
    Auto = 0,
    Console,
    Terminal
};

enum class StartupRendererPreference
{
    Auto = 0,
    Console,
    Terminal
};

enum class StartupValidationScreenPreference
{
    ValidationStartFalse = 0,
    ValidationStartTrue
};

struct StartupConfig
{
    StartupHostPreference hostPreference = StartupHostPreference::Auto;
    StartupRendererPreference rendererPreference = StartupRendererPreference::Auto;
    StartupValidationScreenPreference validationScreenPreference = StartupValidationScreenPreference::ValidationStartFalse;

    bool configFileFound = false;
    std::wstring configFilePath;
};

class StartupConfigLoader
{
public:
    static StartupConfig loadFromStartupConfig();

private:
    static std::wstring getStartupConfigPath();
};

inline const char* toString(StartupValidationScreenPreference preference)
{
    switch (preference)
    {
    case StartupValidationScreenPreference::ValidationStartTrue:
        return "True";

    case StartupValidationScreenPreference::ValidationStartFalse:
    default:
        return "False";
    }
}