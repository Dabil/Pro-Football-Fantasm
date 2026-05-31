#include "Utilities/Unicode/UnicodeConversion.h"

#include <cstddef>

/*
    Purpose:

    Own all encoding conversion boundaries

    Checklist:
        - Add UTF-8 -> UTF-32 conversion implementation
        - Add UTF-32 -> UTF-8 conversion implementation
        - Add code point -> UTF-16 conversion implementation
        - Add UTF-32 -> UTF-16 conversion implementation
        - Add replacement/sanitization policy implementation here, not in renderer
*/

namespace
{
    constexpr char32_t kReplacementCodePoint = 0xFFFD;

    bool isContinuationByte(unsigned char byte)
    {
        return (byte & 0xC0) == 0x80;
    }

    bool isSurrogate(char32_t codePoint)
    {
        return codePoint >= 0xD800 && codePoint <= 0xDFFF;
    }

    bool isNonCharacter(char32_t codePoint)
    {
        if (codePoint >= 0xFDD0 && codePoint <= 0xFDEF)
        {
            return true;
        }

        return (codePoint & 0xFFFE) == 0xFFFE && codePoint <= 0x10FFFF;
    }

    bool isControlCode(char32_t codePoint)
    {
        return (codePoint <= 0x001F) || (codePoint >= 0x007F && codePoint <= 0x009F);
    }
}

namespace UnicodeConversion
{
    char32_t sanitizeCodePoint(char32_t codePoint)
    {
        if (codePoint > 0x10FFFF)
        {
            return kReplacementCodePoint;
        }

        if (isSurrogate(codePoint))
        {
            return kReplacementCodePoint;
        }

        if (isNonCharacter(codePoint))
        {
            return kReplacementCodePoint;
        }

        if (isControlCode(codePoint))
        {
            switch (codePoint)
            {
            case U'\t':
            case U'\n':
            case U'\r':
                return codePoint;
            default:
                return kReplacementCodePoint;
            }
        }

        return codePoint;
    }

    std::u32string utf8ToU32(std::string_view utf8)
    {
        std::u32string result;
        result.reserve(utf8.size());

        std::size_t i = 0;
        while (i < utf8.size())
        {
            const unsigned char b0 = static_cast<unsigned char>(utf8[i]);

            if (b0 <= 0x7F)
            {
                result.push_back(sanitizeCodePoint(static_cast<char32_t>(b0)));
                ++i;
                continue;
            }

            if ((b0 & 0xE0) == 0xC0)
            {
                if (i + 1 >= utf8.size())
                {
                    result.push_back(kReplacementCodePoint);
                    ++i;
                    continue;
                }

                const unsigned char b1 = static_cast<unsigned char>(utf8[i + 1]);
                if (!isContinuationByte(b1))
                {
                    result.push_back(kReplacementCodePoint);
                    ++i;
                    continue;
                }

                const char32_t codePoint =
                    (static_cast<char32_t>(b0 & 0x1F) << 6) |
                    static_cast<char32_t>(b1 & 0x3F);

                if (codePoint < 0x80)
                {
                    result.push_back(kReplacementCodePoint);
                    i += 2;
                    continue;
                }

                result.push_back(sanitizeCodePoint(codePoint));
                i += 2;
                continue;
            }

            if ((b0 & 0xF0) == 0xE0)
            {
                if (i + 2 >= utf8.size())
                {
                    result.push_back(kReplacementCodePoint);
                    ++i;
                    continue;
                }

                const unsigned char b1 = static_cast<unsigned char>(utf8[i + 1]);
                const unsigned char b2 = static_cast<unsigned char>(utf8[i + 2]);

                if (!isContinuationByte(b1) || !isContinuationByte(b2))
                {
                    result.push_back(kReplacementCodePoint);
                    ++i;
                    continue;
                }

                const char32_t codePoint =
                    (static_cast<char32_t>(b0 & 0x0F) << 12) |
                    (static_cast<char32_t>(b1 & 0x3F) << 6) |
                    static_cast<char32_t>(b2 & 0x3F);

                if (codePoint < 0x800)
                {
                    result.push_back(kReplacementCodePoint);
                    i += 3;
                    continue;
                }

                result.push_back(sanitizeCodePoint(codePoint));
                i += 3;
                continue;
            }

            if ((b0 & 0xF8) == 0xF0)
            {
                if (i + 3 >= utf8.size())
                {
                    result.push_back(kReplacementCodePoint);
                    ++i;
                    continue;
                }

                const unsigned char b1 = static_cast<unsigned char>(utf8[i + 1]);
                const unsigned char b2 = static_cast<unsigned char>(utf8[i + 2]);
                const unsigned char b3 = static_cast<unsigned char>(utf8[i + 3]);

                if (!isContinuationByte(b1) ||
                    !isContinuationByte(b2) ||
                    !isContinuationByte(b3))
                {
                    result.push_back(kReplacementCodePoint);
                    ++i;
                    continue;
                }

                const char32_t codePoint =
                    (static_cast<char32_t>(b0 & 0x07) << 18) |
                    (static_cast<char32_t>(b1 & 0x3F) << 12) |
                    (static_cast<char32_t>(b2 & 0x3F) << 6) |
                    static_cast<char32_t>(b3 & 0x3F);

                if (codePoint < 0x10000)
                {
                    result.push_back(kReplacementCodePoint);
                    i += 4;
                    continue;
                }

                result.push_back(sanitizeCodePoint(codePoint));
                i += 4;
                continue;
            }

            result.push_back(kReplacementCodePoint);
            ++i;
        }

        return result;
    }

    std::string u32ToUtf8(std::u32string_view text)
    {
        std::string result;
        result.reserve(text.size());

        for (char32_t codePoint : text)
        {
            codePoint = sanitizeCodePoint(codePoint);

            if (codePoint <= 0x7F)
            {
                result.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7FF)
            {
                result.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else if (codePoint <= 0xFFFF)
            {
                result.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
            else
            {
                result.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
                result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
            }
        }

        return result;
    }

    std::wstring codePointToUtf16(char32_t codePoint)
    {
        codePoint = sanitizeCodePoint(codePoint);

        if (codePoint <= 0xFFFF)
        {
            return std::wstring(1, static_cast<wchar_t>(codePoint));
        }

        codePoint -= 0x10000;

        const wchar_t highSurrogate =
            static_cast<wchar_t>(0xD800 + ((codePoint >> 10) & 0x3FF));
        const wchar_t lowSurrogate =
            static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF));

        std::wstring result;
        result.push_back(highSurrogate);
        result.push_back(lowSurrogate);
        return result;
    }

    std::wstring u32ToUtf16(std::u32string_view text)
    {
        std::wstring result;
        result.reserve(text.size());

        for (char32_t codePoint : text)
        {
            const std::wstring utf16 = codePointToUtf16(codePoint);
            result.append(utf16);
        }

        return result;
    }

    void appendCodePointUtf16(std::wstring& out, char32_t cp)
    {
        if (cp <= 0xFFFF)
        {
            // Basic Multilingual Plane
            out.push_back(static_cast<wchar_t>(cp));
        }
        else
        {
            // Surrogate pair
            cp -= 0x10000;

            wchar_t high = static_cast<wchar_t>((cp >> 10) + 0xD800);
            wchar_t low = static_cast<wchar_t>((cp & 0x3FF) + 0xDC00);

            out.push_back(high);
            out.push_back(low);
        }
    }
}