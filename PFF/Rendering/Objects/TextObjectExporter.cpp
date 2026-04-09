#include "Rendering/Objects/TextObjectExporter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Capabilities/RendererCapabilities.h"
#include "Rendering/SgrEmitter.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/ColorMapping.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/ThemeColor.h"
#include "Rendering/Objects/XpArtExporter.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    struct ExportCodePoint
    {
        char32_t value = U'\0';
        TextObjectExporter::SourcePosition source;
    };

    struct TerminalArtStyleInfo
    {
        Style emittedStyle;
        unsigned char dosAttribute = 0x07;

        bool approximatedColor = false;
        bool approximatedThemeColor = false;
        bool droppedUnsupportedStyle = false;
        bool approximatedReverse = false;
        bool approximatedBold = false;
        bool usedIceColors = false;
    };

    std::string toLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        return value;
    }

    std::string getFileExtensionLower(const std::string& filePath)
    {
        const std::size_t dot = filePath.find_last_of('.');
        if (dot == std::string::npos)
        {
            return {};
        }

        return toLowerCopy(filePath.substr(dot));
    }

    TextObjectExporter::FileType resolveFileType(
        const TextObjectExporter::SaveOptions& options)
    {
        if (options.fileType != TextObjectExporter::FileType::Auto)
        {
            return options.fileType;
        }

        return TextObjectExporter::FileType::Unknown;
    }

    char32_t decodeCp437Byte(unsigned char value)
    {
        if (value <= 0x7F)
        {
            return UnicodeConversion::sanitizeCodePoint(static_cast<char32_t>(value));
        }

        static const char32_t kCp437Table[128] =
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

        return UnicodeConversion::sanitizeCodePoint(kCp437Table[value - 0x80]);
    }

    Color::Basic dosBasicColor(int value)
    {
        switch (value & 0x0F)
        {
        case 0x0: return Color::Basic::Black;
        case 0x1: return Color::Basic::Blue;
        case 0x2: return Color::Basic::Green;
        case 0x3: return Color::Basic::Cyan;
        case 0x4: return Color::Basic::Red;
        case 0x5: return Color::Basic::Magenta;
        case 0x6: return Color::Basic::Yellow;
        case 0x7: return Color::Basic::White;
        case 0x8: return Color::Basic::BrightBlack;
        case 0x9: return Color::Basic::BrightBlue;
        case 0xA: return Color::Basic::BrightGreen;
        case 0xB: return Color::Basic::BrightCyan;
        case 0xC: return Color::Basic::BrightRed;
        case 0xD: return Color::Basic::BrightMagenta;
        case 0xE: return Color::Basic::BrightYellow;
        case 0xF: return Color::Basic::BrightWhite;
        default: return Color::Basic::White;
        }
    }

    int dosColorNibble(Color::Basic basic)
    {
        switch (basic)
        {
        case Color::Basic::Black:         return 0x0;
        case Color::Basic::Blue:          return 0x1;
        case Color::Basic::Green:         return 0x2;
        case Color::Basic::Cyan:          return 0x3;
        case Color::Basic::Red:           return 0x4;
        case Color::Basic::Magenta:       return 0x5;
        case Color::Basic::Yellow:        return 0x6;
        case Color::Basic::White:         return 0x7;
        case Color::Basic::BrightBlack:   return 0x8;
        case Color::Basic::BrightBlue:    return 0x9;
        case Color::Basic::BrightGreen:   return 0xA;
        case Color::Basic::BrightCyan:    return 0xB;
        case Color::Basic::BrightRed:     return 0xC;
        case Color::Basic::BrightMagenta: return 0xD;
        case Color::Basic::BrightYellow:  return 0xE;
        case Color::Basic::BrightWhite:   return 0xF;
        default:                          return 0x7;
        }
    }

    Style styleFromDosAttribute(unsigned char attribute, bool iceColors)
    {
        Style style;
        style = style.withForeground(Color::FromBasic(dosBasicColor(attribute & 0x0F)));

        if (iceColors)
        {
            style = style.withBackground(Color::FromBasic(dosBasicColor((attribute >> 4) & 0x0F)));
        }
        else
        {
            style = style.withBackground(Color::FromBasic(dosBasicColor((attribute >> 4) & 0x07)));
            if ((attribute & 0x80) != 0)
            {
                style = style.withSlowBlink(true);
            }
        }

        return style;
    }

    TextObjectExporter::Encoding resolveEncoding(
        TextObjectExporter::FileType fileType,
        const TextObjectExporter::SaveOptions& options,
        std::string& outError)
    {
        using namespace TextObjectExporter;

        if (fileType == FileType::Nfo)
        {
            if (options.encoding == Encoding::Auto)
            {
                return Encoding::Cp437;
            }

            if (options.encoding != Encoding::Cp437 &&
                !options.allowNonCp437NfoEncoding)
            {
                outError =
                    "NFO export only permits CP437 unless SaveOptions::allowNonCp437NfoEncoding is true.";
                return Encoding::Auto;
            }

            return options.encoding;
        }

        if (fileType == FileType::Ans || fileType == FileType::Bin)
        {
            if (options.encoding == Encoding::Auto || options.encoding == Encoding::Cp437)
            {
                return Encoding::Cp437;
            }

            outError =
                "ANSI and BIN export currently require CP437 encoding so the output remains explicit about terminal-art format limits.";
            return Encoding::Auto;
        }

        if (fileType == FileType::Xp)
        {
            return Encoding::Binary;
        }

        if (options.encoding != Encoding::Auto)
        {
            return options.encoding;
        }

        return Encoding::Utf8;
    }

    void addWarning(
        TextObjectExporter::SaveResult& result,
        TextObjectExporter::SaveWarningCode code,
        const std::string& message)
    {
        TextObjectExporter::SaveWarning warning;
        warning.code = code;
        warning.message = message;
        result.warnings.push_back(warning);
    }


    bool warningImpliesTerminalArtFidelityLoss(TextObjectExporter::SaveWarningCode code)
    {
        switch (code)
        {
        case TextObjectExporter::SaveWarningCode::LossyConversionOccurred:
        case TextObjectExporter::SaveWarningCode::TerminalArtColorApproximationOccurred:
        case TextObjectExporter::SaveWarningCode::TerminalArtThemeColorApproximationOccurred:
        case TextObjectExporter::SaveWarningCode::TerminalArtUnsupportedStyleDropped:
        case TextObjectExporter::SaveWarningCode::TerminalArtReverseApproximated:
        case TextObjectExporter::SaveWarningCode::TerminalArtBoldApproximated:
        case TextObjectExporter::SaveWarningCode::TerminalArtIceColorExportUsed:
        case TextObjectExporter::SaveWarningCode::HighFidelityXpRecommended:
            return true;
        default:
            return false;
        }
    }

    bool shouldRecommendXpForResult(const TextObjectExporter::SaveResult& result)
    {
        if (result.resolvedFileType != TextObjectExporter::FileType::Ans &&
            result.resolvedFileType != TextObjectExporter::FileType::Bin)
        {
            return false;
        }

        for (const TextObjectExporter::SaveWarning& warning : result.warnings)
        {
            if (warningImpliesTerminalArtFidelityLoss(warning.code))
            {
                return true;
            }
        }

        return false;
    }

    std::string xpRecommendationMessage(TextObjectExporter::FileType sourceFileType)
    {
        std::ostringstream stream;
        stream
            << "XP is the recommended high-fidelity export format when "
            << TextObjectExporter::toString(sourceFileType)
            << " export would approximate colors, drop style information, or replace glyphs.";
        return stream.str();
    }

    bool tryEncodeAscii(
        char32_t codePoint,
        char replacementChar,
        bool allowLossyConversion,
        std::string& outBytes,
        bool& outLossy)
    {
        codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);

        if (codePoint <= 0x7F)
        {
            outBytes.push_back(static_cast<char>(codePoint));
            return true;
        }

        if (!allowLossyConversion)
        {
            return false;
        }

        outBytes.push_back(replacementChar);
        outLossy = true;
        return true;
    }

    bool tryEncodeLatin1(
        char32_t codePoint,
        char replacementChar,
        bool allowLossyConversion,
        std::string& outBytes,
        bool& outLossy)
    {
        codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);

        if (codePoint <= 0xFF)
        {
            outBytes.push_back(static_cast<char>(static_cast<unsigned char>(codePoint)));
            return true;
        }

        if (!allowLossyConversion)
        {
            return false;
        }

        outBytes.push_back(replacementChar);
        outLossy = true;
        return true;
    }

    bool tryEncodeCp437(
        char32_t codePoint,
        char replacementChar,
        bool allowLossyConversion,
        std::string& outBytes,
        bool& outLossy)
    {
        codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);

        if (codePoint <= 0x7F)
        {
            outBytes.push_back(static_cast<char>(codePoint));
            return true;
        }

        for (int i = 0; i < 128; ++i)
        {
            const unsigned char candidate = static_cast<unsigned char>(0x80 + i);
            if (decodeCp437Byte(candidate) == codePoint)
            {
                outBytes.push_back(static_cast<char>(candidate));
                return true;
            }
        }

        if (!allowLossyConversion)
        {
            return false;
        }

        outBytes.push_back(replacementChar);
        outLossy = true;
        return true;
    }

    std::vector<ExportCodePoint> buildPlainTextExportCodePoints(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options)
    {
        std::vector<ExportCodePoint> result;

        if (!object.isLoaded())
        {
            return result;
        }

        for (int row = 0; row < object.getHeight(); ++row)
        {
            const std::size_t lineStart = result.size();

            for (int x = 0; x < object.getWidth(); ++x)
            {
                const TextObjectCell& cell = object.getCell(x, row);

                if (cell.kind == CellKind::WideTrailing ||
                    cell.kind == CellKind::CombiningContinuation)
                {
                    continue;
                }

                ExportCodePoint item;
                item.source.x = x;
                item.source.y = row;
                item.value = (cell.kind == CellKind::Empty)
                    ? U' '
                    : UnicodeConversion::sanitizeCodePoint(cell.glyph);

                result.push_back(item);
            }

            if (!options.preserveTrailingSpaces)
            {
                while (result.size() > lineStart && result.back().value == U' ')
                {
                    result.pop_back();
                }
            }

            if (row + 1 < object.getHeight())
            {
                ExportCodePoint newline;
                newline.value = U'\n';
                newline.source.x = -1;
                newline.source.y = row;
                result.push_back(newline);
            }
        }

        return result;
    }

    bool tryEncodeCodePoints(
        const std::vector<ExportCodePoint>& codePoints,
        TextObjectExporter::Encoding encoding,
        const TextObjectExporter::SaveOptions& options,
        std::string& outBytes,
        std::size_t& outLossyCount,
        char32_t& outFirstFailingCodePoint,
        TextObjectExporter::SourcePosition& outFirstFailingPosition)
    {
        outBytes.clear();
        outLossyCount = 0;
        outFirstFailingCodePoint = U'\0';
        outFirstFailingPosition = {};

        if (encoding == TextObjectExporter::Encoding::Utf8)
        {
            std::u32string text;
            text.reserve(codePoints.size());

            for (const ExportCodePoint& item : codePoints)
            {
                text.push_back(item.value);
            }

            outBytes = UnicodeConversion::u32ToUtf8(text);
            return true;
        }

        outBytes.reserve(codePoints.size());

        for (const ExportCodePoint& item : codePoints)
        {
            bool lossyForThisCodePoint = false;
            bool success = false;

            switch (encoding)
            {
            case TextObjectExporter::Encoding::Ascii:
                success = tryEncodeAscii(
                    item.value,
                    options.replacementChar,
                    options.allowLossyConversion,
                    outBytes,
                    lossyForThisCodePoint);
                break;

            case TextObjectExporter::Encoding::Latin1:
                success = tryEncodeLatin1(
                    item.value,
                    options.replacementChar,
                    options.allowLossyConversion,
                    outBytes,
                    lossyForThisCodePoint);
                break;

            case TextObjectExporter::Encoding::Cp437:
                success = tryEncodeCp437(
                    item.value,
                    options.replacementChar,
                    options.allowLossyConversion,
                    outBytes,
                    lossyForThisCodePoint);
                break;

            case TextObjectExporter::Encoding::Auto:
            case TextObjectExporter::Encoding::Utf8:
            default:
                success = false;
                break;
            }

            if (!success)
            {
                outFirstFailingCodePoint = item.value;
                outFirstFailingPosition = item.source;
                return false;
            }

            if (lossyForThisCodePoint)
            {
                ++outLossyCount;
            }
        }

        return true;
    }

    std::string applyLineEndings(
        std::string_view bytes,
        TextObjectExporter::LineEnding lineEnding)
    {
        if (lineEnding == TextObjectExporter::LineEnding::Lf)
        {
            return std::string(bytes);
        }

        std::string result;
        result.reserve(bytes.size());

        for (char ch : bytes)
        {
            if (ch == '\n')
            {
                result.push_back('\r');
                result.push_back('\n');
                continue;
            }

            result.push_back(ch);
        }

        return result;
    }

    std::string toHexCodePoint(char32_t codePoint)
    {
        std::ostringstream stream;
        stream << "U+";
        stream << std::hex << std::uppercase;
        stream.width(4);
        stream.fill('0');
        stream << static_cast<std::uint32_t>(codePoint);
        return stream.str();
    }

    std::string buildUnsupportedCellMessage(const TextObjectCell& cell)
    {
        if (cell.kind == CellKind::WideTrailing)
        {
            return "Terminal-art export cannot serialize WideTrailing cells directly.";
        }

        if (cell.kind == CellKind::CombiningContinuation)
        {
            return "Terminal-art export cannot serialize CombiningContinuation cells directly.";
        }

        if (cell.kind == CellKind::Glyph && cell.width != CellWidth::One)
        {
            return "Terminal-art export currently requires every emitted glyph to occupy exactly one cell.";
        }

        return "Terminal-art export encountered an unsupported cell kind.";
    }

    bool validateTerminalArtCells(
        const TextObject& object,
        TextObjectExporter::SaveResult& outResult)
    {
        for (int y = 0; y < object.getHeight(); ++y)
        {
            for (int x = 0; x < object.getWidth(); ++x)
            {
                const TextObjectCell& cell = object.getCell(x, y);

                if (cell.kind == CellKind::WideTrailing ||
                    cell.kind == CellKind::CombiningContinuation ||
                    (cell.kind == CellKind::Glyph && cell.width != CellWidth::One))
                {
                    outResult.hasEncodingFailure = true;
                    outResult.firstFailingCodePoint = cell.glyph;
                    outResult.firstFailingPosition = { x, y };
                    outResult.errorMessage = buildUnsupportedCellMessage(cell);
                    return false;
                }
            }
        }

        return true;
    }

    bool tryEncodeCp437Glyph(
        char32_t codePoint,
        const TextObjectExporter::SaveOptions& options,
        char& outByte,
        bool& outLossy)
    {
        std::string encoded;
        outLossy = false;

        if (!tryEncodeCp437(
            codePoint,
            options.replacementChar,
            options.allowLossyConversion,
            encoded,
            outLossy))
        {
            return false;
        }

        if (encoded.empty())
        {
            return false;
        }

        outByte = encoded[0];
        return true;
    }

    bool selectThemeColorForTerminalArt(
        const ThemeColor& themeColor,
        Color& outColor,
        bool& outApproximate)
    {
        if (themeColor.hasBasic())
        {
            outColor = *themeColor.basic();
            return true;
        }

        if (themeColor.hasIndexed())
        {
            outColor = ColorMapping::indexedToBasic(*themeColor.indexed());
            outApproximate = true;
            return true;
        }

        if (themeColor.hasRgb())
        {
            outColor = ColorMapping::rgbToBasic(*themeColor.rgb());
            outApproximate = true;
            return true;
        }

        return false;
    }

    bool resolveStyleColorValueToBasic(
        const std::optional<Style::StyleColorValue>& colorValue,
        const std::optional<Color>& concreteFallback,
        const std::optional<ThemeColor>& themeFallback,
        Color::Basic defaultColor,
        bool allowApproximation,
        Color::Basic& outBasic,
        bool& outApproximatedColor,
        bool& outApproximatedTheme)
    {
        auto consumeConcrete = [&](const Color& color) -> bool
            {
                if (color.isDefault())
                {
                    outBasic = defaultColor;
                    return true;
                }

                if (color.isBasic16())
                {
                    outBasic = color.basic();
                    return true;
                }

                if (!allowApproximation)
                {
                    return false;
                }

                outApproximatedColor = true;
                outBasic = ColorMapping::mapToSupport(color, ColorSupport::Basic16).basic();
                return true;
            };

        auto consumeTheme = [&](const ThemeColor& themeColor) -> bool
            {
                Color resolved;
                bool approximateTheme = false;
                if (!selectThemeColorForTerminalArt(themeColor, resolved, approximateTheme))
                {
                    return false;
                }

                outApproximatedTheme = approximateTheme || !themeColor.hasBasic();
                return consumeConcrete(resolved);
            };

        if (colorValue.has_value())
        {
            if (colorValue->hasConcreteColor())
            {
                return consumeConcrete(*colorValue->concreteColor());
            }

            if (colorValue->hasThemeColor())
            {
                return consumeTheme(*colorValue->themeColor());
            }
        }

        if (concreteFallback.has_value())
        {
            return consumeConcrete(*concreteFallback);
        }

        if (themeFallback.has_value())
        {
            return consumeTheme(*themeFallback);
        }

        outBasic = defaultColor;
        return true;
    }

    bool resolveTerminalArtStyle(
        const std::optional<Style>& cellStyle,
        const TextObjectExporter::SaveOptions& options,
        bool forBin,
        TerminalArtStyleInfo& outInfo)
    {
        outInfo = TerminalArtStyleInfo{};

        if (!cellStyle.has_value() || cellStyle->isEmpty())
        {
            outInfo.dosAttribute = 0x07;
            outInfo.emittedStyle = styleFromDosAttribute(outInfo.dosAttribute, options.enableIceColors);
            return true;
        }

        const Style& style = *cellStyle;
        Color::Basic foreground = Color::Basic::White;
        Color::Basic background = Color::Basic::Black;

        if (!resolveStyleColorValueToBasic(
            style.foregroundColorValue(),
            style.foreground(),
            style.foregroundThemeColor(),
            Color::Basic::White,
            options.allowTerminalArtApproximation,
            foreground,
            outInfo.approximatedColor,
            outInfo.approximatedThemeColor))
        {
            return false;
        }

        if (!resolveStyleColorValueToBasic(
            style.backgroundColorValue(),
            style.background(),
            style.backgroundThemeColor(),
            Color::Basic::Black,
            options.allowTerminalArtApproximation,
            background,
            outInfo.approximatedColor,
            outInfo.approximatedThemeColor))
        {
            return false;
        }

        if (style.reverse())
        {
            std::swap(foreground, background);
            outInfo.approximatedReverse = true;
        }

        if (forBin && style.bold() && dosColorNibble(foreground) < 8)
        {
            foreground = dosBasicColor(dosColorNibble(foreground) | 0x08);
            outInfo.approximatedBold = true;
        }

        unsigned char attribute = static_cast<unsigned char>(dosColorNibble(foreground) & 0x0F);

        const int backgroundNibble = dosColorNibble(background);
        if (options.enableIceColors)
        {
            attribute = static_cast<unsigned char>(attribute | ((backgroundNibble & 0x0F) << 4));
            outInfo.usedIceColors = ((backgroundNibble & 0x08) != 0);
        }
        else
        {
            attribute = static_cast<unsigned char>(attribute | ((backgroundNibble & 0x07) << 4));
            if (style.slowBlink() || style.fastBlink())
            {
                attribute = static_cast<unsigned char>(attribute | 0x80);
            }
            else if ((backgroundNibble & 0x08) != 0)
            {
                if (!options.allowTerminalArtApproximation)
                {
                    return false;
                }

                outInfo.approximatedColor = true;
                outInfo.usedIceColors = true;
            }
        }

        if (forBin)
        {
            if (style.dim() ||
                style.underline() ||
                style.invisible() ||
                style.strike() ||
                style.fastBlink())
            {
                outInfo.droppedUnsupportedStyle = true;
            }
        }

        outInfo.dosAttribute = attribute;

        Style emitted = styleFromDosAttribute(attribute, options.enableIceColors);
        if (!forBin)
        {
            if (style.bold())
            {
                emitted = emitted.withBold(true);
            }

            if (style.dim())
            {
                emitted = emitted.withDim(true);
            }

            if (style.underline())
            {
                emitted = emitted.withUnderline(true);
            }

            if (style.reverse())
            {
                emitted = emitted.withReverse(true);
            }

            if (style.invisible())
            {
                emitted = emitted.withInvisible(true);
            }

            if (style.strike())
            {
                emitted = emitted.withStrike(true);
            }

            if (style.fastBlink())
            {
                emitted = emitted.withFastBlink(true);
            }
        }

        outInfo.emittedStyle = emitted;
        return true;
    }

    void appendTerminalArtWarnings(
        TextObjectExporter::SaveResult& result,
        bool approximatedColor,
        bool approximatedThemeColor,
        bool droppedUnsupportedStyle,
        bool approximatedReverse,
        bool approximatedBold,
        bool usedIceColors,
        bool forcedCp437,
        bool forcedTrailingSpaces)
    {
        using namespace TextObjectExporter;

        if (forcedCp437)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtEncodingForcedToCp437,
                "ANSI/BIN export uses CP437 bytes for glyph data. A different logical encoding was not used.");
        }

        if (approximatedColor)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtColorApproximationOccurred,
                "One or more terminal-art colors were approximated to the DOS/basic-16 palette.");
        }

        if (approximatedThemeColor)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtThemeColorApproximationOccurred,
                "One or more theme colors were resolved through terminal-art palette fallback rather than preserved as authored theme intent.");
        }

        if (droppedUnsupportedStyle)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtUnsupportedStyleDropped,
                "One or more TextObject style flags are not representable in BIN and were dropped explicitly.");
        }

        if (approximatedReverse)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtReverseApproximated,
                "Reverse styling was approximated by swapping foreground and background colors before export.");
        }

        if (approximatedBold)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtBoldApproximated,
                "BIN bold styling was approximated through the DOS bright-foreground bit.");
        }

        if (usedIceColors)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtIceColorExportUsed,
                "BIN export used high-background ICE color attributes. Consumers that interpret bit 7 as blink may not match the authored result.");
        }

        if (forcedTrailingSpaces)
        {
            addWarning(
                result,
                SaveWarningCode::TerminalArtTrailingSpacesForced,
                "BIN export always writes the full rectangular grid, so trailing spaces were preserved regardless of SaveOptions::preserveTrailingSpaces.");
        }
    }

    bool exportAnsi(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options,
        TextObjectExporter::SaveResult& result)
    {
        if (!validateTerminalArtCells(object, result))
        {
            return false;
        }

        RendererCapabilities capabilities = RendererCapabilities::VirtualTerminal();
        capabilities.colorTier = ColorSupport::Basic16;
        capabilities.brightBasicColors = RendererFeatureSupport::Supported;
        capabilities.bold = RendererFeatureSupport::Supported;
        capabilities.dim = RendererFeatureSupport::Supported;
        capabilities.underline = RendererFeatureSupport::Supported;
        capabilities.reverse = RendererFeatureSupport::Supported;
        capabilities.invisible = RendererFeatureSupport::Supported;
        capabilities.strike = RendererFeatureSupport::Supported;
        capabilities.slowBlink = RendererFeatureSupport::Supported;
        capabilities.fastBlink = RendererFeatureSupport::Supported;

        SgrEmitter emitter;
        emitter.setCapabilities(capabilities);
        emitter.reset();

        std::string output;
        bool approximatedColor = false;
        bool approximatedThemeColor = false;
        bool droppedUnsupportedStyle = false;
        bool approximatedReverse = false;
        bool approximatedBold = false;
        bool usedIceColors = false;
        bool hadLossyConversion = false;
        std::size_t lossyCount = 0;

        for (int y = 0; y < object.getHeight(); ++y)
        {
            for (int x = 0; x < object.getWidth(); ++x)
            {
                const TextObjectCell& cell = object.getCell(x, y);
                if (cell.kind == CellKind::WideTrailing ||
                    cell.kind == CellKind::CombiningContinuation)
                {
                    continue;
                }

                TerminalArtStyleInfo styleInfo;
                if (!resolveTerminalArtStyle(cell.style, options, false, styleInfo))
                {
                    result.hasEncodingFailure = true;
                    result.firstFailingCodePoint = cell.glyph;
                    result.firstFailingPosition = { x, y };
                    result.errorMessage =
                        "ANSI export could not represent one or more authored colors without approximation, and approximation was disabled.";
                    return false;
                }

                approximatedColor = approximatedColor || styleInfo.approximatedColor;
                approximatedThemeColor = approximatedThemeColor || styleInfo.approximatedThemeColor;
                droppedUnsupportedStyle = droppedUnsupportedStyle || styleInfo.droppedUnsupportedStyle;
                approximatedReverse = approximatedReverse || styleInfo.approximatedReverse;
                approximatedBold = approximatedBold || styleInfo.approximatedBold;
                usedIceColors = usedIceColors || styleInfo.usedIceColors;

                emitter.appendTransitionTo(output, styleInfo.emittedStyle);

                const char32_t glyph = (cell.kind == CellKind::Empty)
                    ? U' '
                    : UnicodeConversion::sanitizeCodePoint(cell.glyph);

                char encodedGlyph = ' ';
                bool lossyForGlyph = false;
                if (!tryEncodeCp437Glyph(glyph, options, encodedGlyph, lossyForGlyph))
                {
                    result.hasEncodingFailure = true;
                    result.firstFailingCodePoint = glyph;
                    result.firstFailingPosition = { x, y };
                    result.errorMessage =
                        "ANSI export failed because at least one glyph is not representable as a single CP437 byte.";
                    return false;
                }

                output.push_back(encodedGlyph);

                if (lossyForGlyph)
                {
                    hadLossyConversion = true;
                    ++lossyCount;
                }
            }

            if (y + 1 < object.getHeight())
            {
                if (options.lineEnding == TextObjectExporter::LineEnding::CrLf)
                {
                    output += "\r\n";
                }
                else
                {
                    output.push_back('\n');
                }
            }
        }

        if (options.ansiEmitFinalReset)
        {
            emitter.appendReset(output);
        }

        result.bytes = output;
        result.hadLossyConversion = hadLossyConversion;
        result.lossyCodePointCount = lossyCount;

        if (result.hadLossyConversion)
        {
            std::ostringstream warning;
            warning << "Lossy conversion occurred for "
                << result.lossyCodePointCount
                << " code point(s).";

            addWarning(
                result,
                TextObjectExporter::SaveWarningCode::LossyConversionOccurred,
                warning.str());
        }

        appendTerminalArtWarnings(
            result,
            approximatedColor,
            approximatedThemeColor,
            droppedUnsupportedStyle,
            approximatedReverse,
            approximatedBold,
            usedIceColors,
            true,
            false);

        if (shouldRecommendXpForResult(result))
        {
            addWarning(
                result,
                TextObjectExporter::SaveWarningCode::HighFidelityXpRecommended,
                xpRecommendationMessage(TextObjectExporter::FileType::Ans));
        }

        result.success = true;
        return true;
    }

    bool exportBin(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options,
        TextObjectExporter::SaveResult& result)
    {
        if (!validateTerminalArtCells(object, result))
        {
            return false;
        }

        std::string output;
        output.reserve(static_cast<std::size_t>(object.getWidth()) * static_cast<std::size_t>(object.getHeight()) * 2u);

        bool approximatedColor = false;
        bool approximatedThemeColor = false;
        bool droppedUnsupportedStyle = false;
        bool approximatedReverse = false;
        bool approximatedBold = false;
        bool usedIceColors = false;
        bool hadLossyConversion = false;
        std::size_t lossyCount = 0;

        for (int y = 0; y < object.getHeight(); ++y)
        {
            for (int x = 0; x < object.getWidth(); ++x)
            {
                const TextObjectCell& cell = object.getCell(x, y);

                TerminalArtStyleInfo styleInfo;
                if (!resolveTerminalArtStyle(cell.style, options, true, styleInfo))
                {
                    result.hasEncodingFailure = true;
                    result.firstFailingCodePoint = cell.glyph;
                    result.firstFailingPosition = { x, y };
                    result.errorMessage =
                        "BIN export could not represent one or more authored colors without approximation, and approximation was disabled.";
                    return false;
                }

                approximatedColor = approximatedColor || styleInfo.approximatedColor;
                approximatedThemeColor = approximatedThemeColor || styleInfo.approximatedThemeColor;
                droppedUnsupportedStyle = droppedUnsupportedStyle || styleInfo.droppedUnsupportedStyle;
                approximatedReverse = approximatedReverse || styleInfo.approximatedReverse;
                approximatedBold = approximatedBold || styleInfo.approximatedBold;
                usedIceColors = usedIceColors || styleInfo.usedIceColors;

                const char32_t glyph = (cell.kind == CellKind::Empty)
                    ? U' '
                    : UnicodeConversion::sanitizeCodePoint(cell.glyph);

                char encodedGlyph = ' ';
                bool lossyForGlyph = false;
                if (!tryEncodeCp437Glyph(glyph, options, encodedGlyph, lossyForGlyph))
                {
                    result.hasEncodingFailure = true;
                    result.firstFailingCodePoint = glyph;
                    result.firstFailingPosition = { x, y };
                    result.errorMessage =
                        "BIN export failed because at least one glyph is not representable as a single CP437 byte.";
                    return false;
                }

                output.push_back(encodedGlyph);
                output.push_back(static_cast<char>(styleInfo.dosAttribute));

                if (lossyForGlyph)
                {
                    hadLossyConversion = true;
                    ++lossyCount;
                }
            }
        }

        result.bytes = output;
        result.hadLossyConversion = hadLossyConversion;
        result.lossyCodePointCount = lossyCount;

        if (result.hadLossyConversion)
        {
            std::ostringstream warning;
            warning << "Lossy conversion occurred for "
                << result.lossyCodePointCount
                << " code point(s).";

            addWarning(
                result,
                TextObjectExporter::SaveWarningCode::LossyConversionOccurred,
                warning.str());
        }

        appendTerminalArtWarnings(
            result,
            approximatedColor,
            approximatedThemeColor,
            droppedUnsupportedStyle,
            approximatedReverse,
            approximatedBold,
            usedIceColors,
            true,
            !options.preserveTrailingSpaces);

        if (shouldRecommendXpForResult(result))
        {
            addWarning(
                result,
                TextObjectExporter::SaveWarningCode::HighFidelityXpRecommended,
                xpRecommendationMessage(TextObjectExporter::FileType::Bin));
        }

        result.success = true;
        return true;
    }
}

