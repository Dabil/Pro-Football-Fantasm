#include "Rendering/Objects/XpArtExporter.h"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

#if defined(__has_include)
#   if __has_include(<zlib.h>)
#       include <zlib.h>
#       define TUI_XP_ART_EXPORTER_HAS_ZLIB 1
#   else
#       define TUI_XP_ART_EXPORTER_HAS_ZLIB 0
#   endif
#else
#   define TUI_XP_ART_EXPORTER_HAS_ZLIB 0
#endif

#include "Rendering/Capabilities/ColorSupport.h"
#include "Rendering/Objects/Cp437Encoding.h"
#include "Rendering/Styles/ColorMapping.h"
#include "Rendering/Styles/ColorResolver.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/ThemeColor.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    using namespace TextObjectExporter;

    XpArtExporter::XpColor makeColor(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
    {
        XpArtExporter::XpColor color;
        color.red = red;
        color.green = green;
        color.blue = blue;
        return color;
    }

    XpArtExporter::XpColor defaultForegroundColor()
    {
        return makeColor(255, 255, 255);
    }

    XpArtExporter::XpColor defaultBackgroundColor()
    {
        return makeColor(0, 0, 0);
    }

    std::string toHexCodePoint(char32_t codePoint)
    {
        std::ostringstream stream;
        stream << "U+"
            << std::uppercase
            << std::hex
            << std::setw(4)
            << std::setfill('0')
            << static_cast<std::uint32_t>(UnicodeConversion::sanitizeCodePoint(codePoint));
        return stream.str();
    }

    void addWarningOnce(
        SaveResult& result,
        SaveWarningCode code,
        const std::string& message)
    {
        for (const SaveWarning& warning : result.warnings)
        {
            if (warning.code == code)
            {
                return;
            }
        }

        SaveWarning warning;
        warning.code = code;
        warning.message = message;
        result.warnings.push_back(warning);
    }

    void addDetailedWarning(
        SaveResult& result,
        SaveWarningCode code,
        const std::string& message,
        const SourcePosition& position,
        char32_t sourceCodePoint,
        char32_t replacementCodePoint)
    {
        SaveWarning warning;
        warning.code = code;
        warning.message = message;
        warning.sourcePosition = position;
        warning.sourceCodePoint = sourceCodePoint;
        warning.replacementCodePoint = replacementCodePoint;
        result.warnings.push_back(warning);
    }

    std::size_t checkedCellCount(int width, int height, bool& ok)
    {
        ok = false;

        if (width <= 0 || height <= 0)
        {
            return 0;
        }

        const std::uint64_t count =
            static_cast<std::uint64_t>(width) *
            static_cast<std::uint64_t>(height);

        if (count > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        {
            return 0;
        }

        ok = true;
        return static_cast<std::size_t>(count);
    }

    int layerIndex(int x, int y, int height)
    {
        return (x * height) + y;
    }

    void appendLe32(std::string& bytes, std::uint32_t value)
    {
        bytes.push_back(static_cast<char>(value & 0xFF));
        bytes.push_back(static_cast<char>((value >> 8) & 0xFF));
        bytes.push_back(static_cast<char>((value >> 16) & 0xFF));
        bytes.push_back(static_cast<char>((value >> 24) & 0xFF));
    }

    XpArtExporter::XpColor colorToXpColor(const Color& color)
    {
        const ColorMapping::RgbValue rgb = ColorMapping::toRgb(color);
        return makeColor(rgb.red, rgb.green, rgb.blue);
    }

    bool resolveColorToXp(
        const std::optional<Style::StyleColorValue>& authored,
        const std::optional<Color>& concreteFallback,
        const std::optional<ThemeColor>& themeFallback,
        const XpArtExporter::XpColor& defaultColor,
        bool& outUsedTheme,
        XpArtExporter::XpColor& outColor)
    {
        outUsedTheme = false;

        if (authored.has_value())
        {
            if (authored->hasThemeColor())
            {
                outUsedTheme = true;
            }

            const Color resolved = ColorResolver::resolve(*authored, ColorSupport::Rgb24);
            if (resolved.isDefault())
            {
                outColor = defaultColor;
                return true;
            }

            outColor = colorToXpColor(resolved);
            return true;
        }

        if (concreteFallback.has_value())
        {
            const Color resolved = ColorMapping::mapToSupport(*concreteFallback, ColorSupport::Rgb24);
            if (resolved.isDefault())
            {
                outColor = defaultColor;
                return true;
            }

            outColor = colorToXpColor(resolved);
            return true;
        }

        if (themeFallback.has_value())
        {
            outUsedTheme = true;
            const Color resolved = ColorResolver::resolveThemeColor(*themeFallback, ColorSupport::Rgb24);
            if (resolved.isDefault())
            {
                outColor = defaultColor;
                return true;
            }

            outColor = colorToXpColor(resolved);
            return true;
        }

        outColor = defaultColor;
        return true;
    }

    struct CellStyleResolution
    {
        XpArtExporter::XpColor foreground = defaultForegroundColor();
        XpArtExporter::XpColor background = defaultBackgroundColor();

        bool usedThemeColor = false;
        bool approximatedReverse = false;
        bool approximatedInvisible = false;
        bool droppedUnsupportedStyle = false;
    };

    CellStyleResolution resolveCellStyle(const std::optional<Style>& style)
    {
        CellStyleResolution result;

        if (!style.has_value() || style->isEmpty())
        {
            return result;
        }

        bool usedForegroundTheme = false;
        bool usedBackgroundTheme = false;

        resolveColorToXp(
            style->foregroundColorValue(),
            style->foreground(),
            style->foregroundThemeColor(),
            defaultForegroundColor(),
            usedForegroundTheme,
            result.foreground);

        resolveColorToXp(
            style->backgroundColorValue(),
            style->background(),
            style->backgroundThemeColor(),
            defaultBackgroundColor(),
            usedBackgroundTheme,
            result.background);

        result.usedThemeColor = usedForegroundTheme || usedBackgroundTheme;

        if (style->reverse())
        {
            std::swap(result.foreground, result.background);
            result.approximatedReverse = true;
        }

        if (style->invisible())
        {
            result.foreground = result.background;
            result.approximatedInvisible = true;
        }

        if (style->bold() ||
            style->dim() ||
            style->underline() ||
            style->slowBlink() ||
            style->fastBlink() ||
            style->strike())
        {
            result.droppedUnsupportedStyle = true;
        }

        return result;
    }

    struct GlyphConversionStats
    {
        std::size_t fallbackCount = 0;
        std::size_t replacementCount = 0;

        bool hasFallbackExample = false;
        SourcePosition fallbackPosition;
        char32_t fallbackSourceCodePoint = U'\0';
        char32_t fallbackReplacementCodePoint = U'\0';
        std::string fallbackReason;

        bool hasReplacementExample = false;
        SourcePosition replacementPosition;
        char32_t replacementSourceCodePoint = U'\0';
        char32_t replacementGlyph = U'\0';
    };

    struct WarningOccurrence
    {
        std::size_t count = 0;
        bool hasExample = false;
        SourcePosition position;
    };

    struct StyleConversionStats
    {
        WarningOccurrence themeColorResolved;
        WarningOccurrence reverseApproximated;
        WarningOccurrence invisibleApproximated;
        WarningOccurrence unsupportedStyleDropped;
    };

    void recordOccurrence(WarningOccurrence& occurrence, int x, int y)
    {
        ++occurrence.count;
        if (!occurrence.hasExample)
        {
            occurrence.hasExample = true;
            occurrence.position = { x, y };
        }
    }

    bool convertGlyphToXpCell(
        const TextObjectCell& sourceCell,
        int x,
        int y,
        const SaveOptions& options,
        SaveResult& ioResult,
        GlyphConversionStats& ioStats,
        XpArtExporter::XpCell& outCell)
    {
        char32_t glyph = U' ';
        if (sourceCell.kind == CellKind::Glyph)
        {
            glyph = sourceCell.glyph;
        }

        Cp437Encoding::EncodeResult encodedGlyph;
        if (!Cp437Encoding::tryEncodeWithFallback(glyph, options.xpReplacementGlyph, encodedGlyph))
        {
            ioResult.hasEncodingFailure = true;
            ioResult.firstFailingCodePoint = glyph;
            ioResult.firstFailingPosition = { x, y };
            ioResult.errorMessage =
                "XP export fallback configuration is invalid because SaveOptions::xpReplacementGlyph is not representable in CP437.";
            return false;
        }

        outCell.glyph = static_cast<std::uint32_t>(encodedGlyph.byte);

        if (encodedGlyph.kind == Cp437Encoding::ConversionKind::Exact)
        {
            return true;
        }

        ioResult.hadLossyConversion = true;
        ++ioResult.lossyCodePointCount;

        if (!hasWarning(ioResult, SaveWarningCode::LossyConversionOccurred))
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::LossyConversionOccurred,
                "XP export performed one or more lossy glyph conversions while mapping Unicode glyphs to CP437 bytes.");
        }

        if (encodedGlyph.kind == Cp437Encoding::ConversionKind::FallbackApproximation)
        {
            ++ioStats.fallbackCount;
            if (!ioStats.hasFallbackExample)
            {
                ioStats.hasFallbackExample = true;
                ioStats.fallbackPosition = { x, y };
                ioStats.fallbackSourceCodePoint = glyph;
                ioStats.fallbackReplacementCodePoint = encodedGlyph.outputCodePoint;
                ioStats.fallbackReason = encodedGlyph.reason != nullptr ? encodedGlyph.reason : "fallback approximation";
            }
            return true;
        }

        ++ioStats.replacementCount;
        if (!ioStats.hasReplacementExample)
        {
            ioStats.hasReplacementExample = true;
            ioStats.replacementPosition = { x, y };
            ioStats.replacementSourceCodePoint = glyph;
            ioStats.replacementGlyph = encodedGlyph.outputCodePoint;
        }

        return true;
    }

    bool convertTextObjectCell(
        const TextObjectCell& sourceCell,
        int x,
        int y,
        const SaveOptions& options,
        SaveResult& ioResult,
        GlyphConversionStats& ioGlyphStats,
        StyleConversionStats& ioStyleStats,
        XpArtExporter::XpCell& outCell)
    {
        if (sourceCell.kind == CellKind::WideTrailing)
        {
            ioResult.hasEncodingFailure = true;
            ioResult.firstFailingCodePoint = sourceCell.glyph;
            ioResult.firstFailingPosition = { x, y };
            ioResult.errorMessage = "XP export cannot serialize WideTrailing cells directly. Flatten or normalize the TextObject before saving.";
            return false;
        }

        if (sourceCell.kind == CellKind::CombiningContinuation)
        {
            ioResult.hasEncodingFailure = true;
            ioResult.firstFailingCodePoint = sourceCell.glyph;
            ioResult.firstFailingPosition = { x, y };
            ioResult.errorMessage = "XP export cannot serialize CombiningContinuation cells directly. Flatten or normalize the TextObject before saving.";
            return false;
        }

        if (sourceCell.kind == CellKind::Glyph && sourceCell.width != CellWidth::One)
        {
            ioResult.hasEncodingFailure = true;
            ioResult.firstFailingCodePoint = sourceCell.glyph;
            ioResult.firstFailingPosition = { x, y };
            ioResult.errorMessage = "XP export currently requires every emitted glyph to occupy exactly one cell.";
            return false;
        }

        const CellStyleResolution styleInfo = resolveCellStyle(sourceCell.style);
        outCell.foreground = styleInfo.foreground;
        outCell.background = styleInfo.background;

        if (styleInfo.usedThemeColor)
        {
            recordOccurrence(ioStyleStats.themeColorResolved, x, y);
        }

        if (styleInfo.approximatedReverse)
        {
            recordOccurrence(ioStyleStats.reverseApproximated, x, y);
        }

        if (styleInfo.approximatedInvisible)
        {
            recordOccurrence(ioStyleStats.invisibleApproximated, x, y);
        }

        if (styleInfo.droppedUnsupportedStyle)
        {
            recordOccurrence(ioStyleStats.unsupportedStyleDropped, x, y);
        }

        return convertGlyphToXpCell(sourceCell, x, y, options, ioResult, ioGlyphStats, outCell);
    }

    void finalizeStyleWarnings(SaveResult& ioResult, const StyleConversionStats& stats)
    {
        if (stats.themeColorResolved.hasExample)
        {
            std::ostringstream message;
            message
                << stats.themeColorResolved.count
                << " cell(s) resolved authored theme colors to concrete RGB values for XP export. First example at ("
                << stats.themeColorResolved.position.x
                << ", "
                << stats.themeColorResolved.position.y
                << ").";

            addDetailedWarning(
                ioResult,
                SaveWarningCode::XpThemeColorResolvedToRgb,
                message.str(),
                stats.themeColorResolved.position,
                U'\0',
                U'\0');
        }

        if (stats.reverseApproximated.hasExample)
        {
            std::ostringstream message;
            message
                << stats.reverseApproximated.count
                << " cell(s) approximated reverse styling by swapping foreground and background colors during XP export. First example at ("
                << stats.reverseApproximated.position.x
                << ", "
                << stats.reverseApproximated.position.y
                << ").";

            addDetailedWarning(
                ioResult,
                SaveWarningCode::XpReverseApproximated,
                message.str(),
                stats.reverseApproximated.position,
                U'\0',
                U'\0');
        }

        if (stats.invisibleApproximated.hasExample)
        {
            std::ostringstream message;
            message
                << stats.invisibleApproximated.count
                << " cell(s) approximated invisible styling by matching foreground to background during XP export. First example at ("
                << stats.invisibleApproximated.position.x
                << ", "
                << stats.invisibleApproximated.position.y
                << ").";

            addDetailedWarning(
                ioResult,
                SaveWarningCode::XpInvisibleApproximated,
                message.str(),
                stats.invisibleApproximated.position,
                U'\0',
                U'\0');
        }

        if (stats.unsupportedStyleDropped.hasExample)
        {
            std::ostringstream message;
            message
                << stats.unsupportedStyleDropped.count
                << " cell(s) dropped unsupported TextObject style attributes during XP export. First example at ("
                << stats.unsupportedStyleDropped.position.x
                << ", "
                << stats.unsupportedStyleDropped.position.y
                << ").";

            addDetailedWarning(
                ioResult,
                SaveWarningCode::XpUnsupportedStyleDropped,
                message.str(),
                stats.unsupportedStyleDropped.position,
                U'\0',
                U'\0');
        }
    }

    void finalizeGlyphWarnings(SaveResult& ioResult, const GlyphConversionStats& stats)
    {
        if (stats.hasFallbackExample)
        {
            std::ostringstream message;
            message
                << stats.fallbackCount
                << " glyph(s) were approximated to nearby CP437 equivalents during XP export. First example: "
                << toHexCodePoint(stats.fallbackSourceCodePoint)
                << " -> "
                << toHexCodePoint(stats.fallbackReplacementCodePoint)
                << " at ("
                << stats.fallbackPosition.x
                << ", "
                << stats.fallbackPosition.y
                << ") because "
                << stats.fallbackReason
                << ".";

            addDetailedWarning(
                ioResult,
                SaveWarningCode::XpGlyphFallbackSubstituted,
                message.str(),
                stats.fallbackPosition,
                stats.fallbackSourceCodePoint,
                stats.fallbackReplacementCodePoint);
        }

        if (stats.hasReplacementExample)
        {
            std::ostringstream message;
            message
                << stats.replacementCount
                << " glyph(s) were replaced with the configured XP replacement glyph "
                << toHexCodePoint(stats.replacementGlyph)
                << ". First example: "
                << toHexCodePoint(stats.replacementSourceCodePoint)
                << " -> "
                << toHexCodePoint(stats.replacementGlyph)
                << " at ("
                << stats.replacementPosition.x
                << ", "
                << stats.replacementPosition.y
                << ").";

            addDetailedWarning(
                ioResult,
                SaveWarningCode::XpGlyphReplacementUsed,
                message.str(),
                stats.replacementPosition,
                stats.replacementSourceCodePoint,
                stats.replacementGlyph);
        }
    }

