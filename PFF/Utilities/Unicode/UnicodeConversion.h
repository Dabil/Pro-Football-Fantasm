#pragma once

#include <string>
#include <string_view>

/*
    Purpose:

    Own all encoding conversion boundaries
*/

namespace UnicodeConversion
{
    std::u32string utf8ToU32(std::string_view utf8);
    std::string u32ToUtf8(std::u32string_view text);

    std::wstring codePointToUtf16(char32_t codePoint);
    std::wstring u32ToUtf16(std::u32string_view text);

    char32_t sanitizeCodePoint(char32_t codePoint);
}