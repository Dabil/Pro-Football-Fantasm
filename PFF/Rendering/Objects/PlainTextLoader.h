#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace PlainTextLoader
{
    enum class FileType
    {
        Auto,
        Unknown,
        Asc,
        Txt,
        Nfo,
        Diz
    };

    enum class Encoding
    {
        Auto,
        Utf8,
        Ascii,
        Latin1,
        Cp437
    };

    struct LoadOptions
    {
        FileType fileType = FileType::Auto;
        Encoding encoding = Encoding::Auto;

        bool normalizeLineEndings = true;
        bool expandTabs = false;
        int tabWidth = 4;

        bool preserveTrailingSpaces = true;

        std::optional<Style> style;
    };

    struct LoadResult
    {
        TextObject object;

        bool success = false;

        FileType detectedFileType = FileType::Unknown;
        Encoding resolvedEncoding = Encoding::Utf8;

        bool hadUtf8Bom = false;
        bool normalizedLineEndings = false;
    };

    FileType detectFileType(const std::string& filePath);

    LoadResult loadFromFile(const std::string& filePath);
    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options);
    LoadResult loadFromFile(const std::string& filePath, const Style& style);

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject);
    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const LoadOptions& options);
    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const Style& style);

    TextObject loadFromBytes(std::string_view bytes, const LoadOptions& options = {});
    std::u32string decodeToU32(std::string_view bytes, const LoadOptions& options = {});

    const char* toString(FileType fileType);
    const char* toString(Encoding encoding);
}