#if TUI_XP_ART_EXPORTER_HAS_ZLIB
    bool gzipCompress(std::string_view payloadBytes, std::string& outCompressedBytes)
    {
        outCompressedBytes.clear();

        z_stream stream{};
        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(payloadBytes.data()));
        stream.avail_in = static_cast<uInt>(payloadBytes.size());

        const int windowBits = 15 + 16;
        const int memLevel = 8;
        if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, windowBits, memLevel, Z_DEFAULT_STRATEGY) != Z_OK)
        {
            return false;
        }

        constexpr std::size_t kChunkSize = 16384;
        std::vector<char> chunk(kChunkSize);

        int rc = Z_OK;
        while (rc == Z_OK)
        {
            stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
            stream.avail_out = static_cast<uInt>(chunk.size());

            rc = deflate(&stream, Z_FINISH);
            if (rc != Z_OK && rc != Z_STREAM_END)
            {
                deflateEnd(&stream);
                outCompressedBytes.clear();
                return false;
            }

            const std::size_t produced = chunk.size() - static_cast<std::size_t>(stream.avail_out);
            outCompressedBytes.append(chunk.data(), produced);
        }

        deflateEnd(&stream);
        return rc == Z_STREAM_END;
    }
#endif
}

namespace XpArtExporter
{
    bool XpLayer::isValid() const
    {
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        const std::uint64_t expected =
            static_cast<std::uint64_t>(width) *
            static_cast<std::uint64_t>(height);

        return cells.size() == static_cast<std::size_t>(expected);
    }

