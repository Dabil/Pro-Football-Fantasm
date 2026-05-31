#pragma once

#include <cstdint>

namespace Cp437Encoding
{
    enum class ConversionKind
    {
        Exact,
        FallbackApproximation,
        Replacement
    };

    struct EncodeResult
    {
        bool success = false;
        unsigned char byte = 0;
        char32_t outputCodePoint = U'\0';
        ConversionKind kind = ConversionKind::Exact;
        const char* reason = nullptr;
    };

    char32_t decodeByte(unsigned char value);

    bool tryEncodeExact(char32_t codePoint, unsigned char& outByte);

    bool tryEncodeWithFallback(
        char32_t codePoint,
        char32_t replacementGlyph,
        EncodeResult& outResult);
}
