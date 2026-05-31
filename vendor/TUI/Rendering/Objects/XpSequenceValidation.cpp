#include "Rendering/Objects/XpSequenceValidation.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace
{
    using namespace XpSequenceValidation;

    struct ManifestFrameEntry
    {
        int frameIndex = -1;
        std::string source;
    };

    std::string trimCopy(std::string_view value)
    {
        std::size_t begin = 0;
        while (begin < value.size() &&
            std::isspace(static_cast<unsigned char>(value[begin])) != 0)
        {
            ++begin;
        }

        std::size_t end = value.size();
        while (end > begin &&
            std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
        {
            --end;
        }

        return std::string(value.substr(begin, end - begin));
    }

    std::string stripQuotes(std::string value)
    {
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        {
            value.erase(value.begin());
            value.pop_back();
        }

        return value;
    }

    std::vector<std::string> tokenizeManifestLine(std::string_view text)
    {
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;

        for (const char ch : text)
        {
            if (ch == '"')
            {
                inQuotes = !inQuotes;
                current.push_back(ch);
                continue;
            }

            if (!inQuotes && std::isspace(static_cast<unsigned char>(ch)) != 0)
            {
                if (!current.empty())
                {
                    tokens.push_back(current);
                    current.clear();
                }

                continue;
            }

            current.push_back(ch);
        }

        if (!current.empty())
        {
            tokens.push_back(current);
        }

        return tokens;
    }

    bool tryParseInt(const std::string& text, int& outValue)
    {
        try
        {
            std::size_t consumed = 0;
            const int value = std::stoi(text, &consumed, 10);
            if (consumed != text.size())
            {
                return false;
            }

            outValue = value;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string tryCanonicalizePath(const std::string& filePath)
    {
        if (filePath.empty())
        {
            return std::string();
        }

        std::error_code error;
        const std::filesystem::path weaklyCanonical =
            std::filesystem::weakly_canonical(std::filesystem::path(filePath), error);

        if (error)
        {
            return std::filesystem::path(filePath).lexically_normal().string();
        }

        return weaklyCanonical.string();
    }

    bool readAllText(const std::string& filePath, std::string& outText)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            return false;
        }

        std::ostringstream stream;
        stream << file.rdbuf();
        outText = stream.str();
        return file.good() || file.eof();
    }

    void appendDiagnostic(
        ValidationResult& result,
        DiagnosticCode code,
        const std::string& message,
        const int frameIndex,
        const std::string& manifestPath,
        const std::string& sourcePath = std::string(),
        const bool isError = true)
    {
        Diagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.isError = isError;
        diagnostic.frameIndex = frameIndex;
        diagnostic.manifestPath = manifestPath;
        diagnostic.sourcePath = sourcePath;
        diagnostic.message = message;
        result.diagnostics.push_back(std::move(diagnostic));
    }

    void appendDiagnostic(
        RoundTripResult& result,
        DiagnosticCode code,
        const std::string& message,
        const int frameIndex,
        const std::string& manifestPath,
        const std::string& sourcePath = std::string(),
        const bool isError = true)
    {
        Diagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.isError = isError;
        diagnostic.frameIndex = frameIndex;
        diagnostic.manifestPath = manifestPath;
        diagnostic.sourcePath = sourcePath;
        diagnostic.message = message;
        result.diagnostics.push_back(std::move(diagnostic));
    }

    void appendDiagnostics(RoundTripResult& result, const ValidationResult& validation)
    {
        for (const Diagnostic& diagnostic : validation.diagnostics)
        {
            result.diagnostics.push_back(diagnostic);
        }
    }

    bool hasErrors(const std::vector<Diagnostic>& diagnostics)
    {
        return std::any_of(
            diagnostics.begin(),
            diagnostics.end(),
            [](const Diagnostic& diagnostic)
            {
                return diagnostic.isValid() && diagnostic.isError;
            });
    }

    bool hasDuplicateValues(const std::vector<int>& values)
    {
        std::vector<int> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        return std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end();
    }

    bool containsParentDirectoryEscape(const std::filesystem::path& relativePath)
    {
        const std::filesystem::path normalized = relativePath.lexically_normal();
        return !normalized.empty() &&
            !normalized.is_absolute() &&
            *normalized.begin() == "..";
    }

    void appendSequenceMetadataWarnings(
        ValidationResult& result,
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestPath)
    {
        if (!sequence.metadata.name.empty() && !sequence.metadata.sequenceLabel.empty())
        {
            appendDiagnostic(
                result,
                sequence.metadata.name == sequence.metadata.sequenceLabel
                ? DiagnosticCode::LoadedRedundantMetadata
                : DiagnosticCode::LoadedConflictingMetadata,
                sequence.metadata.name == sequence.metadata.sequenceLabel
                ? "Loaded XP sequence contains both name and sequenceLabel with the same value."
                : "Loaded XP sequence contains both name and sequenceLabel with different values.",
                -1,
                manifestPath,
                std::string(),
                false);
        }

        if (!sequence.metadata.defaultExplicitVisibleLayerIndices.empty())
        {
            if (sequence.metadata.defaultVisibleLayerMode != XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::LoadedRedundantMetadata,
                    "Loaded XP sequence contains default explicit visible-layer indices without default_visible_layers=UseExplicitVisibleLayerList.",
                    -1,
                    manifestPath,
                    std::string(),
                    false);
            }

            if (hasDuplicateValues(sequence.metadata.defaultExplicitVisibleLayerIndices))
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::LoadedConflictingMetadata,
                    "Loaded XP sequence contains duplicate sequence-level explicit visible-layer indices.",
                    -1,
                    manifestPath,
                    std::string(),
                    false);
            }
        }

        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            if (!frame.sourcePath.empty())
            {
                const std::filesystem::path sourcePath(frame.sourcePath);
                if (sourcePath.is_absolute() || containsParentDirectoryEscape(sourcePath))
                {
                    appendDiagnostic(
                        result,
                        DiagnosticCode::LoadedSourcePathPortabilityRisk,
                        sourcePath.is_absolute()
                        ? "Loaded XP frame source path is absolute and may not be portable."
                        : "Loaded XP frame source path escapes the manifest directory using '..'.",
                        frame.frameIndex,
                        manifestPath,
                        frame.sourcePath,
                        false);
                }
            }

            if (frame.overrides.durationMilliseconds.has_value() &&
                sequence.metadata.defaultFrameDurationMilliseconds.has_value() &&
                frame.overrides.durationMilliseconds == sequence.metadata.defaultFrameDurationMilliseconds)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::LoadedRedundantMetadata,
                    "Loaded XP frame contains a duration override that matches the sequence default.",
                    frame.frameIndex,
                    manifestPath,
                    frame.sourcePath,
                    false);
            }

            if (frame.overrides.compositeMode.has_value() &&
                sequence.metadata.defaultCompositeMode.has_value() &&
                frame.overrides.compositeMode == sequence.metadata.defaultCompositeMode)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::LoadedRedundantMetadata,
                    "Loaded XP frame contains a composite override that matches the sequence default.",
                    frame.frameIndex,
                    manifestPath,
                    frame.sourcePath,
                    false);
            }

            if (frame.overrides.visibleLayerMode.has_value() &&
                sequence.metadata.defaultVisibleLayerMode.has_value() &&
                frame.overrides.visibleLayerMode == sequence.metadata.defaultVisibleLayerMode)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::LoadedRedundantMetadata,
                    "Loaded XP frame contains a visible-layer override that matches the sequence default.",
                    frame.frameIndex,
                    manifestPath,
                    frame.sourcePath,
                    false);
            }

            if (!frame.overrides.explicitVisibleLayerIndices.empty())
            {
                if (frame.overrides.visibleLayerMode != XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList)
                {
                    appendDiagnostic(
                        result,
                        DiagnosticCode::LoadedRedundantMetadata,
                        "Loaded XP frame contains explicit visible-layer indices without visible_layers=UseExplicitVisibleLayerList.",
                        frame.frameIndex,
                        manifestPath,
                        frame.sourcePath,
                        false);
                }

                if (hasDuplicateValues(frame.overrides.explicitVisibleLayerIndices))
                {
                    appendDiagnostic(
                        result,
                        DiagnosticCode::LoadedConflictingMetadata,
                        "Loaded XP frame contains duplicate explicit visible-layer indices.",
                        frame.frameIndex,
                        manifestPath,
                        frame.sourcePath,
                        false);
                }
            }
        }
    }

    void appendExportWarningDiagnostics(
        ValidationResult& result,
        const TextObjectExporter::SaveResult& saveResult)
    {
        for (const TextObjectExporter::SaveWarning& warning : saveResult.warnings)
        {
            switch (warning.code)
            {
            case TextObjectExporter::SaveWarningCode::XpSequenceMixedLinkedAndGeneratedFrames:
                appendDiagnostic(
                    result,
                    DiagnosticCode::ExportMixedLinkedAndGeneratedFrames,
                    warning.message,
                    -1,
                    result.manifestPath,
                    std::string(),
                    false);
                break;

            case TextObjectExporter::SaveWarningCode::XpSequenceSourcePathPortabilityRisk:
                appendDiagnostic(
                    result,
                    DiagnosticCode::ExportSourcePathPortabilityRisk,
                    warning.message,
                    -1,
                    result.manifestPath,
                    std::string(),
                    false);
                break;

            case TextObjectExporter::SaveWarningCode::XpSequenceSuspiciousFrameIndexing:
                appendDiagnostic(
                    result,
                    DiagnosticCode::ExportSuspiciousFrameIndexing,
                    warning.message,
                    -1,
                    result.manifestPath,
                    std::string(),
                    false);
                break;

            case TextObjectExporter::SaveWarningCode::XpSequenceRedundantMetadata:
                appendDiagnostic(
                    result,
                    DiagnosticCode::ExportRedundantMetadata,
                    warning.message,
                    -1,
                    result.manifestPath,
                    std::string(),
                    false);
                break;

            case TextObjectExporter::SaveWarningCode::XpSequenceConflictingMetadata:
                appendDiagnostic(
                    result,
                    DiagnosticCode::ExportConflictingMetadata,
                    warning.message,
                    -1,
                    result.manifestPath,
                    std::string(),
                    false);
                break;

            default:
                break;
            }
        }
    }

    std::string joinIntegers(const std::vector<int>& values)
    {
        std::ostringstream stream;
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                stream << ',';
            }

            stream << values[index];
        }

        return stream.str();
    }

    void compareOptionalInt(
        ValidationResult& result,
        const DiagnosticCode code,
        const std::optional<int>& expected,
        const std::optional<int>& actual,
        const std::string& manifestPath,
        const std::string& label,
        const int frameIndex = -1)
    {
        if (expected == actual)
        {
            return;
        }

        const std::string expectedText = expected.has_value()
            ? std::to_string(*expected)
            : std::string("<unset>");
        const std::string actualText = actual.has_value()
            ? std::to_string(*actual)
            : std::string("<unset>");

        appendDiagnostic(
            result,
            code,
            label + " mismatch. Expected " + expectedText + ", reloaded " + actualText + ".",
            frameIndex,
            manifestPath);
    }

    template <typename T>
    void compareOptionalEnum(
        ValidationResult& result,
        const DiagnosticCode code,
        const std::optional<T>& expected,
        const std::optional<T>& actual,
        const std::string& manifestPath,
        const std::string& label,
        const char* (*toStringFn)(T),
        const int frameIndex = -1)
    {
        if (expected == actual)
        {
            return;
        }

        const std::string expectedText = expected.has_value()
            ? std::string(toStringFn(*expected))
            : std::string("<unset>");
        const std::string actualText = actual.has_value()
            ? std::string(toStringFn(*actual))
            : std::string("<unset>");

        appendDiagnostic(
            result,
            code,
            label + " mismatch. Expected " + expectedText + ", reloaded " + actualText + ".",
            frameIndex,
            manifestPath);
    }

    void compareString(
        ValidationResult& result,
        const DiagnosticCode code,
        const std::string& expected,
        const std::string& actual,
        const std::string& manifestPath,
        const std::string& label,
        const int frameIndex = -1)
    {
        if (expected == actual)
        {
            return;
        }

        appendDiagnostic(
            result,
            code,
            label + " mismatch. Expected '" + expected + "', reloaded '" + actual + "'.",
            frameIndex,
            manifestPath);
    }

    void compareOptionalBool(
        ValidationResult& result,
        const DiagnosticCode code,
        const std::optional<bool>& expected,
        const std::optional<bool>& actual,
        const std::string& manifestPath,
        const std::string& label)
    {
        if (expected == actual)
        {
            return;
        }

        const auto toText = [](const std::optional<bool>& value) -> std::string
            {
                if (!value.has_value())
                {
                    return "<unset>";
                }

                return *value ? "true" : "false";
            };

        appendDiagnostic(
            result,
            code,
            label + " mismatch. Expected " + toText(expected) + ", reloaded " + toText(actual) + ".",
            -1,
            manifestPath);
    }

    void compareIntVector(
        ValidationResult& result,
        const DiagnosticCode code,
        const std::vector<int>& expected,
        const std::vector<int>& actual,
        const std::string& manifestPath,
        const std::string& label,
        const int frameIndex = -1)
    {
        if (expected == actual)
        {
            return;
        }

        appendDiagnostic(
            result,
            code,
            label + " mismatch. Expected [" + joinIntegers(expected) + "], reloaded [" + joinIntegers(actual) + "].",
            frameIndex,
            manifestPath);
    }

    std::vector<ManifestFrameEntry> parseManifestFrameEntries(const std::string& manifestBytes)
    {
        std::vector<ManifestFrameEntry> entries;
        std::istringstream stream(manifestBytes);
        std::string line;

        while (std::getline(stream, line))
        {
            const std::string trimmed = trimCopy(line);
            if (trimmed.empty() || trimmed[0] == '#')
            {
                continue;
            }

            if (trimmed == "frame{" || trimmed == "frame {" || trimmed == "}")
            {
                continue;
            }

            if (trimmed.rfind("frame", 0) != 0)
            {
                continue;
            }

            ManifestFrameEntry entry;
            const std::vector<std::string> tokens = tokenizeManifestLine(trimmed);
            for (std::size_t index = 1; index < tokens.size(); ++index)
            {
                const std::string& token = tokens[index];
                const std::size_t equals = token.find('=');
                if (equals == std::string::npos)
                {
                    continue;
                }

                const std::string key = token.substr(0, equals);
                const std::string value = stripQuotes(token.substr(equals + 1));
                if (key == "index")
                {
                    int parsedIndex = -1;
                    if (tryParseInt(value, parsedIndex))
                    {
                        entry.frameIndex = parsedIndex;
                    }
                }
                else if (key == "source")
                {
                    entry.source = value;
                }
            }

            entries.push_back(std::move(entry));
        }

        return entries;
    }

    ValidationResult validateManifestExport(
        const XpArtExporter::RetainedExportResult& exportResult,
        const ValidationOptions& options)
    {
        ValidationResult result;
        result.manifestPath = exportResult.saveResult.outputPath;

        if (result.manifestPath.empty())
        {
            result.errorMessage = "Round-trip validation requires a concrete .xpseq manifest path.";
            appendDiagnostic(
                result,
                DiagnosticCode::ExportManifestPathRequired,
                result.errorMessage,
                -1,
                result.manifestPath);
            return result;
        }

        if (!exportResult.saveResult.success)
        {
            result.errorMessage = exportResult.saveResult.errorMessage.empty()
                ? std::string("Manifest export failed.")
                : exportResult.saveResult.errorMessage;
            appendDiagnostic(
                result,
                DiagnosticCode::ExportFailed,
                result.errorMessage,
                -1,
                result.manifestPath);
            return result;
        }

        if (options.exportOptions.mode != XpArtExporter::RetainedExportMode::XpSequenceManifest)
        {
            appendDiagnostic(
                result,
                DiagnosticCode::ExportModeMustBeManifestFirst,
                "Round-trip validation currently requires RetainedExportMode::XpSequenceManifest.",
                -1,
                result.manifestPath);
        }

        if (TextObjectExporter::hasWarning(
            exportResult.saveResult,
            TextObjectExporter::SaveWarningCode::LossyConversionOccurred))
        {
            appendDiagnostic(
                result,
                DiagnosticCode::ExportLossyWarning,
                "Manifest export reported lossy or mixed-source warnings through SaveResult.",
                -1,
                result.manifestPath,
                std::string(),
                options.treatExportLossyWarningsAsErrors);
        }

        appendExportWarningDiagnostics(result, exportResult.saveResult);

        bool linkedExistingFrames = false;
        bool generatedFrames = false;
        const std::filesystem::path manifestPath = std::filesystem::path(result.manifestPath).lexically_normal();
        for (const XpArtExporter::FrameFileRecord& record : exportResult.frameFiles)
        {
            const std::string canonicalRecordPath = tryCanonicalizePath(record.path);
            std::error_code ec;
            if (!record.path.empty() && !std::filesystem::exists(std::filesystem::path(record.path), ec))
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::MissingFrameSourceOnDisk,
                    "Export reported a frame file path that does not exist on disk.",
                    record.frameIndex,
                    result.manifestPath,
                    record.path);
            }

            const std::filesystem::path generatedPath(record.path);
            const std::string stem = manifestPath.stem().string();
            std::ostringstream expectedName;
            expectedName
                << stem
                << options.exportOptions.frameFileSeparator
                << std::setw(std::max(1, options.exportOptions.frameNumberWidth))
                << std::setfill('0')
                << record.frameIndex
                << ".xp";
            const std::filesystem::path expectedPath =
                (manifestPath.parent_path() / expectedName.str()).lexically_normal();

            const std::filesystem::path actualPath = std::filesystem::path(canonicalRecordPath).lexically_normal();
            if (actualPath == tryCanonicalizePath(expectedPath.string()))
            {
                generatedFrames = true;
            }
            else
            {
                linkedExistingFrames = true;
            }
        }

        if (linkedExistingFrames && generatedFrames &&
            !TextObjectExporter::hasWarning(
                exportResult.saveResult,
                TextObjectExporter::SaveWarningCode::XpSequenceMixedLinkedAndGeneratedFrames))
        {
            appendDiagnostic(
                result,
                DiagnosticCode::ExportMixedLinkedAndGeneratedFrames,
                "Manifest export used a mixed strategy: some frames were linked to existing .xp assets while others were regenerated.",
                -1,
                result.manifestPath,
                std::string(),
                false);
        }

        std::vector<ManifestFrameEntry> manifestEntries = parseManifestFrameEntries(exportResult.saveResult.bytes);
        if (manifestEntries.size() != exportResult.frameFiles.size())
        {
            appendDiagnostic(
                result,
                DiagnosticCode::ManifestFrameCountMismatch,
                "Manifest frame directive count does not match the number of exported frame file records.",
                -1,
                result.manifestPath);
        }

        const std::size_t count = std::min(manifestEntries.size(), exportResult.frameFiles.size());
        for (std::size_t index = 0; index < count; ++index)
        {
            const ManifestFrameEntry& entry = manifestEntries[index];
            const XpArtExporter::FrameFileRecord& record = exportResult.frameFiles[index];

            if (entry.frameIndex != record.frameIndex)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::ManifestFrameIndexMismatch,
                    "Manifest frame index does not match exported frame record order.",
                    record.frameIndex,
                    result.manifestPath,
                    record.path);
            }

            if (entry.source.empty())
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::ManifestFrameSourceMissing,
                    "Manifest frame entry is missing a source path.",
                    record.frameIndex,
                    result.manifestPath,
                    record.path);
                continue;
            }

            const std::filesystem::path sourcePath(entry.source);
            if (sourcePath.is_absolute() && options.requireRelativeManifestFrameSources)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::ManifestFrameSourceNotRelative,
                    "Manifest frame source was exported as an absolute path.",
                    record.frameIndex,
                    result.manifestPath,
                    entry.source);
            }

            if (!sourcePath.is_absolute())
            {
                const std::filesystem::path normalizedRelative = sourcePath.lexically_normal();
                if (options.disallowManifestRelativeSourceEscapes &&
                    !normalizedRelative.empty() &&
                    *normalizedRelative.begin() == "..")
                {
                    appendDiagnostic(
                        result,
                        DiagnosticCode::ManifestFrameSourceEscapesManifestDirectory,
                        "Manifest frame source escapes the manifest directory using '..'.",
                        record.frameIndex,
                        result.manifestPath,
                        entry.source);
                }
            }

            const std::filesystem::path resolvedPath = sourcePath.is_absolute()
                ? sourcePath.lexically_normal()
                : (manifestPath.parent_path() / sourcePath).lexically_normal();
            const std::string canonicalResolved = tryCanonicalizePath(resolvedPath.string());
            const std::string canonicalRecord = tryCanonicalizePath(record.path);
            if (!canonicalResolved.empty() && !canonicalRecord.empty() && canonicalResolved != canonicalRecord)
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::ManifestFrameSourceResolutionMismatch,
                    "Manifest frame source does not resolve to the exported frame path recorded by the exporter.",
                    record.frameIndex,
                    result.manifestPath,
                    entry.source);
            }
        }

        result.success = !hasErrors(result.diagnostics);
        if (!result.success && result.errorMessage.empty())
        {
            result.errorMessage = "Manifest export validation reported one or more errors.";
        }

        return result;
    }

    std::unordered_map<int, const XpArtLoader::XpFrame*> indexFramesByFrameIndex(
        const XpArtLoader::XpSequence& sequence)
    {
        std::unordered_map<int, const XpArtLoader::XpFrame*> framesByIndex;
        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            framesByIndex[frame.frameIndex] = &frame;
        }

        return framesByIndex;
    }
}

