#include "Rendering/Objects/Cp437Encoding.h"

#include <array>

#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    const std::array<char32_t, 128>& cp437Table()
    {
        static const std::array<char32_t, 128> kCp437Table =
        {
            U'\u00C7', U'\u00FC', U'\u00E9', U'\u00E2', U'\u00E4', U'\u00E0', U'\u00E5', U'\u00E7',
            U'\u00EA', U'\u00EB', U'\u00E8', U'\u00EF', U'\u00EE', U'\u00EC', U'\u00C4', U'\u00C5',
            U'\u00C9', U'\u00E6', U'\u00C6', U'\u00F4', U'\u00F6', U'\u00F2', U'\u00FB', U'\u00F9',
            U'\u00FF', U'\u00D6', U'\u00DC', U'\u00A2', U'\u00A3', U'\u00A5', U'\u20A7', U'\u0192',
            U'\u00E1', U'\u00ED', U'\u00F3', U'\u00FA', U'\u00F1', U'\u00D1', U'\u00AA', U'\u00BA',
            U'\u00BF', U'\u2310', U'\u00AC', U'\u00BD', U'\u00BC', U'\u00A1', U'\u00AB', U'\u00BB',
            U'\u2591', U'\u2592', U'\u2593', U'\u2502', U'\u2524', U'\u2561', U'\u2562', U'\u2556',
            U'\u2555', U'\u2563', U'\u2551', U'\u2557', U'\u255D', U'\u255C', U'\u255B', U'\u2510',
            U'\u2514', U'\u2534', U'\u252C', U'\u251C', U'\u2500', U'\u253C', U'\u255E', U'\u255F',
            U'\u255A', U'\u2554', U'\u2569', U'\u2566', U'\u2560', U'\u2550', U'\u256C', U'\u2567',
            U'\u2568', U'\u2564', U'\u2565', U'\u2559', U'\u2558', U'\u2552', U'\u2553', U'\u256B',
            U'\u256A', U'\u2518', U'\u250C', U'\u2588', U'\u2584', U'\u258C', U'\u2590', U'\u2580',
            U'\u03B1', U'\u00DF', U'\u0393', U'\u03C0', U'\u03A3', U'\u03C3', U'\u00B5', U'\u03C4',
            U'\u03A6', U'\u0398', U'\u03A9', U'\u03B4', U'\u221E', U'\u03C6', U'\u03B5', U'\u2229',
            U'\u2261', U'\u00B1', U'\u2265', U'\u2264', U'\u2320', U'\u2321', U'\u00F7', U'\u2248',
            U'\u00B0', U'\u2219', U'\u00B7', U'\u221A', U'\u207F', U'\u00B2', U'\u25A0', U'\u00A0'
        };

        return kCp437Table;
    }

    bool tryResolveFallbackApproximation(
        char32_t codePoint,
        char32_t& outApproximation,
        const char*& outReason)
    {
        codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);

        switch (codePoint)
        {
        case U'\u2018':
        case U'\u2019':
        case U'\u201A':
        case U'\u201B':
        case U'\u2032':
            outApproximation = U'\'';
            outReason = "curved apostrophe approximated to ASCII apostrophe";
            return true;

        case U'\u201C':
        case U'\u201D':
        case U'\u201E':
        case U'\u201F':
        case U'\u2033':
            outApproximation = U'\"';
            outReason = "curved quote approximated to ASCII quote";
            return true;

        case U'\u2010':
        case U'\u2011':
        case U'\u2012':
        case U'\u2013':
        case U'\u2014':
        case U'\u2015':
        case U'\u2212':
            outApproximation = U'-';
            outReason = "dash-like glyph approximated to ASCII hyphen-minus";
            return true;

        case U'\u2022':
        case U'\u2043':
            outApproximation = U'*';
            outReason = "bullet approximated to ASCII asterisk";
            return true;

        case U'\u2026':
            outApproximation = U'.';
            outReason = "ellipsis approximated to period";
            return true;

        case U'\u00A0':
        case U'\u2000':
        case U'\u2001':
        case U'\u2002':
        case U'\u2003':
        case U'\u2004':
        case U'\u2005':
        case U'\u2006':
        case U'\u2007':
        case U'\u2008':
        case U'\u2009':
        case U'\u200A':
        case U'\u202F':
        case U'\u205F':
        case U'\u3000':
            outApproximation = U' ';
            outReason = "Unicode spacing glyph approximated to ASCII space";
            return true;

        case U'\u2190':
            outApproximation = U'<';
            outReason = "left arrow approximated to ASCII less-than";
            return true;

        case U'\u2191':
            outApproximation = U'^';
            outReason = "up arrow approximated to ASCII caret";
            return true;

        case U'\u2192':
            outApproximation = U'>';
            outReason = "right arrow approximated to ASCII greater-than";
            return true;

        case U'\u2193':
            outApproximation = U'v';
            outReason = "down arrow approximated to ASCII v";
            return true;

        case U'\u2713':
        case U'\u2714':
            outApproximation = U'v';
            outReason = "check mark approximated to ASCII v";
            return true;

        case U'\u2715':
        case U'\u2716':
        case U'\u00D7':
            outApproximation = U'x';
            outReason = "multiplication/check-cross glyph approximated to ASCII x";
            return true;

        default:
            break;
        }

        return false;
    }
}

namespace Cp437Encoding
{
    char32_t decodeByte(unsigned char value)
    {
        if (value <= 0x7F)
        {
            return UnicodeConversion::sanitizeCodePoint(static_cast<char32_t>(value));
        }

        return UnicodeConversion::sanitizeCodePoint(cp437Table()[value - 0x80]);
    }

    bool tryEncodeExact(char32_t codePoint, unsigned char& outByte)
    {
        codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);

        if (codePoint <= 0x7F)
        {
            outByte = static_cast<unsigned char>(codePoint);
            return true;
        }

        const auto& table = cp437Table();
        for (std::size_t index = 0; index < table.size(); ++index)
        {
            if (table[index] == codePoint)
            {
                outByte = static_cast<unsigned char>(0x80u + index);
                return true;
            }
        }

        return false;
    }

    bool tryEncodeWithFallback(
        char32_t codePoint,
        char32_t replacementGlyph,
        EncodeResult& outResult)
    {
        outResult = EncodeResult{};

        unsigned char exactByte = 0;
        if (tryEncodeExact(codePoint, exactByte))
        {
            outResult.success = true;
            outResult.byte = exactByte;
            outResult.outputCodePoint = decodeByte(exactByte);
            outResult.kind = ConversionKind::Exact;
            outResult.reason = "exact CP437 mapping";
            return true;
        }

        char32_t approximation = U'\0';
        const char* approximationReason = nullptr;
        if (tryResolveFallbackApproximation(codePoint, approximation, approximationReason))
        {
            unsigned char approximationByte = 0;
            if (tryEncodeExact(approximation, approximationByte))
            {
                outResult.success = true;
                outResult.byte = approximationByte;
                outResult.outputCodePoint = decodeByte(approximationByte);
                outResult.kind = ConversionKind::FallbackApproximation;
                outResult.reason = approximationReason;
                return true;
            }
        }

        unsigned char replacementByte = 0;
        if (!tryEncodeExact(replacementGlyph, replacementByte))
        {
            return false;
        }

        outResult.success = true;
        outResult.byte = replacementByte;
        outResult.outputCodePoint = decodeByte(replacementByte);
        outResult.kind = ConversionKind::Replacement;
        outResult.reason = "glyph not representable in CP437; replacement glyph used";
        return true;
    }
}