    bool XpDocument::isValid() const
    {
        if (canvasWidth <= 0 || canvasHeight <= 0 || layers.empty())
        {
            return false;
        }

        for (const XpLayer& layer : layers)
        {
            if (!layer.isValid())
            {
                return false;
            }
        }

        return true;
    }

    bool buildDocument(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options,
        TextObjectExporter::SaveResult& ioResult,
        XpDocument& outDocument)
    {
        outDocument = XpDocument{};
        outDocument.formatVersion = 1;
        outDocument.canvasWidth = object.getWidth();
        outDocument.canvasHeight = object.getHeight();

        if (!object.isLoaded())
        {
            ioResult.errorMessage = "Cannot export an unloaded TextObject to XP.";
            return false;
        }

        if (object.getWidth() <= 0 || object.getHeight() <= 0)
        {
            ioResult.errorMessage = "XP export requires a non-empty TextObject canvas.";
            return false;
        }

        bool countOk = false;
        const std::size_t cellCount = checkedCellCount(object.getWidth(), object.getHeight(), countOk);
        if (!countOk)
        {
            ioResult.errorMessage = "XP export cell count overflow.";
            return false;
        }

        XpLayer layer;
        layer.width = object.getWidth();
        layer.height = object.getHeight();
        layer.cells.resize(cellCount);

        GlyphConversionStats glyphStats;
        StyleConversionStats styleStats;

        for (int y = 0; y < object.getHeight(); ++y)
        {
            for (int x = 0; x < object.getWidth(); ++x)
            {
                const TextObjectCell& sourceCell = object.getCell(x, y);
                XpCell convertedCell;
                if (!convertTextObjectCell(sourceCell, x, y, options, ioResult, glyphStats, styleStats, convertedCell))
                {
                    return false;
                }

                const std::size_t index = static_cast<std::size_t>(layerIndex(x, y, layer.height));
                layer.cells[index] = convertedCell;
            }
        }

        finalizeGlyphWarnings(ioResult, glyphStats);
        finalizeStyleWarnings(ioResult, styleStats);

        outDocument.layers.clear();
        outDocument.layers.push_back(std::move(layer));
        return outDocument.isValid();
    }