namespace TextObjectExporter
{
    FileType detectFileType(const std::string& filePath)
    {
        const std::string ext = getFileExtensionLower(filePath);

        if (ext == ".txt")
        {
            return FileType::Txt;
        }

        if (ext == ".asc")
        {
            return FileType::Asc;
        }

        if (ext == ".diz")
        {
            return FileType::Diz;
        }

        if (ext == ".nfo")
        {
            return FileType::Nfo;
        }

        if (ext == ".ans")
        {
            return FileType::Ans;
        }

        if (ext == ".bin")
        {
            return FileType::Bin;
        }

        if (ext == ".xp")
        {
            return FileType::Xp;
        }

        return FileType::Unknown;
    }

    Encoding resolveEffectiveEncoding(
        FileType fileType,
        const SaveOptions& options,
        std::string* outErrorMessage)
    {
        std::string error;
        const Encoding encoding = resolveEncoding(fileType, options, error);

        if (outErrorMessage != nullptr)
        {
            *outErrorMessage = error;
        }

        return encoding;
    }

    bool canEncodeCodePoint(char32_t codePoint, Encoding encoding)
    {
        std::string bytes;
        bool lossy = false;

        const SaveOptions strictOptions = []()
            {
                SaveOptions options;
                options.allowLossyConversion = false;
                options.replacementChar = '?';
                return options;
            }();

        switch (encoding)
        {
        case Encoding::Utf8:
            return true;

        case Encoding::Ascii:
            return tryEncodeAscii(
                codePoint,
                strictOptions.replacementChar,
                strictOptions.allowLossyConversion,
                bytes,
                lossy);

        case Encoding::Latin1:
            return tryEncodeLatin1(
                codePoint,
                strictOptions.replacementChar,
                strictOptions.allowLossyConversion,
                bytes,
                lossy);

        case Encoding::Cp437:
            return tryEncodeCp437(
                codePoint,
                strictOptions.replacementChar,
                strictOptions.allowLossyConversion,
                bytes,
                lossy);

        case Encoding::Binary:
            return true;

        case Encoding::Auto:
        default:
            return false;
        }
    }

