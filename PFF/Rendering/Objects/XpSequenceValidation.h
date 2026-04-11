#pragma once

#include <string>
#include <vector>

#include "Rendering/Objects/TextObjectExporter.h"
#include "Rendering/Objects/XpArtExporter.h"
#include "Rendering/Objects/XpSequenceLoader.h"

namespace XpSequenceValidation
{
    enum class DiagnosticCode
    {
        None,
        SequenceInvalid,
        DuplicateFrameIndices,
        NonContiguousFrameIndices,
        FrameStorageOrderMismatch,
        MissingFrameDocumentReference,
        MissingFrameSourcePath,
        MissingFrameSourceOnDisk,
        ExportManifestPathRequired,
        ExportModeMustBeManifestFirst,
        ExportFailed,
        ExportLossyWarning,
        ExportMixedLinkedAndGeneratedFrames,
        ExportSourcePathPortabilityRisk,
        ExportSuspiciousFrameIndexing,
        ExportRedundantMetadata,
        ExportConflictingMetadata,
        ManifestFrameCountMismatch,
        ManifestFrameIndexMismatch,
        ManifestFrameSourceMissing,
        ManifestFrameSourceNotRelative,
        ManifestFrameSourceEscapesManifestDirectory,
        ManifestFrameSourceResolutionMismatch,
        ReloadFailed,
        ReloadFrameCountMismatch,
        MissingReloadedFrame,
        UnexpectedReloadedFrame,
        FrameLabelMismatch,
        FrameResolvedSourcePathMismatch,
        SequenceNameMismatch,
        SequenceLabelMismatch,
        SequenceLoopMismatch,
        SequenceDefaultDurationMismatch,
        SequenceDefaultFpsMismatch,
        SequenceDefaultCompositeMismatch,
        SequenceDefaultVisibleLayerModeMismatch,
        SequenceDefaultExplicitVisibleLayersMismatch,
        FrameDurationMismatch,
        FrameCompositeMismatch,
        FrameVisibleLayerModeMismatch,
        FrameExplicitVisibleLayersMismatch,
        ReloadedFrameDocumentMissing,
        LoadedSourcePathPortabilityRisk,
        LoadedRedundantMetadata,
        LoadedConflictingMetadata
    };

    struct Diagnostic
    {
        DiagnosticCode code = DiagnosticCode::None;
        bool isError = true;
        int frameIndex = -1;
        std::string manifestPath;
        std::string sourcePath;
        std::string message;

        bool isValid() const
        {
            return code != DiagnosticCode::None;
        }
    };

    struct ValidationOptions
    {
        XpSequenceLoader::LoadOptions reloadLoadOptions;
        XpArtExporter::RetainedExportOptions exportOptions;

        bool requireContiguousFrameIndices = true;
        bool requireStoredFramesInFrameIndexOrder = true;
        bool requireFrameDocumentReferences = true;
        bool requireFrameSourcePaths = true;
        bool requireFrameSourceFilesOnDisk = true;

        bool requireRelativeManifestFrameSources = true;
        bool disallowManifestRelativeSourceEscapes = true;
        bool compareResolvedFrameSourcePaths = true;
        bool treatExportLossyWarningsAsErrors = false;
    };

    struct ValidationResult
    {
        bool success = false;
        std::string manifestPath;
        std::vector<Diagnostic> diagnostics;
        std::string errorMessage;
    };

    struct RoundTripResult
    {
        bool success = false;
        std::string sourceManifestPath;
        std::string roundTripManifestPath;

        XpSequenceLoader::LoadResult sourceLoadResult;
        ValidationResult sourceValidation;

        XpArtExporter::RetainedExportResult exportResult;
        ValidationResult exportValidation;

        XpSequenceLoader::LoadResult reloadResult;
        ValidationResult parityValidation;

        std::vector<Diagnostic> diagnostics;
        std::string errorMessage;
    };

    ValidationResult validateLoadedSequence(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestPath,
        const ValidationOptions& options = {});

    RoundTripResult roundTripSequence(
        const XpArtLoader::XpSequence& sequence,
        const std::string& sourceManifestPath,
        const std::string& roundTripManifestPath,
        const ValidationOptions& options = {});

    RoundTripResult roundTripSequenceFile(
        const std::string& sourceManifestPath,
        const std::string& roundTripManifestPath,
        const ValidationOptions& options = {});

    const Diagnostic* getFirstDiagnosticByCode(
        const ValidationResult& result,
        DiagnosticCode code);

    const Diagnostic* getFirstDiagnosticByCode(
        const RoundTripResult& result,
        DiagnosticCode code);

    std::string formatValidationSummary(const ValidationResult& result);
    std::string formatRoundTripSummary(const RoundTripResult& result);

    const char* toString(DiagnosticCode code);
}