    bool serializeDocument(
        const XpDocument& document,
        TextObjectExporter::SaveResult& ioResult,
        std::string& outPayloadBytes)
    {
        outPayloadBytes.clear();

        if (!document.isValid())
        {
            ioResult.errorMessage = "XP export document is invalid before serialization.";
            return false;
        }

        std::size_t totalCells = 0;
        for (const XpLayer& layer : document.layers)
        {
            totalCells += layer.cells.size();
        }

        const std::size_t headerBytes = 8;
        const std::size_t layerHeaderBytes = document.layers.size() * 8;
        const std::size_t cellBytes = totalCells * 10;
        outPayloadBytes.reserve(headerBytes + layerHeaderBytes + cellBytes);

        const std::int32_t versionHeader = -document.formatVersion;
        appendLe32(outPayloadBytes, static_cast<std::uint32_t>(versionHeader));
        appendLe32(outPayloadBytes, static_cast<std::uint32_t>(document.layers.size()));

        for (const XpLayer& layer : document.layers)
        {
            appendLe32(outPayloadBytes, static_cast<std::uint32_t>(layer.width));
            appendLe32(outPayloadBytes, static_cast<std::uint32_t>(layer.height));

            for (const XpCell& cell : layer.cells)
            {
                appendLe32(outPayloadBytes, cell.glyph);
                outPayloadBytes.push_back(static_cast<char>(cell.foreground.red));
                outPayloadBytes.push_back(static_cast<char>(cell.foreground.green));
                outPayloadBytes.push_back(static_cast<char>(cell.foreground.blue));
                outPayloadBytes.push_back(static_cast<char>(cell.background.red));
                outPayloadBytes.push_back(static_cast<char>(cell.background.green));
                outPayloadBytes.push_back(static_cast<char>(cell.background.blue));
            }
        }

        return true;
    }