namespace XpSequenceValidation
{
    ValidationResult validateLoadedSequence(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestPath,
        const ValidationOptions& options)
    {
        ValidationResult result;
        result.manifestPath = manifestPath;

        if (!sequence.isValid())
        {
            result.errorMessage = "XP sequence is invalid.";
            appendDiagnostic(
                result,
                DiagnosticCode::SequenceInvalid,
                result.errorMessage,
                -1,
                manifestPath);
            return result;
        }

        if (!sequence.hasUniqueFrameIndices())
        {
            appendDiagnostic(
                result,
                DiagnosticCode::DuplicateFrameIndices,
                "XP sequence contains duplicate frame indices.",
                -1,
                manifestPath);
        }

        if (options.requireContiguousFrameIndices &&
            !sequence.hasContiguousFrameIndicesStartingAtZero())
        {
            appendDiagnostic(
                result,
                DiagnosticCode::NonContiguousFrameIndices,
                "XP sequence frame indices are not contiguous starting at zero.",
                -1,
                manifestPath);
        }

        if (options.requireStoredFramesInFrameIndexOrder &&
            !sequence.areFramesStoredInFrameIndexOrder())
        {
            appendDiagnostic(
                result,
                DiagnosticCode::FrameStorageOrderMismatch,
                "XP sequence frames are not stored in ascending frame-index order.",
                -1,
                manifestPath);
        }

        for (const XpArtLoader::XpFrame& frame : sequence.frames)
        {
            if (options.requireFrameDocumentReferences && !frame.hasDocument())
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::MissingFrameDocumentReference,
                    "XP frame is missing its retained document reference.",
                    frame.frameIndex,
                    manifestPath,
                    frame.sourcePath);
            }

