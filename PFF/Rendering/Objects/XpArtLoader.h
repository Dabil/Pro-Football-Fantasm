#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace XpArtLoader
{
    enum class FileType
    {
        Auto,
        Unknown,
        Xp
    };

    enum class LoadWarningCode
    {
        None,
        InputWasAlreadyDecompressed,
        TransparentCellsSkipped,
        MultipleLayersFlattened,
        ExtraTrailingBytesIgnored,
        LegacyVersionHeaderDetected,
        LayerSizeMismatchClipped,
        HiddenLayersSkipped,
        NonDefaultCompositeModeUsed
    };

    enum class XpCompositeMode
    {
        // Default Phase 4.5B-compatible compositing behavior:
        // - transparent background + visible glyph overlays glyph/foreground only
        // - transparent background + blank glyph contributes nothing
        // - opaque cells overwrite background and optionally glyph
        Phase45BCompatible,

        // Alternate policy:
        // - transparent-background cells contribute nothing at all
        // - only opaque cells affect the flattened result
        StrictOpaqueOverwrite
    };

    struct SourcePosition
    {
        int x = -1;
        int y = -1;

        bool isValid() const
        {
            return x >= 0 && y >= 0;
        }
    };

    struct LoadWarning
    {
        LoadWarningCode code = LoadWarningCode::None;
        std::string message;
        std::size_t byteOffset = 0;
        SourcePosition sourcePosition;

        bool isValid() const
        {
            return code != LoadWarningCode::None;
        }
    };

    struct RgbColor
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;

        bool operator==(const RgbColor& other) const
        {
            return red == other.red &&
                green == other.green &&
                blue == other.blue;
        }

        bool operator!=(const RgbColor& other) const
        {
            return !(*this == other);
        }
    };

    struct CellData
    {
        std::uint32_t glyph = 0;
        RgbColor foreground;
        RgbColor background;

        bool hasTransparentBackground() const;
    };

    struct LayerData
    {
        int width = 0;
        int height = 0;
        std::vector<CellData> cells;

        bool isValid() const;
        bool inBounds(int x, int y) const;
        const CellData* tryGetCell(int x, int y) const;
    };

    // ParsedDocument is the raw file-structure-oriented parse result.
    // It should remain separate from retained runtime-ready layer content.
    struct ParsedDocument
    {
        int formatVersion = 0;
        bool usesLegacyLayerCountHeader = false;
        int canvasWidth = 0;
        int canvasHeight = 0;
        std::vector<LayerData> layers;

        bool isValid() const;
    };

    struct ParseResult
    {
        ParsedDocument document;
        bool success = false;
        bool inputWasCompressed = false;
        bool inputWasAlreadyDecompressed = false;

        std::size_t bytesConsumed = 0;
        std::string errorMessage;

        bool hasParseFailure = false;
        std::size_t failingByteOffset = 0;
        SourcePosition firstFailingPosition;
    };

    // Per-layer retained metadata intended for diagnostics/inspection.
    // This stores facts learned during parse/build and later updated facts
    // about how flattening actually used the layer.
    struct XpLayerMetadata
    {
        int sourceLayerIndex = -1;
        int sourceWidth = 0;
        int sourceHeight = 0;

        bool matchedCanvasSize = false;
        bool clippingOccurredDuringFlatten = false;

        bool encounteredTransparentBackgroundCells = false;
        bool encounteredVisibleGlyphsOnTransparentBackground = false;

        bool visibilityUsedForFlattening = true;
        XpCompositeMode compositeModeUsed = XpCompositeMode::Phase45BCompatible;
    };

    // Document-level retained metadata intended for diagnostics/inspection.
    // This keeps the retained document self-describing enough for validation
    // screens and import inspectors without forcing a re-parse.
    struct XpDocumentMetadata
    {
        int canvasWidth = 0;
        int canvasHeight = 0;
        int layerCount = 0;
        int parsedFormatVersion = 0;

        bool retainedPathAvailable = false;
        bool flattenPathUsed = false;

        bool inputWasCompressed = false;
        bool inputWasAlreadyDecompressed = false;

        XpCompositeMode compositeModeUsed = XpCompositeMode::Phase45BCompatible;
    };

    // Retained runtime-ready XP content.
    struct XpLayerCell
    {
        std::uint32_t glyph = 0;
        RgbColor foreground;
        RgbColor background;

        bool hasTransparentBackground() const;
    };

    struct XpLayer
    {
        int width = 0;
        int height = 0;
        bool visible = true;
        std::vector<XpLayerCell> cells;
        XpLayerMetadata metadata;

        bool isValid() const;
        bool inBounds(int x, int y) const;
        const XpLayerCell* tryGetCell(int x, int y) const;
    };

    // XpDocument is the retained post-parse XP representation.
    // It preserves layer order, dimensions, cell content, visibility state,
    // and inspection metadata for future validation/debug workflows.
    struct XpDocument
    {
        int width = 0;
        int height = 0;
        int formatVersion = 0;
        bool usesLegacyLayerCountHeader = false;
        std::vector<XpLayer> layers;
        XpDocumentMetadata metadata;

        bool isValid() const;
        int getLayerCount() const;
    };

    // A retained frame wraps one retained XP document so the importer can grow
    // from today's single-document path into future animation/state-variant or
    // frame-sequence workflows without redesigning the retained XP model.
    struct XpFrame
    {
        int frameIndex = 0;
        std::string label;
        XpDocument document;

        bool isValid() const;
    };

    // Minimal retained frame container.
    // Current single XP imports populate this as a trivial one-frame sequence.
    // Future multi-frame importers can append additional frames without changing
    // the current XpDocument or flattened TextObject conversion model.
    struct XpSequence
    {
        std::vector<XpFrame> frames;

        bool isValid() const;
        int getFrameCount() const;
        const XpFrame* tryGetFrame(int frameIndex) const;
        const XpFrame* getDefaultFrame() const;
    };

    struct LoadOptions
    {
        FileType fileType = FileType::Auto;

        bool allowCompressedInput = true;
        bool allowAlreadyDecompressedInput = true;

        // Current runtime path still flattens into TextObject by default.
        bool flattenLayers = true;

        bool treatMagentaBackgroundAsTransparent = true;
        bool visibleTransparentBaseCellsUseBlackBackground = true;
        bool strictLayerSizeValidation = true;

        // Phase 4.5D compositing policy selection.
        XpCompositeMode compositeMode = XpCompositeMode::Phase45BCompatible;

        std::optional<Style> baseStyle;
    };

    struct LoadResult
    {
        TextObject object;
        XpDocument retainedDocument;
        XpSequence retainedSequence;

        bool success = false;
        bool hasRetainedDocument = false;
        bool hasRetainedSequence = false;

        FileType detectedFileType = FileType::Unknown;
        int resolvedWidth = 0;
        int resolvedHeight = 0;
        int resolvedLayerCount = 0;
        int resolvedFrameCount = 0;
        int parsedFormatVersion = 0;

        bool inputWasCompressed = false;
        bool inputWasAlreadyDecompressed = false;
        bool usedLayerFlattening = false;

        XpCompositeMode compositeModeUsed = XpCompositeMode::Phase45BCompatible;
        int visibleLayerCount = 0;
        int hiddenLayerCount = 0;

        std::vector<LoadWarning> warnings;

        bool hasParseFailure = false;
        std::size_t failingByteOffset = 0;
        SourcePosition firstFailingPosition;

        std::string errorMessage;
    };

    FileType detectFileType(const std::string& filePath);

    ParseResult parseDecompressedPayload(std::string_view bytes);

    // Conversion boundary:
    // ParsedDocument -> retained XpDocument
    XpDocument buildRetainedDocument(const ParsedDocument& document);

    // Future-ready frame/container conversion seams.
    XpFrame buildRetainedFrame(
        const ParsedDocument& document,
        int frameIndex = 0,
        const std::string& label = std::string());

    XpSequence buildRetainedSequence(const ParsedDocument& document);

    XpSequence buildRetainedSequence(
        const XpDocument& document,
        int frameIndex = 0,
        const std::string& label = std::string());

    LoadResult loadFromFile(const std::string& filePath);
    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options);
    LoadResult loadFromFile(const std::string& filePath, const Style& style);

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject);
    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const LoadOptions& options);
    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const Style& style);

    LoadResult loadFromBytes(std::string_view bytes, const LoadOptions& options = {});

    // Compatibility wrapper:
    // ParsedDocument -> retained XpDocument -> flattened TextObject
    LoadResult buildTextObject(const ParsedDocument& document, const LoadOptions& options = {});

    // Retained-model flattening entry point:
    // XpDocument -> flattened TextObject
    LoadResult buildTextObject(const XpDocument& document, const LoadOptions& options = {});
    LoadResult buildTextObject(const XpFrame& frame, const LoadOptions& options = {});
    LoadResult buildTextObject(const XpSequence& sequence, const LoadOptions& options = {});

    bool hasWarning(const LoadResult& result, LoadWarningCode code);
    const LoadWarning* getWarningByCode(const LoadResult& result, LoadWarningCode code);

    // Small inspection helpers for diagnostics code.
    const XpLayer* tryGetLayer(const XpDocument& document, int layerIndex);
    const XpLayerMetadata* tryGetLayerMetadata(const XpDocument& document, int layerIndex);
    const XpFrame* tryGetFrame(const XpSequence& sequence, int frameIndex);

    std::string formatLoadError(const LoadResult& result);
    std::string formatLoadSuccess(const LoadResult& result);
    std::string formatRetainedDocumentSummary(const LoadResult& result);
    std::string formatRetainedLayerSummary(const LoadResult& result, int layerIndex);
    std::string formatRetainedSequenceSummary(const LoadResult& result);

    const char* toString(FileType fileType);
    const char* toString(LoadWarningCode warningCode);
    const char* toString(XpCompositeMode compositeMode);
}