    SaveResult exportToBytes(const TextObject& object, const SaveOptions& options)
    {
        SaveResult result;
        result.resolvedFileType = resolveFileType(options);
        result.resolvedLineEnding = options.lineEnding;

        if (!object.isLoaded())
        {
            result.errorMessage = "Cannot export an unloaded TextObject.";
            return result;
        }

        std::string encodingError;
        result.resolvedEncoding = resolveEffectiveEncoding(
            result.resolvedFileType,
            options,
            &encodingError);
        if (result.resolvedEncoding == Encoding::Auto)
        {
            result.errorMessage = encodingError;
            return result;
        }

        if (result.resolvedFileType == FileType::Nfo &&
            result.resolvedEncoding != Encoding::Cp437 &&
            options.allowNonCp437NfoEncoding)
        {
            addWarning(
                result,
                SaveWarningCode::NonCp437NfoEncodingOverride,
                "NFO export is using a non-CP437 encoding by explicit override. Compatibility with traditional NFO viewers may be reduced.");
        }

        switch (result.resolvedFileType)
        {
        case FileType::Ans:
            return exportAnsi(object, options, result) ? result : result;

        case FileType::Bin:
            return exportBin(object, options, result) ? result : result;

        case FileType::Xp:
            return XpArtExporter::exportToBytes(object, options, result) ? result : result;

        case FileType::Auto:
        case FileType::Unknown:
        case FileType::Txt:
        case FileType::Asc:
        case FileType::Diz:
        case FileType::Nfo:
        default:
            break;
        }

        const std::vector<ExportCodePoint> codePoints = buildPlainTextExportCodePoints(object, options);

        std::string encodedBytes;
        std::size_t lossyCount = 0;
        char32_t firstFailingCodePoint = U'\0';
        SourcePosition firstFailingPosition;

        if (!tryEncodeCodePoints(
            codePoints,
            result.resolvedEncoding,
            options,
            encodedBytes,
            lossyCount,
            firstFailingCodePoint,
            firstFailingPosition))
        {
            result.hasEncodingFailure = true;
            result.firstFailingCodePoint = firstFailingCodePoint;
            result.firstFailingPosition = firstFailingPosition;
            result.errorMessage =
                "Export failed because the TextObject contains code points that cannot be represented in the selected encoding without lossy conversion.";
            return result;
        }

        result.hadLossyConversion = (lossyCount > 0);
        result.lossyCodePointCount = lossyCount;

        if (result.hadLossyConversion)
        {
            std::ostringstream warning;
            warning << "Lossy conversion occurred for "
                << result.lossyCodePointCount
                << " code point(s).";

            addWarning(
                result,
                SaveWarningCode::LossyConversionOccurred,
                warning.str());
        }

        result.bytes = applyLineEndings(encodedBytes, options.lineEnding);

        if (result.resolvedEncoding == Encoding::Utf8 && options.includeUtf8Bom)
        {
            result.bytes.insert(0, "\xEF\xBB\xBF");
            result.usedUtf8Bom = true;

            addWarning(
                result,
                SaveWarningCode::Utf8BomIncluded,
                "UTF-8 BOM was included in the exported output.");
        }

        result.success = true;
        return result;
    }