    bool compressSerializedDocument(
        std::string_view payloadBytes,
        TextObjectExporter::SaveResult& ioResult,
        std::string& outCompressedBytes)
    {
        outCompressedBytes.clear();

        if (payloadBytes.empty())
        {
            ioResult.errorMessage = "XP export cannot compress an empty payload.";
            return false;
        }

#if TUI_XP_ART_EXPORTER_HAS_ZLIB
        if (!gzipCompress(payloadBytes, outCompressedBytes))
        {
            ioResult.errorMessage = "XP export failed while compressing the serialized payload with zlib/gzip.";
            return false;
        }

        if (outCompressedBytes.empty())
        {
            ioResult.errorMessage = "XP export compression completed but produced no output bytes.";
            return false;
        }

        return true;
#else
        ioResult.errorMessage =
            "XP export requires zlib support for gzip-compatible XP compression, but zlib was not available in this build.";
        return false;
#endif
    }

    bool exportToBytes(
        const TextObject& object,
        const TextObjectExporter::SaveOptions& options,
        TextObjectExporter::SaveResult& ioResult)
    {
        XpDocument document;
        if (!buildDocument(object, options, ioResult, document))
        {
            return false;
        }

        std::string payloadBytes;
        if (!serializeDocument(document, ioResult, payloadBytes))
        {
            return false;
        }

        if (!compressSerializedDocument(payloadBytes, ioResult, ioResult.bytes))
        {
            return false;
        }

        ioResult.success = true;
        return true;
    }
}