            if (options.requireFrameSourcePaths && !frame.hasSourcePath())
            {
                appendDiagnostic(
                    result,
                    DiagnosticCode::MissingFrameSourcePath,
                    "XP frame is missing its source path.",
                    frame.frameIndex,
                    manifestPath);
            }

            if (options.requireFrameSourceFilesOnDisk && frame.hasSourcePath())
            {
                std::error_code ec;
                if (!std::filesystem::exists(std::filesystem::path(frame.sourcePath), ec) || ec)
                {
                    appendDiagnostic(
                        result,
                        DiagnosticCode::MissingFrameSourceOnDisk,
                        "XP frame source path does not exist on disk.",
                        frame.frameIndex,
                        manifestPath,
                        frame.sourcePath);
                }
            }
        }

        appendSequenceMetadataWarnings(result, sequence, manifestPath);

        result.success = !hasErrors(result.diagnostics);
        if (!result.success && result.errorMessage.empty())
        {
            result.errorMessage = "XP sequence validation reported one or more errors.";
        }

        return result;
    }

    RoundTripResult roundTripSequence(
        const XpArtLoader::XpSequence& sequence,
        const std::string& sourceManifestPath,
        const std::string& roundTripManifestPath,
        const ValidationOptions& options)
    {
        RoundTripResult result;
        result.sourceManifestPath = sourceManifestPath;
        result.roundTripManifestPath = roundTripManifestPath;

        result.sourceValidation = validateLoadedSequence(sequence, sourceManifestPath, options);
        appendDiagnostics(result, result.sourceValidation);
        if (!result.sourceValidation.success)
        {
            result.errorMessage = result.sourceValidation.errorMessage;
            return result;
        }

        if (options.exportOptions.mode != XpArtExporter::RetainedExportMode::XpSequenceManifest)
        {
            result.exportValidation.manifestPath = roundTripManifestPath;
            result.exportValidation.errorMessage =
                "Round-trip validation requires RetainedExportMode::XpSequenceManifest.";
            appendDiagnostic(
                result.exportValidation,
                DiagnosticCode::ExportModeMustBeManifestFirst,
                result.exportValidation.errorMessage,
                -1,
                roundTripManifestPath);
            appendDiagnostics(result, result.exportValidation);
            result.errorMessage = result.exportValidation.errorMessage;
            return result;
        }

        if (!XpArtExporter::saveSequenceToManifestFile(
            sequence,
            roundTripManifestPath,
            options.exportOptions,
            result.exportResult))
        {
            result.exportValidation.manifestPath = roundTripManifestPath;
            result.exportValidation.errorMessage = result.exportResult.saveResult.errorMessage.empty()
                ? std::string("Manifest export failed.")
                : result.exportResult.saveResult.errorMessage;
            appendDiagnostic(
                result.exportValidation,
                DiagnosticCode::ExportFailed,
                result.exportValidation.errorMessage,
                -1,
                roundTripManifestPath);
            appendDiagnostics(result, result.exportValidation);
            result.errorMessage = result.exportValidation.errorMessage;
            return result;
        }

        result.exportValidation = validateManifestExport(result.exportResult, options);
        appendDiagnostics(result, result.exportValidation);
        if (!result.exportValidation.success)
        {
            result.errorMessage = result.exportValidation.errorMessage;
            return result;
        }

        result.reloadResult = XpSequenceLoader::loadFromFile(
            roundTripManifestPath,
            options.reloadLoadOptions);
        if (!result.reloadResult.success)
        {
            result.parityValidation.manifestPath = roundTripManifestPath;
            result.parityValidation.errorMessage = result.reloadResult.errorMessage.empty()
                ? std::string("Reload failed.")
                : result.reloadResult.errorMessage;
            appendDiagnostic(
                result.parityValidation,
                DiagnosticCode::ReloadFailed,
                result.parityValidation.errorMessage,
                -1,
                roundTripManifestPath);
            appendDiagnostics(result, result.parityValidation);
            result.errorMessage = result.parityValidation.errorMessage;
            return result;
        }

        result.parityValidation.manifestPath = roundTripManifestPath;
        const XpArtLoader::XpSequence& reloaded = result.reloadResult.sequence;
        result.parityValidation = validateLoadedSequence(reloaded, roundTripManifestPath, options);

        if (sequence.getFrameCount() != reloaded.getFrameCount())
        {
            appendDiagnostic(
                result.parityValidation,
                DiagnosticCode::ReloadFrameCountMismatch,
                "Reloaded sequence frame count does not match the source sequence.",
                -1,
                roundTripManifestPath);
        }

        compareString(
            result.parityValidation,
            DiagnosticCode::SequenceNameMismatch,
            sequence.metadata.name,
            reloaded.metadata.name,
            roundTripManifestPath,
            "Sequence name");

        compareString(
            result.parityValidation,
            DiagnosticCode::SequenceLabelMismatch,
            sequence.metadata.sequenceLabel,
            reloaded.metadata.sequenceLabel,
            roundTripManifestPath,
            "Sequence label");

        compareOptionalBool(
            result.parityValidation,
            DiagnosticCode::SequenceLoopMismatch,
            sequence.metadata.loop,
            reloaded.metadata.loop,
            roundTripManifestPath,
            "Sequence loop flag");

        compareOptionalInt(
            result.parityValidation,
            DiagnosticCode::SequenceDefaultDurationMismatch,
            sequence.metadata.defaultFrameDurationMilliseconds,
            reloaded.metadata.defaultFrameDurationMilliseconds,
            roundTripManifestPath,
            "Sequence default frame duration");

        compareOptionalInt(
            result.parityValidation,
            DiagnosticCode::SequenceDefaultFpsMismatch,
            sequence.metadata.defaultFramesPerSecond,
            reloaded.metadata.defaultFramesPerSecond,
            roundTripManifestPath,
            "Sequence default FPS");

        compareOptionalEnum(
            result.parityValidation,
            DiagnosticCode::SequenceDefaultCompositeMismatch,
            sequence.metadata.defaultCompositeMode,
            reloaded.metadata.defaultCompositeMode,
            roundTripManifestPath,
            "Sequence default composite mode",
            &XpArtLoader::toString);

        compareOptionalEnum(
            result.parityValidation,
            DiagnosticCode::SequenceDefaultVisibleLayerModeMismatch,
            sequence.metadata.defaultVisibleLayerMode,
            reloaded.metadata.defaultVisibleLayerMode,
            roundTripManifestPath,
            "Sequence default visible-layer mode",
            &XpArtLoader::toString);

        compareIntVector(
            result.parityValidation,
            DiagnosticCode::SequenceDefaultExplicitVisibleLayersMismatch,
            sequence.metadata.defaultExplicitVisibleLayerIndices,
            reloaded.metadata.defaultExplicitVisibleLayerIndices,
            roundTripManifestPath,
            "Sequence default explicit visible layers");

        const std::unordered_map<int, const XpArtLoader::XpFrame*> reloadedFramesByIndex =
            indexFramesByFrameIndex(reloaded);

        std::unordered_map<int, std::string> exportedFramePathsByIndex;
        for (const XpArtExporter::FrameFileRecord& record : result.exportResult.frameFiles)
        {
            exportedFramePathsByIndex[record.frameIndex] = tryCanonicalizePath(record.path);
        }

        for (const XpArtLoader::XpFrame& sourceFrame : sequence.frames)
        {
            const auto reloadedIt = reloadedFramesByIndex.find(sourceFrame.frameIndex);
            if (reloadedIt == reloadedFramesByIndex.end() || reloadedIt->second == nullptr)
            {
                appendDiagnostic(
                    result.parityValidation,
                    DiagnosticCode::MissingReloadedFrame,
                    "Reloaded sequence is missing a frame present in the source sequence.",
                    sourceFrame.frameIndex,
                    roundTripManifestPath,
                    sourceFrame.sourcePath);
                continue;
            }

            const XpArtLoader::XpFrame& reloadedFrame = *reloadedIt->second;
            compareString(
                result.parityValidation,
                DiagnosticCode::FrameLabelMismatch,
                sourceFrame.label,
                reloadedFrame.label,
                roundTripManifestPath,
                "Frame label",
                sourceFrame.frameIndex);

            compareOptionalInt(
                result.parityValidation,
                DiagnosticCode::FrameDurationMismatch,
                sourceFrame.overrides.durationMilliseconds,
                reloadedFrame.overrides.durationMilliseconds,
                roundTripManifestPath,
                "Frame duration override",
                sourceFrame.frameIndex);

            compareOptionalEnum(
                result.parityValidation,
                DiagnosticCode::FrameCompositeMismatch,
                sourceFrame.overrides.compositeMode,
                reloadedFrame.overrides.compositeMode,
                roundTripManifestPath,
                "Frame composite override",
                &XpArtLoader::toString,
                sourceFrame.frameIndex);

            compareOptionalEnum(
                result.parityValidation,
                DiagnosticCode::FrameVisibleLayerModeMismatch,
                sourceFrame.overrides.visibleLayerMode,
                reloadedFrame.overrides.visibleLayerMode,
                roundTripManifestPath,
                "Frame visible-layer override",
                &XpArtLoader::toString,
                sourceFrame.frameIndex);

            compareIntVector(
                result.parityValidation,
                DiagnosticCode::FrameExplicitVisibleLayersMismatch,
                sourceFrame.overrides.explicitVisibleLayerIndices,
                reloadedFrame.overrides.explicitVisibleLayerIndices,
                roundTripManifestPath,
                "Frame explicit visible layers",
                sourceFrame.frameIndex);

            if (options.requireFrameDocumentReferences && !reloadedFrame.hasDocument())
            {
                appendDiagnostic(
                    result.parityValidation,
                    DiagnosticCode::ReloadedFrameDocumentMissing,
                    "Reloaded frame is missing its retained document reference.",
                    sourceFrame.frameIndex,
                    roundTripManifestPath,
                    reloadedFrame.sourcePath);
            }

            if (options.compareResolvedFrameSourcePaths)
            {
                const auto exportedPathIt = exportedFramePathsByIndex.find(sourceFrame.frameIndex);
                const std::string expectedCanonical =
                    exportedPathIt != exportedFramePathsByIndex.end()
                    ? exportedPathIt->second
                    : tryCanonicalizePath(sourceFrame.sourcePath);
                const std::string actualCanonical = tryCanonicalizePath(reloadedFrame.sourcePath);
                if (!expectedCanonical.empty() && !actualCanonical.empty() && expectedCanonical != actualCanonical)
                {
                    appendDiagnostic(
                        result.parityValidation,
                        DiagnosticCode::FrameResolvedSourcePathMismatch,
                        "Reloaded frame source path resolved to a different XP asset than the manifest export referenced.",
                        sourceFrame.frameIndex,
                        roundTripManifestPath,
                        reloadedFrame.sourcePath);
                }
            }
        }

        const std::unordered_map<int, const XpArtLoader::XpFrame*> sourceFramesByIndex =
            indexFramesByFrameIndex(sequence);
        for (const XpArtLoader::XpFrame& reloadedFrame : reloaded.frames)
        {
            if (sourceFramesByIndex.find(reloadedFrame.frameIndex) == sourceFramesByIndex.end())
            {
                appendDiagnostic(
                    result.parityValidation,
                    DiagnosticCode::UnexpectedReloadedFrame,
                    "Reloaded sequence contains a frame index that was not present in the source sequence.",
                    reloadedFrame.frameIndex,
                    roundTripManifestPath,
                    reloadedFrame.sourcePath);
            }
        }

        result.parityValidation.success = !hasErrors(result.parityValidation.diagnostics);
        if (!result.parityValidation.success && result.parityValidation.errorMessage.empty())
        {
            result.parityValidation.errorMessage = "Round-trip parity validation reported one or more errors.";
        }

        appendDiagnostics(result, result.parityValidation);
        result.success =
            result.sourceValidation.success &&
            result.exportValidation.success &&
            result.parityValidation.success;

        if (!result.success && result.errorMessage.empty())
        {
            result.errorMessage = "XP sequence round-trip validation reported one or more errors.";
        }

        return result;
    }

    RoundTripResult roundTripSequenceFile(
        const std::string& sourceManifestPath,
        const std::string& roundTripManifestPath,
        const ValidationOptions& options)
    {
        RoundTripResult result;
        result.sourceManifestPath = sourceManifestPath;
        result.roundTripManifestPath = roundTripManifestPath;
        result.sourceLoadResult = XpSequenceLoader::loadFromFile(
            sourceManifestPath,
            options.reloadLoadOptions);

        if (!result.sourceLoadResult.success)
        {
            result.errorMessage = result.sourceLoadResult.errorMessage.empty()
                ? std::string("Failed to load the source .xpseq manifest.")
                : result.sourceLoadResult.errorMessage;
            appendDiagnostic(
                result,
                DiagnosticCode::ReloadFailed,
                result.errorMessage,
                -1,
                sourceManifestPath);
            return result;
        }

        RoundTripResult loadedResult = roundTripSequence(
            result.sourceLoadResult.sequence,
            sourceManifestPath,
            roundTripManifestPath,
            options);
        loadedResult.sourceLoadResult = std::move(result.sourceLoadResult);
        return loadedResult;
    }

    const Diagnostic* getFirstDiagnosticByCode(
        const ValidationResult& result,
        const DiagnosticCode code)
    {
        const auto it = std::find_if(
            result.diagnostics.begin(),
            result.diagnostics.end(),
            [code](const Diagnostic& diagnostic)
            {
                return diagnostic.code == code;
            });

        return it != result.diagnostics.end() ? &(*it) : nullptr;
    }

    const Diagnostic* getFirstDiagnosticByCode(
        const RoundTripResult& result,
        const DiagnosticCode code)
    {
        const auto it = std::find_if(
            result.diagnostics.begin(),
            result.diagnostics.end(),
            [code](const Diagnostic& diagnostic)
            {
                return diagnostic.code == code;
            });

        return it != result.diagnostics.end() ? &(*it) : nullptr;
    }

    std::string formatValidationSummary(const ValidationResult& result)
    {
        if (result.success)
        {
            return "XP sequence validation succeeded with no blocking diagnostics.";
        }

        std::ostringstream stream;
        stream << "XP sequence validation failed";
        if (!result.manifestPath.empty())
        {
            stream << " for '" << result.manifestPath << "'";
        }
        stream << '.';

        if (!result.errorMessage.empty())
        {
            stream << ' ' << result.errorMessage;
        }
        else if (!result.diagnostics.empty())
        {
            stream << ' ' << result.diagnostics.front().message;
        }

        return stream.str();
    }

    std::string formatRoundTripSummary(const RoundTripResult& result)
    {
        if (result.success)
        {
            return "XP sequence round-trip validation succeeded with semantic parity preserved.";
        }

        std::ostringstream stream;
        stream << "XP sequence round-trip validation failed";
        if (!result.roundTripManifestPath.empty())
        {
            stream << " for '" << result.roundTripManifestPath << "'";
        }
        stream << '.';

        if (!result.errorMessage.empty())
        {
            stream << ' ' << result.errorMessage;
        }
        else if (!result.diagnostics.empty())
        {
            stream << ' ' << result.diagnostics.front().message;
        }

        return stream.str();
    }

    const char* toString(const DiagnosticCode code)
    {
        switch (code)
        {
        case DiagnosticCode::None:
            return "None";
        case DiagnosticCode::SequenceInvalid:
            return "SequenceInvalid";
        case DiagnosticCode::DuplicateFrameIndices:
            return "DuplicateFrameIndices";
        case DiagnosticCode::NonContiguousFrameIndices:
            return "NonContiguousFrameIndices";
        case DiagnosticCode::FrameStorageOrderMismatch:
            return "FrameStorageOrderMismatch";
        case DiagnosticCode::MissingFrameDocumentReference:
            return "MissingFrameDocumentReference";
        case DiagnosticCode::MissingFrameSourcePath:
            return "MissingFrameSourcePath";
        case DiagnosticCode::MissingFrameSourceOnDisk:
            return "MissingFrameSourceOnDisk";
        case DiagnosticCode::ExportManifestPathRequired:
            return "ExportManifestPathRequired";
        case DiagnosticCode::ExportModeMustBeManifestFirst:
            return "ExportModeMustBeManifestFirst";
        case DiagnosticCode::ExportFailed:
            return "ExportFailed";
        case DiagnosticCode::ExportLossyWarning:
            return "ExportLossyWarning";
        case DiagnosticCode::ExportMixedLinkedAndGeneratedFrames:
            return "ExportMixedLinkedAndGeneratedFrames";
        case DiagnosticCode::ExportSourcePathPortabilityRisk:
            return "ExportSourcePathPortabilityRisk";
        case DiagnosticCode::ExportSuspiciousFrameIndexing:
            return "ExportSuspiciousFrameIndexing";
        case DiagnosticCode::ExportRedundantMetadata:
            return "ExportRedundantMetadata";
        case DiagnosticCode::ExportConflictingMetadata:
            return "ExportConflictingMetadata";
        case DiagnosticCode::ManifestFrameCountMismatch:
            return "ManifestFrameCountMismatch";
        case DiagnosticCode::ManifestFrameIndexMismatch:
            return "ManifestFrameIndexMismatch";
        case DiagnosticCode::ManifestFrameSourceMissing:
            return "ManifestFrameSourceMissing";
        case DiagnosticCode::ManifestFrameSourceNotRelative:
            return "ManifestFrameSourceNotRelative";
        case DiagnosticCode::ManifestFrameSourceEscapesManifestDirectory:
            return "ManifestFrameSourceEscapesManifestDirectory";
        case DiagnosticCode::ManifestFrameSourceResolutionMismatch:
            return "ManifestFrameSourceResolutionMismatch";
        case DiagnosticCode::ReloadFailed:
            return "ReloadFailed";
        case DiagnosticCode::ReloadFrameCountMismatch:
            return "ReloadFrameCountMismatch";
        case DiagnosticCode::MissingReloadedFrame:
            return "MissingReloadedFrame";
        case DiagnosticCode::UnexpectedReloadedFrame:
            return "UnexpectedReloadedFrame";
        case DiagnosticCode::FrameLabelMismatch:
            return "FrameLabelMismatch";
        case DiagnosticCode::FrameResolvedSourcePathMismatch:
            return "FrameResolvedSourcePathMismatch";
        case DiagnosticCode::SequenceNameMismatch:
            return "SequenceNameMismatch";
        case DiagnosticCode::SequenceLabelMismatch:
            return "SequenceLabelMismatch";
        case DiagnosticCode::SequenceLoopMismatch:
            return "SequenceLoopMismatch";
        case DiagnosticCode::SequenceDefaultDurationMismatch:
            return "SequenceDefaultDurationMismatch";
        case DiagnosticCode::SequenceDefaultFpsMismatch:
            return "SequenceDefaultFpsMismatch";
        case DiagnosticCode::SequenceDefaultCompositeMismatch:
            return "SequenceDefaultCompositeMismatch";
        case DiagnosticCode::SequenceDefaultVisibleLayerModeMismatch:
            return "SequenceDefaultVisibleLayerModeMismatch";
        case DiagnosticCode::SequenceDefaultExplicitVisibleLayersMismatch:
            return "SequenceDefaultExplicitVisibleLayersMismatch";
        case DiagnosticCode::FrameDurationMismatch:
            return "FrameDurationMismatch";
        case DiagnosticCode::FrameCompositeMismatch:
            return "FrameCompositeMismatch";
        case DiagnosticCode::FrameVisibleLayerModeMismatch:
            return "FrameVisibleLayerModeMismatch";
        case DiagnosticCode::FrameExplicitVisibleLayersMismatch:
            return "FrameExplicitVisibleLayersMismatch";
        case DiagnosticCode::ReloadedFrameDocumentMissing:
            return "ReloadedFrameDocumentMissing";
        case DiagnosticCode::LoadedSourcePathPortabilityRisk:
            return "LoadedSourcePathPortabilityRisk";
        case DiagnosticCode::LoadedRedundantMetadata:
            return "LoadedRedundantMetadata";
        case DiagnosticCode::LoadedConflictingMetadata:
            return "LoadedConflictingMetadata";
        default:
            return "Unknown";
        }
    }
}