    SaveResult saveToFile(const TextObject& object, const std::string& filePath, const SaveOptions& options)
    {
        SaveOptions resolvedOptions = options;

        if (resolvedOptions.fileType == FileType::Auto)
        {
            resolvedOptions.fileType = detectFileType(filePath);
        }

        SaveResult result = exportToBytes(object, resolvedOptions);
        result.outputPath = filePath;
        result.resolvedFileType = resolvedOptions.fileType;

        if (!result.success)
        {
            return result;
        }

        std::ofstream file(filePath, std::ios::binary);
        if (!file)
        {
            result.success = false;
            result.errorMessage = "Failed to open output file for writing.";
            result.bytes.clear();
            return result;
        }

        file.write(result.bytes.data(), static_cast<std::streamsize>(result.bytes.size()));
        if (!file)
        {
            result.success = false;
            result.errorMessage = "Failed while writing output file.";
            result.bytes.clear();
            return result;
        }

        return result;
    }

    bool trySaveToFile(const TextObject& object, const std::string& filePath)
    {
        return trySaveToFile(object, filePath, SaveOptions{});
    }

    bool trySaveToFile(const TextObject& object, const std::string& filePath, const SaveOptions& options)
    {
        return saveToFile(object, filePath, options).success;
    }

    std::string formatSaveError(const SaveResult& result)
    {
        if (result.success)
        {
            return {};
        }

        std::ostringstream message;

        if (!result.outputPath.empty())
        {
            message << "TextObject export failed for \"" << result.outputPath << "\". ";
        }

        if (!result.errorMessage.empty())
        {
            message << result.errorMessage;
        }
        else
        {
            message << "TextObject export failed.";
        }

        if (result.hasEncodingFailure)
        {
            message << " Encoding=" << toString(result.resolvedEncoding) << ".";

            if (result.firstFailingCodePoint != U'\0')
            {
                message << " First failing code point=" << toHexCodePoint(result.firstFailingCodePoint) << ".";
            }

            if (result.firstFailingPosition.isValid())
            {
                message << " Position=("
                    << result.firstFailingPosition.x
                    << ", "
                    << result.firstFailingPosition.y
                    << ").";
            }
        }

        if (!result.warnings.empty())
        {
            message << " Warnings=" << result.warnings.size() << ".";
        }

        if (shouldRecommendXpForResult(result))
        {
            message << " Recommendation=Prefer XP for higher-fidelity export.";
        }

        return message.str();
    }

