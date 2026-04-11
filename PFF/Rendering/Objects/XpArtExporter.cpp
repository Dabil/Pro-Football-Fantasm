#include "Rendering/Objects/XpArtExporter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
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

    constexpr char kSequenceMagic[8] = { 'T', 'U', 'I', 'X', 'P', 'S', 'Q', '1' };
    constexpr std::uint32_t kSequenceFormatVersion = 1u;
    constexpr std::uint32_t kFrameFlagHasLabel = 0x01u;
    constexpr std::uint32_t kFrameFlagHasHiddenLayers = 0x02u;
    constexpr int kXpSeqManifestVersion = 1;

    struct SequenceFramePlan
    {
        const XpArtLoader::XpFrame* frame = nullptr;
        std::filesystem::path resolvedFramePath;
        std::string manifestSourcePath;
        bool linksExistingSource = false;
    };

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
            if (warning.code == code && warning.message == message)
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

    bool areWarningsEquivalent(
        const SaveWarning& lhs,
        const SaveWarning& rhs)
    {
        return lhs.code == rhs.code &&
            lhs.message == rhs.message &&
            lhs.sourcePosition.x == rhs.sourcePosition.x &&
            lhs.sourcePosition.y == rhs.sourcePosition.y &&
            lhs.sourceCodePoint == rhs.sourceCodePoint &&
            lhs.replacementCodePoint == rhs.replacementCodePoint;
    }

    void addWarningIfMissing(
        SaveResult& result,
        const SaveWarning& warning)
    {
        for (const SaveWarning& existing : result.warnings)
        {
            if (areWarningsEquivalent(existing, warning))
            {
                return;
            }
        }

        result.warnings.push_back(warning);
    }

    void appendSaveResultWarnings(
        SaveResult& topLevelResult,
        const SaveResult& nestedResult,
        const std::string& warningContextPrefix)
    {
        if (nestedResult.warnings.empty())
        {
            return;
        }

        for (const SaveWarning& nestedWarning : nestedResult.warnings)
        {
            SaveWarning mergedWarning = nestedWarning;
            if (!warningContextPrefix.empty())
            {
                mergedWarning.message = warningContextPrefix + nestedWarning.message;
            }

            addWarningIfMissing(topLevelResult, mergedWarning);
        }
    }

    void appendFrameWarningsToTopLevel(
        SaveResult& topLevelResult,
        const SaveResult& frameResult,
        const std::string& frameOutputPath)
    {
        if (frameResult.hadLossyConversion)
        {
            topLevelResult.hadLossyConversion = true;
            topLevelResult.lossyCodePointCount += frameResult.lossyCodePointCount;
        }

        if (!topLevelResult.hasEncodingFailure && frameResult.hasEncodingFailure)
        {
            topLevelResult.hasEncodingFailure = true;
            topLevelResult.firstFailingCodePoint = frameResult.firstFailingCodePoint;
            topLevelResult.firstFailingPosition = frameResult.firstFailingPosition;
        }

        const std::string warningPrefix = frameOutputPath.empty()
            ? std::string()
            : std::string("Frame export warning [") + frameOutputPath + "]: ";
        appendSaveResultWarnings(topLevelResult, frameResult, warningPrefix);
    }

    void mergeManifestSaveResultIntoTopLevel(
        SaveResult& topLevelResult,
        const SaveResult& manifestSaveResult)
    {
        topLevelResult.success = manifestSaveResult.success;
        topLevelResult.outputPath = manifestSaveResult.outputPath;
        topLevelResult.resolvedFileType = manifestSaveResult.resolvedFileType;
        topLevelResult.resolvedEncoding = manifestSaveResult.resolvedEncoding;
        topLevelResult.resolvedLineEnding = manifestSaveResult.resolvedLineEnding;
        topLevelResult.usedUtf8Bom = manifestSaveResult.usedUtf8Bom;
        topLevelResult.bytes = manifestSaveResult.bytes;

        if (manifestSaveResult.hadLossyConversion)
        {
            topLevelResult.hadLossyConversion = true;
            topLevelResult.lossyCodePointCount += manifestSaveResult.lossyCodePointCount;
        }

        if (!topLevelResult.hasEncodingFailure && manifestSaveResult.hasEncodingFailure)
        {
            topLevelResult.hasEncodingFailure = true;
            topLevelResult.firstFailingCodePoint = manifestSaveResult.firstFailingCodePoint;
            topLevelResult.firstFailingPosition = manifestSaveResult.firstFailingPosition;
        }

        if (!manifestSaveResult.errorMessage.empty())
        {
            topLevelResult.errorMessage = manifestSaveResult.errorMessage;
        }

        appendSaveResultWarnings(topLevelResult, manifestSaveResult, std::string());
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

    void appendBytes(std::string& bytes, const char* data, std::size_t size)
    {
        bytes.append(data, size);
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

    bool convertRetainedLayerCell(
        const XpArtLoader::XpLayerCell& sourceCell,
        XpArtExporter::XpCell& outCell)
    {
        outCell.glyph = sourceCell.glyph;
        outCell.foreground = makeColor(
            sourceCell.foreground.red,
            sourceCell.foreground.green,
            sourceCell.foreground.blue);
        outCell.background = makeColor(
            sourceCell.background.red,
            sourceCell.background.green,
            sourceCell.background.blue);
        return true;
    }

    bool buildLayerFromRetainedLayer(
        const XpArtLoader::XpLayer& sourceLayer,
        XpArtExporter::XpLayer& outLayer)
    {
        if (!sourceLayer.isValid())
        {
            return false;
        }

        bool countOk = false;
        const std::size_t cellCount = checkedCellCount(sourceLayer.width, sourceLayer.height, countOk);
        if (!countOk)
        {
            return false;
        }

        outLayer = XpArtExporter::XpLayer{};
        outLayer.width = sourceLayer.width;
        outLayer.height = sourceLayer.height;
        outLayer.cells.resize(cellCount);

        for (int y = 0; y < sourceLayer.height; ++y)
        {
            for (int x = 0; x < sourceLayer.width; ++x)
            {
                const XpArtLoader::XpLayerCell* cell = sourceLayer.tryGetCell(x, y);
                if (cell == nullptr)
                {
                    return false;
                }

                XpArtExporter::XpCell converted;
                if (!convertRetainedLayerCell(*cell, converted))
                {
                    return false;
                }

                outLayer.cells[static_cast<std::size_t>(layerIndex(x, y, outLayer.height))] = converted;
            }
        }

        return outLayer.isValid();
    }

    bool sequenceHasAnyFrameLabels(const XpArtLoader::XpSequence& sequence)
    {
        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            if (!frame.label.empty())
            {
                return true;
            }
        }

        return false;
    }

    bool documentHasHiddenLayers(const XpArtLoader::XpDocument& document)
    {
        for (const XpArtLoader::XpLayer& layer : document.layers)
        {
            if (!layer.visible)
            {
                return true;
            }
        }

        return false;
    }

    int countVisibleLayers(const XpArtLoader::XpDocument& document)
    {
        int count = 0;
        for (const XpArtLoader::XpLayer& layer : document.layers)
        {
            if (layer.visible)
            {
                ++count;
            }
        }
        return count;
    }

    std::string buildFrameFilePath(
        const std::string& baseFilePath,
        const XpArtExporter::RetainedExportOptions& options,
        int frameIndex)
    {
        const std::filesystem::path basePath(baseFilePath);
        const std::filesystem::path parentPath = basePath.has_parent_path()
            ? basePath.parent_path()
            : std::filesystem::path();
        const std::string stem = basePath.stem().string();

        std::ostringstream fileName;
        fileName
            << stem
            << options.frameFileSeparator
            << std::setw(std::max(1, options.frameNumberWidth))
            << std::setfill('0')
            << frameIndex
            << ".xp";

        return (parentPath / fileName.str()).lexically_normal().string();
    }

    bool hasXpSeqExtension(const std::string& filePath)
    {
        return std::filesystem::path(filePath).extension() == ".xpseq";
    }

    std::string quoteManifestValue(const std::string& value)
    {
        bool needsQuotes = value.empty();
        for (char ch : value)
        {
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '"')
            {
                needsQuotes = true;
                break;
            }
        }

        if (!needsQuotes)
        {
            return value;
        }

        std::string sanitized = value;
        std::replace(sanitized.begin(), sanitized.end(), '"', '\'');
        return std::string("\"") + sanitized + "\"";
    }

    std::string makeManifestSourcePath(
        const std::filesystem::path& manifestPath,
        const std::filesystem::path& framePath)
    {
        std::error_code error;
        const std::filesystem::path relative =
            std::filesystem::relative(framePath, manifestPath.parent_path(), error);

        if (!error && !relative.empty())
        {
            return relative.generic_string();
        }

        return framePath.lexically_normal().generic_string();
    }

    void appendIntegerList(std::ostringstream& stream, const std::vector<int>& values)
    {
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                stream << ',';
            }

            stream << values[index];
        }
    }

    bool canReuseExistingFrameSourcePath(
        const XpArtLoader::XpFrame& frame,
        const XpArtExporter::RetainedExportOptions& options,
        const std::filesystem::path& manifestPath,
        std::filesystem::path& outFramePath,
        std::string& outManifestSourcePath)
    {
        outFramePath.clear();
        outManifestSourcePath.clear();

        if (!options.preferLinkedFrameSourcePaths || !frame.hasSourcePath())
        {
            return false;
        }

        const std::filesystem::path sourcePath(frame.sourcePath);
        if (sourcePath.extension() != ".xp")
        {
            return false;
        }

        std::error_code ec;
        std::filesystem::path resolved = sourcePath;
        if (resolved.is_relative())
        {
            const std::filesystem::path manifestParent =
                manifestPath.has_parent_path()
                ? manifestPath.parent_path()
                : std::filesystem::current_path();
            resolved = (manifestParent / resolved).lexically_normal();
        }
        else
        {
            resolved = resolved.lexically_normal();
        }

        if (!std::filesystem::exists(resolved, ec) || ec)
        {
            return false;
        }

        const std::string manifestRelativePath = makeManifestSourcePath(manifestPath, resolved);
        const bool isAbsolutePath = std::filesystem::path(manifestRelativePath).is_absolute();
        if (isAbsolutePath && !options.allowAbsoluteFrameSourcePaths)
        {
            return false;
        }

        outFramePath = resolved;
        outManifestSourcePath = manifestRelativePath;
        return true;
    }

    bool buildSequenceFramePlans(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestFilePath,
        const XpArtExporter::RetainedExportOptions& options,
        SaveResult& ioResult,
        std::vector<SequenceFramePlan>& outPlans)
    {
        outPlans.clear();

        const std::filesystem::path manifestPath =
            std::filesystem::path(manifestFilePath).lexically_normal();

        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            if (!frame.isValid())
            {
                ioResult.errorMessage = "Retained XP sequence contains an invalid frame.";
                return false;
            }

            SequenceFramePlan plan;
            plan.frame = &frame;

            if (canReuseExistingFrameSourcePath(
                frame,
                options,
                manifestPath,
                plan.resolvedFramePath,
                plan.manifestSourcePath))
            {
                plan.linksExistingSource = true;
            }
            else
            {
                if (options.preferLinkedFrameSourcePaths &&
                    frame.hasSourcePath() &&
                    !options.rewriteMissingLinkedFrames)
                {
                    ioResult.errorMessage =
                        "Sequence frame source path could not be linked as an existing .xp asset and rewriteMissingLinkedFrames is disabled.";
                    return false;
                }

                plan.resolvedFramePath = buildFrameFilePath(manifestPath.string(), options, frame.frameIndex);
                plan.manifestSourcePath = makeManifestSourcePath(manifestPath, plan.resolvedFramePath);
                plan.linksExistingSource = false;
            }

            outPlans.push_back(std::move(plan));
        }

        return true;
    }

    bool containsParentDirectoryEscape(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path normalized = relativePath.lexically_normal();
        return !normalized.empty() &&
            !normalized.is_absolute() &&
            *normalized.begin() == "..";
    }

    bool hasDuplicateValues(const std::vector<int>& values)
    {
        std::vector<int> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
    }

    void appendManifestLevelWarnings(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestFilePath,
        const XpArtExporter::RetainedExportOptions& options,
        const std::vector<SequenceFramePlan>& framePlans,
        SaveResult& ioResult)
    {
        bool linkedExistingFrames = false;
        bool wroteNewFrameFiles = false;
        bool wroteAbsoluteFrameSources = false;
        bool wroteEscapingRelativeFrameSources = false;

        const std::filesystem::path manifestPath =
            std::filesystem::path(manifestFilePath).lexically_normal();

        if (!sequence.areFramesStoredInFrameIndexOrder() ||
            !sequence.hasContiguousFrameIndicesStartingAtZero())
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::XpSequenceSuspiciousFrameIndexing,
                "Manifest-first .xpseq export is writing frames whose stored order or frame indices are non-contiguous/surprising. Reload remains deterministic, but callers should verify intended playback order.");
        }

        if (!sequence.metadata.name.empty() && !sequence.metadata.sequenceLabel.empty())
        {
            if (sequence.metadata.name == sequence.metadata.sequenceLabel)
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceRedundantMetadata,
                    "Manifest-first .xpseq export found both sequence name and sequenceLabel with the same value. The manifest will preserve a single name field.");
            }
            else
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceConflictingMetadata,
                    "Manifest-first .xpseq export found both sequence name and sequenceLabel with different values. The manifest writes the name field first, which may hide the older label value on reload.");
            }
        }

        if (!sequence.metadata.defaultExplicitVisibleLayerIndices.empty())
        {
            if (sequence.metadata.defaultVisibleLayerMode != XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList)
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceRedundantMetadata,
                    "Manifest-first .xpseq export found default_explicit_visible_layers metadata without default_visible_layers=UseExplicitVisibleLayerList. The explicit layer list is likely redundant or misleading.");
            }

            if (hasDuplicateValues(sequence.metadata.defaultExplicitVisibleLayerIndices))
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceConflictingMetadata,
                    "Manifest-first .xpseq export found duplicate sequence-level explicit visible-layer indices. The manifest preserves them, but callers should verify the intended layer list.");
            }
        }

        for (std::size_t index = 0; index < framePlans.size(); ++index)
        {
            const SequenceFramePlan& framePlan = framePlans[index];
            const XpArtLoader::XpFrame& frame = *framePlan.frame;

            const std::filesystem::path generatedPath =
                std::filesystem::path(
                    buildFrameFilePath(manifestPath.string(), options, frame.frameIndex)).lexically_normal();
            if (framePlan.resolvedFramePath.lexically_normal() == generatedPath)
            {
                wroteNewFrameFiles = true;
            }
            else
            {
                linkedExistingFrames = true;
            }

            const std::filesystem::path manifestSourcePath(framePlan.manifestSourcePath);
            if (manifestSourcePath.is_absolute())
            {
                wroteAbsoluteFrameSources = true;
            }
            else if (containsParentDirectoryEscape(manifestSourcePath))
            {
                wroteEscapingRelativeFrameSources = true;
            }

            if (frame.overrides.durationMilliseconds.has_value() &&
                sequence.metadata.defaultFrameDurationMilliseconds.has_value() &&
                frame.overrides.durationMilliseconds == sequence.metadata.defaultFrameDurationMilliseconds)
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceRedundantMetadata,
                    "Manifest-first .xpseq export found one or more frame duration overrides that exactly match the sequence default duration.");
            }

            if (frame.overrides.compositeMode.has_value() &&
                sequence.metadata.defaultCompositeMode.has_value() &&
                frame.overrides.compositeMode == sequence.metadata.defaultCompositeMode)
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceRedundantMetadata,
                    "Manifest-first .xpseq export found one or more frame composite overrides that exactly match the sequence default composite mode.");
            }

            if (frame.overrides.visibleLayerMode.has_value() &&
                sequence.metadata.defaultVisibleLayerMode.has_value() &&
                frame.overrides.visibleLayerMode == sequence.metadata.defaultVisibleLayerMode)
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::XpSequenceRedundantMetadata,
                    "Manifest-first .xpseq export found one or more frame visible-layer overrides that exactly match the sequence default visible-layer mode.");
            }

            if (!frame.overrides.explicitVisibleLayerIndices.empty())
            {
                if (frame.overrides.visibleLayerMode != XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList)
                {
                    addWarningOnce(
                        ioResult,
                        SaveWarningCode::XpSequenceRedundantMetadata,
                        "Manifest-first .xpseq export found one or more frame explicit_visible_layers lists without visible_layers=UseExplicitVisibleLayerList. The explicit list is likely redundant or misleading.");
                }

                if (sequence.metadata.defaultVisibleLayerMode == XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList &&
                    frame.overrides.visibleLayerMode == XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList &&
                    frame.overrides.explicitVisibleLayerIndices == sequence.metadata.defaultExplicitVisibleLayerIndices)
                {
                    addWarningOnce(
                        ioResult,
                        SaveWarningCode::XpSequenceRedundantMetadata,
                        "Manifest-first .xpseq export found one or more frame explicit visible-layer lists that exactly match the sequence default explicit list.");
                }

                if (hasDuplicateValues(frame.overrides.explicitVisibleLayerIndices))
                {
                    addWarningOnce(
                        ioResult,
                        SaveWarningCode::XpSequenceConflictingMetadata,
                        "Manifest-first .xpseq export found duplicate frame explicit visible-layer indices. The manifest preserves them, but callers should verify the intended frame override.");
                }
            }
        }

        if (wroteAbsoluteFrameSources)
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::XpSequenceSourcePathPortabilityRisk,
                "Manifest-first .xpseq export wrote one or more absolute frame source paths. The manifest may not be portable across machines or folders.");
        }

        if (wroteEscapingRelativeFrameSources)
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::XpSequenceSourcePathPortabilityRisk,
                "Manifest-first .xpseq export wrote one or more relative frame source paths that escape the manifest directory using '..'.");
        }

        if (wroteNewFrameFiles && linkedExistingFrames)
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::XpSequenceMixedLinkedAndGeneratedFrames,
                "Manifest-first .xpseq export used a mixed strategy: some frames were linked to existing .xp assets while others were emitted as new external .xp files.");
        }
    }

    void appendManifestDefaults(
        std::ostringstream& stream,
        const XpArtLoader::XpSequenceMetadata& metadata)
    {
        const std::string sequenceName = !metadata.name.empty()
            ? metadata.name
            : metadata.sequenceLabel;
        if (!sequenceName.empty())
        {
            stream << "name=" << quoteManifestValue(sequenceName) << "\n";
        }

        if (metadata.loop.has_value())
        {
            stream << "loop=" << (*metadata.loop ? "true" : "false") << "\n";
        }

        if (metadata.defaultFrameDurationMilliseconds.has_value())
        {
            stream << "default_frame_duration_ms="
                << *metadata.defaultFrameDurationMilliseconds
                << "\n";
        }

        if (metadata.defaultFramesPerSecond.has_value())
        {
            stream << "default_fps="
                << *metadata.defaultFramesPerSecond
                << "\n";
        }

        if (metadata.defaultCompositeMode.has_value())
        {
            stream << "default_composite="
                << XpArtLoader::toString(*metadata.defaultCompositeMode)
                << "\n";
        }

        if (metadata.defaultVisibleLayerMode.has_value())
        {
            stream << "default_visible_layers="
                << XpArtLoader::toString(*metadata.defaultVisibleLayerMode)
                << "\n";
        }

        if (!metadata.defaultExplicitVisibleLayerIndices.empty())
        {
            stream << "default_explicit_visible_layers=";
            appendIntegerList(stream, metadata.defaultExplicitVisibleLayerIndices);
            stream << "\n";
        }
    }

    bool serializeSequenceManifest(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestFilePath,
        const XpArtExporter::RetainedExportOptions& options,
        SaveResult& ioResult,
        std::string& outBytes)
    {
        outBytes.clear();

        if (!sequence.isValid())
        {
            ioResult.errorMessage = "Cannot export an invalid retained XP sequence.";
            return false;
        }

        if (manifestFilePath.empty())
        {
            ioResult.errorMessage = "Manifest-first sequence export requires a concrete .xpseq file path.";
            return false;
        }

        if (!hasXpSeqExtension(manifestFilePath))
        {
            ioResult.errorMessage = "Manifest-first sequence export requires a .xpseq output path.";
            return false;
        }

        std::vector<SequenceFramePlan> framePlans;
        if (!buildSequenceFramePlans(sequence, manifestFilePath, options, ioResult, framePlans))
        {
            return false;
        }

        appendManifestLevelWarnings(
            sequence,
            manifestFilePath,
            options,
            framePlans,
            ioResult);

        std::ostringstream stream;
        stream << "xpseq " << kXpSeqManifestVersion << "\n";
        appendManifestDefaults(stream, sequence.metadata);

        for (const SequenceFramePlan& framePlan : framePlans)
        {
            const XpArtLoader::XpFrame& frame = *framePlan.frame;

            stream << "frame"
                << " index=" << frame.frameIndex
                << " source=" << quoteManifestValue(framePlan.manifestSourcePath);

            if (!frame.label.empty())
            {
                stream << " label=" << quoteManifestValue(frame.label);
            }

            if (frame.overrides.durationMilliseconds.has_value())
            {
                stream << " duration_ms=" << *frame.overrides.durationMilliseconds;
            }

            if (frame.overrides.compositeMode.has_value())
            {
                stream << " composite="
                    << XpArtLoader::toString(*frame.overrides.compositeMode);
            }

            if (frame.overrides.visibleLayerMode.has_value())
            {
                stream << " visible_layers="
                    << XpArtLoader::toString(*frame.overrides.visibleLayerMode);
            }

            if (!frame.overrides.explicitVisibleLayerIndices.empty())
            {
                stream << " explicit_visible_layers=";
                appendIntegerList(stream, frame.overrides.explicitVisibleLayerIndices);
            }

            stream << "\n";
        }

        outBytes = stream.str();
        ioResult.resolvedEncoding = Encoding::Utf8;
        ioResult.usedUtf8Bom = false;
        ioResult.success = true;
        return true;
    }

    bool validateRetainedDocumentForLayeredXp(
        const XpArtLoader::XpDocument& document,
        const XpArtExporter::RetainedExportOptions& options,
        SaveResult& ioResult)
    {
        if (!document.isValid())
        {
            ioResult.errorMessage = "Cannot export an invalid retained XP document.";
            return false;
        }

        if (document.layers.empty())
        {
            ioResult.errorMessage = "Cannot export a retained XP document with no layers.";
            return false;
        }

        if (documentHasHiddenLayers(document) &&
            options.includeHiddenLayers &&
            !options.allowHiddenLayerVisibilityLoss)
        {
            ioResult.errorMessage =
                "Layered .xp export cannot preserve retained hidden-layer visibility. Set allowHiddenLayerVisibilityLoss to true to export hidden layers as ordinary visible XP layers, or choose FlattenedXp / XpSequenceManifest instead.";
            return false;
        }

        return true;
    }

    bool flattenRetainedDocumentToTextObject(
        const XpArtLoader::XpDocument& document,
        const XpArtExporter::RetainedExportOptions& options,
        SaveResult& ioResult,
        TextObject& outObject)
    {
        XpArtLoader::LoadOptions loadOptions;
        loadOptions.flattenLayers = true;
        loadOptions.compositeMode = options.flattenCompositeMode;

        XpArtLoader::LoadResult flattened = XpArtLoader::buildTextObject(document, loadOptions);
        if (!flattened.success)
        {
            ioResult.errorMessage = flattened.errorMessage.empty()
                ? "Failed to flatten retained XP document before export."
                : flattened.errorMessage;
            return false;
        }

        outObject = flattened.object;
        return true;
    }

    bool serializeSequenceContainer(
        const XpArtLoader::XpSequence& sequence,
        const XpArtExporter::RetainedExportOptions& options,
        SaveResult& ioResult,
        std::string& outBytes)
    {
        outBytes.clear();

        if (!sequence.isValid())
        {
            ioResult.errorMessage = "Cannot export an invalid retained XP sequence.";
            return false;
        }

        appendBytes(outBytes, kSequenceMagic, sizeof(kSequenceMagic));
        appendLe32(outBytes, kSequenceFormatVersion);
        appendLe32(outBytes, static_cast<std::uint32_t>(sequence.frames.size()));
        appendLe32(outBytes, static_cast<std::uint32_t>(options.flattenCompositeMode));

        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            if (!frame.isValid())
            {
                ioResult.errorMessage = "Retained XP sequence contains an invalid frame.";
                return false;
            }

            const XpArtLoader::XpDocument* frameDocument = frame.getDocument();
            if (frameDocument == nullptr)
            {
                ioResult.errorMessage = "Retained XP sequence contains a frame with no document.";
                return false;
            }

            XpArtExporter::RetainedExportOptions frameOptions = options;
            frameOptions.mode = XpArtExporter::RetainedExportMode::LayeredXp;
            frameOptions.includeHiddenLayers = true;
            frameOptions.allowHiddenLayerVisibilityLoss = true;

            SaveResult frameSaveResult;
            if (!XpArtExporter::exportToBytes(*frameDocument, frameOptions, frameSaveResult))
            {
                ioResult.errorMessage = frameSaveResult.errorMessage.empty()
                    ? "Failed to serialize an XP frame while building the sequence container."
                    : frameSaveResult.errorMessage;
                return false;
            }

            const std::string labelBytes = frame.label;
            const std::uint32_t flags =
                (labelBytes.empty() ? 0u : kFrameFlagHasLabel) |
                (documentHasHiddenLayers(*frameDocument) ? kFrameFlagHasHiddenLayers : 0u);

            appendLe32(outBytes, static_cast<std::uint32_t>(frame.frameIndex));
            appendLe32(outBytes, static_cast<std::uint32_t>(frameDocument->width));
            appendLe32(outBytes, static_cast<std::uint32_t>(frameDocument->height));
            appendLe32(outBytes, static_cast<std::uint32_t>(frameDocument->layers.size()));
            appendLe32(outBytes, static_cast<std::uint32_t>(labelBytes.size()));
            appendLe32(outBytes, static_cast<std::uint32_t>(frameSaveResult.bytes.size()));
            appendLe32(outBytes, flags);

            for (const XpArtLoader::XpLayer& layer : frameDocument->layers)
            {
                outBytes.push_back(static_cast<char>(layer.visible ? 1 : 0));
            }

            outBytes.append(labelBytes);
            outBytes.append(frameSaveResult.bytes);
        }

        return true;
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
        outDocument.formatVersion = XpArtLoader::XpRulesConfig::rexPaintDefaults().defaultFormatVersion;
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

    bool buildDocument(
        const XpArtLoader::XpDocument& document,
        const RetainedExportOptions& options,
        TextObjectExporter::SaveResult& ioResult,
        XpDocument& outDocument)
    {
        outDocument = XpDocument{};

        if (options.mode == RetainedExportMode::FlattenedXp)
        {
            TextObject flattened;
            if (!flattenRetainedDocumentToTextObject(document, options, ioResult, flattened))
            {
                return false;
            }

            if (document.layers.size() > 1)
            {
                addWarningOnce(
                    ioResult,
                    SaveWarningCode::LossyConversionOccurred,
                    "Flattened XP export collapsed retained layer structure into one visible XP layer by explicit request.");
            }

            return buildDocument(flattened, options.xpSaveOptions, ioResult, outDocument);
        }

        if (!validateRetainedDocumentForLayeredXp(document, options, ioResult))
        {
            return false;
        }

        outDocument.formatVersion = document.formatVersion > 0 ? document.formatVersion : XpArtLoader::XpRulesConfig::rexPaintDefaults().defaultFormatVersion;
        outDocument.canvasWidth = document.width;
        outDocument.canvasHeight = document.height;

        for (const XpArtLoader::XpLayer& sourceLayer : document.layers)
        {
            if (!sourceLayer.visible && !options.includeHiddenLayers)
            {
                continue;
            }

            XpLayer exportedLayer;
            if (!buildLayerFromRetainedLayer(sourceLayer, exportedLayer))
            {
                ioResult.errorMessage = "Retained XP document contains an invalid layer that could not be exported.";
                return false;
            }

            outDocument.layers.push_back(std::move(exportedLayer));
        }

        if (outDocument.layers.empty())
        {
            ioResult.errorMessage =
                options.includeHiddenLayers
                ? "Retained XP export produced no layers."
                : "Retained XP export excluded all layers because they were hidden.";
            return false;
        }

        if (!options.includeHiddenLayers && countVisibleLayers(document) != static_cast<int>(outDocument.layers.size()))
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::LossyConversionOccurred,
                "Layered XP export omitted one or more hidden retained layers by explicit request.");
        }

        if (documentHasHiddenLayers(document) && options.includeHiddenLayers)
        {
            addWarningOnce(
                ioResult,
                SaveWarningCode::LossyConversionOccurred,
                "Layered .xp export wrote hidden retained layers as ordinary XP layers because native .xp does not preserve visibility state.");
        }

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

    bool exportToBytes(
        const XpArtLoader::XpDocument& document,
        const RetainedExportOptions& options,
        TextObjectExporter::SaveResult& ioResult)
    {
        XpDocument exported;
        if (!buildDocument(document, options, ioResult, exported))
        {
            return false;
        }

        std::string payloadBytes;
        if (!serializeDocument(exported, ioResult, payloadBytes))
        {
            return false;
        }

        if (!compressSerializedDocument(payloadBytes, ioResult, ioResult.bytes))
        {
            return false;
        }

        ioResult.resolvedFileType = TextObjectExporter::FileType::Xp;
        ioResult.resolvedEncoding = TextObjectExporter::Encoding::Binary;
        ioResult.success = true;
        return true;
    }

    bool exportSequenceToBytes(
        const XpArtLoader::XpSequence& sequence,
        const RetainedExportOptions& options,
        TextObjectExporter::SaveResult& ioResult)
    {
        if (!sequence.isValid())
        {
            ioResult.errorMessage = "Cannot export an invalid retained XP sequence.";
            return false;
        }

        if (options.mode == RetainedExportMode::LayeredXp ||
            options.mode == RetainedExportMode::FlattenedXp)
        {
            if (sequence.getFrameCount() != 1)
            {
                ioResult.errorMessage =
                    "Single-document XP export cannot represent more than one frame. Use XpSequenceManifest or FramePerFile for multi-frame retained XP content.";
                return false;
            }

            if (sequenceHasAnyFrameLabels(sequence) && !options.allowFrameMetadataLoss)
            {
                ioResult.errorMessage =
                    "Single-document XP export would drop retained frame labels or sequence metadata. Set allowFrameMetadataLoss to true or use XpSequenceManifest instead.";
                return false;
            }

            const XpArtLoader::XpDocument* frameDocument = sequence.frames.front().getDocument();
            if (frameDocument == nullptr)
            {
                ioResult.errorMessage = "Retained XP sequence contains a frame with no document.";
                return false;
            }

            return exportToBytes(*frameDocument, options, ioResult);
        }

        if (options.mode == RetainedExportMode::FramePerFile)
        {
            ioResult.errorMessage = "FramePerFile export writes multiple files and must use saveSequenceToFrameFiles().";
            return false;
        }

        if (options.mode == RetainedExportMode::XpSequenceManifest)
        {
            ioResult.errorMessage =
                "Manifest-first .xpseq export writes a UTF-8 manifest plus external .xp frame files and must use saveSequenceToManifestFile() or saveSequenceToFile() with a .xpseq path.";
            return false;
        }

        if (options.mode != RetainedExportMode::ExperimentalBinarySequenceContainerInternal ||
            !options.allowExperimentalBinarySequenceContainer)
        {
            ioResult.errorMessage =
                "Binary sequence container export is deferred. The active source-sequence architecture is manifest-first .xpseq.";
            return false;
        }

        if (!serializeSequenceContainer(sequence, options, ioResult, ioResult.bytes))
        {
            return false;
        }

        ioResult.resolvedEncoding = TextObjectExporter::Encoding::Binary;
        ioResult.success = true;
        return true;
    }

    bool exportSequenceManifestToBytes(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestFilePath,
        const RetainedExportOptions& options,
        TextObjectExporter::SaveResult& ioResult)
    {
        if (options.mode != RetainedExportMode::XpSequenceManifest)
        {
            ioResult.errorMessage = "exportSequenceManifestToBytes() requires RetainedExportMode::XpSequenceManifest.";
            return false;
        }

        if (!serializeSequenceManifest(sequence, manifestFilePath, options, ioResult, ioResult.bytes))
        {
            return false;
        }

        ioResult.outputPath = manifestFilePath;
        return true;
    }

    bool saveToFile(
        const XpArtLoader::XpDocument& document,
        const std::string& filePath,
        const RetainedExportOptions& options,
        TextObjectExporter::SaveResult& outResult)
    {
        outResult = TextObjectExporter::SaveResult{};
        if (!exportToBytes(document, options, outResult))
        {
            outResult.outputPath = filePath;
            return false;
        }

        std::ofstream file(filePath, std::ios::binary);
        if (!file)
        {
            outResult.success = false;
            outResult.errorMessage = "Failed to open retained XP output file for writing.";
            outResult.outputPath = filePath;
            outResult.bytes.clear();
            return false;
        }

        file.write(outResult.bytes.data(), static_cast<std::streamsize>(outResult.bytes.size()));
        if (!file)
        {
            outResult.success = false;
            outResult.errorMessage = "Failed while writing retained XP output bytes.";
            outResult.outputPath = filePath;
            outResult.bytes.clear();
            return false;
        }

        outResult.outputPath = filePath;
        return true;
    }

    bool saveSequenceToFile(
        const XpArtLoader::XpSequence& sequence,
        const std::string& filePath,
        const RetainedExportOptions& options,
        TextObjectExporter::SaveResult& outResult)
    {
        outResult = TextObjectExporter::SaveResult{};

        if (options.mode == RetainedExportMode::XpSequenceManifest)
        {
            RetainedExportResult manifestResult;
            const bool success = saveSequenceToManifestFile(sequence, filePath, options, manifestResult);
            outResult = manifestResult.saveResult;
            outResult.outputPath = filePath;
            return success;
        }

        if (!exportSequenceToBytes(sequence, options, outResult))
        {
            outResult.outputPath = filePath;
            return false;
        }

        std::ofstream file(filePath, std::ios::binary);
        if (!file)
        {
            outResult.success = false;
            outResult.errorMessage = "Failed to open retained XP sequence output file for writing.";
            outResult.outputPath = filePath;
            outResult.bytes.clear();
            return false;
        }

        file.write(outResult.bytes.data(), static_cast<std::streamsize>(outResult.bytes.size()));
        if (!file)
        {
            outResult.success = false;
            outResult.errorMessage = "Failed while writing retained XP sequence output bytes.";
            outResult.outputPath = filePath;
            outResult.bytes.clear();
            return false;
        }

        outResult.outputPath = filePath;
        return true;
    }

    bool saveSequenceToManifestFile(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestFilePath,
        const RetainedExportOptions& options,
        RetainedExportResult& outResult)
    {
        outResult = RetainedExportResult{};

        if (options.mode != RetainedExportMode::XpSequenceManifest)
        {
            outResult.saveResult.errorMessage =
                "saveSequenceToManifestFile() requires RetainedExportMode::XpSequenceManifest.";
            outResult.saveResult.outputPath = manifestFilePath;
            return false;
        }

        if (!sequence.isValid())
        {
            outResult.saveResult.errorMessage = "Cannot export an invalid retained XP sequence.";
            outResult.saveResult.outputPath = manifestFilePath;
            return false;
        }

        if (!hasXpSeqExtension(manifestFilePath))
        {
            outResult.saveResult.errorMessage = "Manifest-first sequence export requires a .xpseq output path.";
            outResult.saveResult.outputPath = manifestFilePath;
            return false;
        }

        RetainedExportOptions perFrameOptions = options;
        perFrameOptions.mode = RetainedExportMode::LayeredXp;
        perFrameOptions.includeHiddenLayers = true;
        perFrameOptions.allowHiddenLayerVisibilityLoss = true;

        std::vector<SequenceFramePlan> framePlans;
        if (!buildSequenceFramePlans(sequence, manifestFilePath, options, outResult.saveResult, framePlans))
        {
            outResult.saveResult.outputPath = manifestFilePath;
            return false;
        }

        for (const SequenceFramePlan& framePlan : framePlans)
        {
            const XpArtLoader::XpFrame& frame = *framePlan.frame;
            const XpArtLoader::XpDocument* frameDocument = frame.getDocument();
            if (frameDocument == nullptr)
            {
                outResult.saveResult.errorMessage = "Retained XP sequence contains a frame with no document.";
                outResult.saveResult.outputPath = manifestFilePath;
                return false;
            }

            if (!framePlan.linksExistingSource)
            {
                TextObjectExporter::SaveResult frameResult;
                if (!saveToFile(*frameDocument, framePlan.resolvedFramePath.string(), perFrameOptions, frameResult))
                {
                    outResult.saveResult = frameResult;
                    outResult.saveResult.outputPath = framePlan.resolvedFramePath.string();
                    return false;
                }

                appendFrameWarningsToTopLevel(
                    outResult.saveResult,
                    frameResult,
                    framePlan.resolvedFramePath.string());
            }

            FrameFileRecord record;
            record.frameIndex = frame.frameIndex;
            record.label = frame.label;
            record.path = framePlan.resolvedFramePath.string();
            outResult.frameFiles.push_back(std::move(record));
        }

        TextObjectExporter::SaveResult manifestSaveResult;
        if (!exportSequenceManifestToBytes(sequence, manifestFilePath, options, manifestSaveResult))
        {
            mergeManifestSaveResultIntoTopLevel(outResult.saveResult, manifestSaveResult);
            outResult.saveResult.outputPath = manifestFilePath;
            return false;
        }

        std::ofstream file(manifestFilePath, std::ios::binary);
        if (!file)
        {
            mergeManifestSaveResultIntoTopLevel(outResult.saveResult, manifestSaveResult);
            outResult.saveResult.success = false;
            outResult.saveResult.errorMessage = "Failed to open .xpseq manifest output file for writing.";
            outResult.saveResult.outputPath = manifestFilePath;
            outResult.saveResult.bytes.clear();
            return false;
        }

        file.write(
            manifestSaveResult.bytes.data(),
            static_cast<std::streamsize>(manifestSaveResult.bytes.size()));
        if (!file)
        {
            mergeManifestSaveResultIntoTopLevel(outResult.saveResult, manifestSaveResult);
            outResult.saveResult.success = false;
            outResult.saveResult.errorMessage = "Failed while writing .xpseq manifest output bytes.";
            outResult.saveResult.outputPath = manifestFilePath;
            outResult.saveResult.bytes.clear();
            return false;
        }

        mergeManifestSaveResultIntoTopLevel(outResult.saveResult, manifestSaveResult);
        outResult.saveResult.outputPath = manifestFilePath;

        bool linkedExistingFrames = false;
        bool wroteNewFrameFiles = false;
        for (const FrameFileRecord& record : outResult.frameFiles)
        {
            std::error_code ec;
            const std::filesystem::path expectedGeneratedPath =
                std::filesystem::path(buildFrameFilePath(manifestFilePath, options, record.frameIndex)).lexically_normal();
            const std::filesystem::path actualPath = std::filesystem::path(record.path).lexically_normal();
            if (actualPath == expectedGeneratedPath)
            {
                wroteNewFrameFiles = true;
            }
            else
            {
                linkedExistingFrames = true;
            }

            if (actualPath.is_absolute() && options.allowAbsoluteFrameSourcePaths)
            {
                linkedExistingFrames = true;
            }
        }

        if (linkedExistingFrames)
        {
            addWarningOnce(
                outResult.saveResult,
                SaveWarningCode::LossyConversionOccurred,
                "Manifest-first .xpseq export linked one or more existing external .xp frame assets instead of rewriting every frame.");
        }

        outResult.saveResult.success = true;
        return true;
    }

    bool saveSequenceToFrameFiles(
        const XpArtLoader::XpSequence& sequence,
        const std::string& baseFilePath,
        const RetainedExportOptions& options,
        RetainedExportResult& outResult)
    {
        outResult = RetainedExportResult{};

        if (!sequence.isValid())
        {
            outResult.saveResult.errorMessage = "Cannot export an invalid retained XP sequence.";
            return false;
        }

        if (sequence.getFrameCount() > 1)
        {
            addWarningOnce(
                outResult.saveResult,
                SaveWarningCode::LossyConversionOccurred,
                "Frame-per-file XP export writes each retained frame as a separate .xp file and does not preserve sequence membership in a single file.");
        }

        if (sequenceHasAnyFrameLabels(sequence))
        {
            addWarningOnce(
                outResult.saveResult,
                SaveWarningCode::LossyConversionOccurred,
                "Frame-per-file XP export does not embed frame labels unless your caller preserves them externally (for example in file naming or a manifest).");
        }

        RetainedExportOptions perFrameOptions = options;
        perFrameOptions.mode = (options.mode == RetainedExportMode::FlattenedXp)
            ? RetainedExportMode::FlattenedXp
            : RetainedExportMode::LayeredXp;

        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            const XpArtLoader::XpDocument* frameDocument = frame.getDocument();
            if (frameDocument == nullptr)
            {
                outResult.saveResult.errorMessage = "Retained XP sequence contains a frame with no document.";
                return false;
            }

            TextObjectExporter::SaveResult frameResult;
            const std::string framePath = buildFrameFilePath(baseFilePath, options, frame.frameIndex);
            if (!saveToFile(*frameDocument, framePath, perFrameOptions, frameResult))
            {
                outResult.saveResult = frameResult;
                outResult.saveResult.outputPath = framePath;
                return false;
            }

            appendFrameWarningsToTopLevel(
                outResult.saveResult,
                frameResult,
                framePath);

            FrameFileRecord record;
            record.frameIndex = frame.frameIndex;
            record.label = frame.label;
            record.path = framePath;
            outResult.frameFiles.push_back(record);
        }

        outResult.saveResult.success = true;
        outResult.saveResult.outputPath = baseFilePath;
        outResult.saveResult.resolvedEncoding = TextObjectExporter::Encoding::Binary;
        outResult.saveResult.resolvedFileType = TextObjectExporter::FileType::Xp;
        return true;
    }

    const char* toString(RetainedExportMode mode)
    {
        switch (mode)
        {
        case RetainedExportMode::LayeredXp:
            return "LayeredXp";
        case RetainedExportMode::FlattenedXp:
            return "FlattenedXp";
        case RetainedExportMode::XpSequenceManifest:
            return "XpSequenceManifest";
        case RetainedExportMode::FramePerFile:
            return "FramePerFile";
        case RetainedExportMode::ExperimentalBinarySequenceContainerInternal:
            return "ExperimentalBinarySequenceContainerInternal";
        default:
            return "Unknown";
        }
    }
}
