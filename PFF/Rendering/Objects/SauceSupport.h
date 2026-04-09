#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SauceSupport
{
    struct SauceMetadata
    {
        bool present = false;

        std::string title;
        std::string author;
        std::string group;
        std::string date;

        std::uint8_t dataType = 0;
        std::uint8_t fileType = 0;

        std::uint16_t tInfo1 = 0; // width
        std::uint16_t tInfo2 = 0; // height

        std::uint8_t commentLineCount = 0;
        std::uint8_t flags = 0;

        std::string fontName;

        std::vector<std::string> comments;
    };

    struct SauceParseResult
    {
        SauceMetadata metadata;

        bool found = false;
        bool truncated = false;
        bool invalidCommentBlock = false;

        std::size_t contentSize = 0;
    };

    SauceParseResult parse(std::string_view bytes);

    std::string trimRightAscii(const std::string& value);
}