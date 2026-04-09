#include "App/StartupConfig.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <string>

namespace
{
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

    StartupHostPreference parseHostPreference(const std::wstring& value)
    {
        const std::wstring normalized = toLower(trim(value));

        if (normalized == L"console")
        {
            return StartupHostPreference::Console;
        }

        if (normalized == L"terminal")
        {
            return StartupHostPreference::Terminal;
        }

        return StartupHostPreference::Auto;
    }

    StartupRendererPreference parseRendererPreference(const std::wstring& value)
    {
        const std::wstring normalized = toLower(trim(value));

        if (normalized == L"console")
        {
            return StartupRendererPreference::Console;
        }

        if (normalized == L"terminal")
        {
            return StartupRendererPreference::Terminal;
        }

        return StartupRendererPreference::Auto;
    }

    StartupValidationScreenPreference parseValidationPreference(const std::wstring& value)
    {
        const std::wstring normalized = toLower(trim(value));

        if (normalized == L"true")
        {
            return StartupValidationScreenPreference::ValidationStartTrue;
        }

        if (normalized == L"false")
        {
            return StartupValidationScreenPreference::ValidationStartFalse;
        }
    }

    std::wstring getConfigString(
        const std::wstring& section,
        const std::wstring& key,
        const std::wstring& defaultValue,
        const std::wstring& path)
    {
        wchar_t buffer[256]{};

        GetPrivateProfileStringW(
            section.c_str(),
            key.c_str(),
            defaultValue.c_str(),
            buffer,
            static_cast<DWORD>(std::size(buffer)),
            path.c_str());

        return std::wstring(buffer);
    }

    bool fileExists(const std::wstring& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
    }
}

std::wstring StartupConfigLoader::getStartupConfigPath()
{
    wchar_t modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    if (length == 0 || length >= MAX_PATH)
    {
        return L"startup.config";
    }

    std::wstring path(modulePath, length);

    const std::size_t slashPos = path.find_last_of(L"\\/");
    if (slashPos == std::wstring::npos)
    {
        return L"startup.config";
    }

    path.erase(slashPos + 1);
    path += L"startup.config";
    return path;
}

StartupConfig StartupConfigLoader::loadFromStartupConfig()
{
    StartupConfig config{};

    const std::wstring configPath = getStartupConfigPath();
    config.configFilePath = configPath;
    config.configFileFound = fileExists(configPath);

    const std::wstring hostValue =
        getConfigString(L"Startup", L"Host", L"Auto", configPath);

    const std::wstring rendererValue =
        getConfigString(L"Startup", L"Renderer", L"Auto", configPath);

    const std::wstring validationScreenValue =
        getConfigString(L"Startup", L"ValidationScreen", L"False", configPath);

    config.hostPreference = parseHostPreference(hostValue);
    config.rendererPreference = parseRendererPreference(rendererValue);
    config.validationScreenPreference = parseValidationPreference(validationScreenValue);

    return config;
}