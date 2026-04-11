#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/TextObject.h"

namespace TextObjectExporter
{
    enum class FileType
    {
        Auto,
        Unknown,
        Txt,
        Asc,
        Diz,
        Nfo,
        Ans,
        Bin,
        Xp
    };

    enum class Encoding
    {
        Auto,
        Utf8,
        Ascii,
        Latin1,
        Cp437,
        Binary
    };

    enum class LineEnding
    {
        Lf,
        CrLf
    };

    enum class SaveWarningCode
    {
        None,
        NonCp437NfoEncodingOverride,
        LossyConversionOccurred,
        Utf8BomIncluded,

        TerminalArtColorApproximationOccurred,
        TerminalArtThemeColorApproximationOccurred,
        TerminalArtUnsupportedStyleDropped,
        TerminalArtReverseApproximated,
        TerminalArtBoldApproximated,
        TerminalArtIceColorExportUsed,
        TerminalArtTrailingSpacesForced,
        TerminalArtEncodingForcedToCp437,

        XpThemeColorResolvedToRgb,
        XpUnsupportedStyleDropped,
        XpReverseApproximated,
        XpInvisibleApproximated,
        XpGlyphFallbackSubstituted,
        XpGlyphReplacementUsed,

        XpSequenceSourcePathPortabilityRisk,
        XpSequenceMixedLinkedAndGeneratedFrames,
        XpSequenceSuspiciousFrameIndexing,
        XpSequenceRedundantMetadata,
        XpSequenceConflictingMetadata,

        HighFidelityXpRecommended
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

    struct SaveWarning
    {
        SaveWarningCode code = SaveWarningCode::None;
        std::string message;
        SourcePosition sourcePosition;
        char32_t sourceCodePoint = U'\0';
        char32_t replacementCodePoint = U'\0';

        bool isValid() const
        {
            return code != SaveWarningCode::None;
        }
    };

    struct SaveOptions
    {
        FileType fileType = FileType::Auto;
        Encoding encoding = Encoding::Auto;

        LineEnding lineEnding = LineEnding::Lf;

        bool includeUtf8Bom = false;
        bool preserveTrailingSpaces = true;

        bool allowLossyConversion = false;
        char replacementChar = '?';

        bool allowNonCp437NfoEncoding = false;

        bool enableIceColors = false;
        bool ansiEmitFinalReset = true;
        bool allowTerminalArtApproximation = true;

        char32_t xpReplacementGlyph = U'?';
    };

    struct SaveResult
    {
        bool success = false;

        std::string outputPath;

        FileType resolvedFileType = FileType::Unknown;
        Encoding resolvedEncoding = Encoding::Utf8;
        LineEnding resolvedLineEnding = LineEnding::Lf;

        bool usedUtf8Bom = false;
        bool hadLossyConversion = false;
        std::size_t lossyCodePointCount = 0;

        bool hasEncodingFailure = false;
        char32_t firstFailingCodePoint = U'\0';
        SourcePosition firstFailingPosition;

        std::vector<SaveWarning> warnings;

        std::string bytes;
        std::string errorMessage;
    };

    FileType detectFileType(const std::string& filePath);

    Encoding resolveEffectiveEncoding(
        FileType fileType,
        const SaveOptions& options,
        std::string* outErrorMessage = nullptr);

    bool canEncodeCodePoint(char32_t codePoint, Encoding encoding);

    SaveResult exportToBytes(const TextObject& object, const SaveOptions& options = {});
    SaveResult saveToFile(const TextObject& object, const std::string& filePath, const SaveOptions& options = {});

    bool trySaveToFile(const TextObject& object, const std::string& filePath);
    bool trySaveToFile(const TextObject& object, const std::string& filePath, const SaveOptions& options);

    std::string formatSaveError(const SaveResult& result);
    std::string formatSaveSuccess(const SaveResult& result);

    bool hasWarning(const SaveResult& result, SaveWarningCode code);

    bool shouldRecommendXpForFidelity(const SaveResult& result);

    const SaveWarning* getWarningByCode(
        const SaveResult& result,
        SaveWarningCode code);

    const char* toString(FileType fileType);
    const char* toString(Encoding encoding);
    const char* toString(LineEnding lineEnding);
    const char* toString(SaveWarningCode warningCode);
}