    std::string formatSaveSuccess(const SaveResult& result)
    {
        if (!result.success)
        {
            return {};
        }

        std::ostringstream message;
        message << "TextObject export succeeded";

        if (!result.outputPath.empty())
        {
            message << " for \"" << result.outputPath << "\"";
        }

        message << ".";
        message << " FileType=" << toString(result.resolvedFileType) << ".";
        message << " Encoding=" << toString(result.resolvedEncoding) << ".";
        message << " LineEnding=" << toString(result.resolvedLineEnding) << ".";
        message << " Bytes=" << result.bytes.size() << ".";

        if (result.warnings.empty())
        {
            message << " No warnings.";
        }
        else
        {
            message << " Warnings=" << result.warnings.size() << ".";
        }

        if (shouldRecommendXpForResult(result))
        {
            message << " Recommendation=Prefer XP for higher-fidelity export.";
        }

        return message.str();
    }

    bool hasWarning(const SaveResult& result, SaveWarningCode code)
    {
        for (const SaveWarning& warning : result.warnings)
        {
            if (warning.code == code)
            {
                return true;
            }
        }

        return false;
    }

    bool shouldRecommendXpForFidelity(const SaveResult& result)
    {
        return shouldRecommendXpForResult(result);
    }

    const SaveWarning* getWarningByCode(
        const SaveResult& result,
        SaveWarningCode code)
    {
        for (const SaveWarning& warning : result.warnings)
        {
            if (warning.code == code)
            {
                return &warning;
            }
        }

        return nullptr;
    }

    const char* toString(FileType fileType)
    {
        switch (fileType)
        {
        case FileType::Auto:
            return "Auto";
        case FileType::Unknown:
            return "Unknown";
        case FileType::Txt:
            return "TXT";
        case FileType::Asc:
            return "ASC";
        case FileType::Diz:
            return "DIZ";
        case FileType::Nfo:
            return "NFO";
        case FileType::Ans:
            return "ANS";
        case FileType::Bin:
            return "BIN";
        case FileType::Xp:
            return "XP";
        default:
            return "Unknown";
        }
    }

    const char* toString(Encoding encoding)
    {
        switch (encoding)
        {
        case Encoding::Auto:
            return "Auto";
        case Encoding::Utf8:
            return "UTF-8";
        case Encoding::Ascii:
            return "ASCII";
        case Encoding::Latin1:
            return "Latin-1";
        case Encoding::Cp437:
            return "CP437";
        case Encoding::Binary:
            return "Binary";
        default:
            return "Unknown";
        }
    }

    const char* toString(LineEnding lineEnding)
    {
        switch (lineEnding)
        {
        case LineEnding::Lf:
            return "LF";
        case LineEnding::CrLf:
            return "CRLF";
        default:
            return "Unknown";
        }
    }

    const char* toString(SaveWarningCode warningCode)
    {
        switch (warningCode)
        {
        case SaveWarningCode::None:
            return "None";
        case SaveWarningCode::NonCp437NfoEncodingOverride:
            return "NonCp437NfoEncodingOverride";
        case SaveWarningCode::LossyConversionOccurred:
            return "LossyConversionOccurred";
        case SaveWarningCode::Utf8BomIncluded:
            return "Utf8BomIncluded";
        case SaveWarningCode::TerminalArtColorApproximationOccurred:
            return "TerminalArtColorApproximationOccurred";
        case SaveWarningCode::TerminalArtThemeColorApproximationOccurred:
            return "TerminalArtThemeColorApproximationOccurred";
        case SaveWarningCode::TerminalArtUnsupportedStyleDropped:
            return "TerminalArtUnsupportedStyleDropped";
        case SaveWarningCode::TerminalArtReverseApproximated:
            return "TerminalArtReverseApproximated";
        case SaveWarningCode::TerminalArtBoldApproximated:
            return "TerminalArtBoldApproximated";
        case SaveWarningCode::TerminalArtIceColorExportUsed:
            return "TerminalArtIceColorExportUsed";
        case SaveWarningCode::TerminalArtTrailingSpacesForced:
            return "TerminalArtTrailingSpacesForced";
        case SaveWarningCode::TerminalArtEncodingForcedToCp437:
            return "TerminalArtEncodingForcedToCp437";
        case SaveWarningCode::XpThemeColorResolvedToRgb:
            return "XpThemeColorResolvedToRgb";
        case SaveWarningCode::XpUnsupportedStyleDropped:
            return "XpUnsupportedStyleDropped";
        case SaveWarningCode::XpReverseApproximated:
            return "XpReverseApproximated";
        case SaveWarningCode::XpInvisibleApproximated:
            return "XpInvisibleApproximated";
        case SaveWarningCode::XpGlyphFallbackSubstituted:
            return "XpGlyphFallbackSubstituted";
        case SaveWarningCode::XpGlyphReplacementUsed:
            return "XpGlyphReplacementUsed";
        case SaveWarningCode::HighFidelityXpRecommended:
            return "HighFidelityXpRecommended";
        default:
            return "Unknown";
        }
    }
}