#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Rendering/Objects/XpArtLoader.h"
#include "Rendering/Objects/XpDiagnosticsFormatting.h"
#include "Rendering/Objects/XpSequenceLoader.h"
#include "Rendering/Objects/XpSequenceValidation.h"

namespace XpSequenceInspection
{
    struct FrameInspection
    {
        int ordinal = -1;
        int frameIndex = -1;
        std::string label;

        bool hasDocument = false;
        int documentWidth = 0;
        int documentHeight = 0;
        int documentLayerCount = 0;

        std::string sourcePath;
        bool hasSourcePath = false;
        bool sourcePathIsRelative = false;
        bool sourcePathEscapesManifestDirectory = false;
        bool sourceExistsOnDisk = false;
        std::string resolvedSourcePath;

        bool hasDurationOverride = false;
        std::optional<int> durationOverrideMilliseconds;
        std::optional<int> resolvedDurationMilliseconds;

        bool hasCompositeOverride = false;
        XpArtLoader::XpCompositeMode resolvedCompositeMode = XpArtLoader::XpCompositeMode::Phase45BCompatible;

        bool hasVisibleLayerModeOverride = false;
        XpArtLoader::XpVisibleLayerMode resolvedVisibleLayerMode = XpArtLoader::XpVisibleLayerMode::UseDocumentVisibility;

        bool hasExplicitVisibleLayerOverride = false;
        std::vector<int> explicitVisibleLayerIndices;
        std::vector<int> resolvedExplicitVisibleLayerIndices;

        std::string documentSummary;
        std::string visibilityCompositeSummary;
        std::vector<std::string> layerDetails;
        int resolvedVisibleLayerCount = 0;
        int resolvedHiddenLayerCount = 0;

        bool hasAnyRetainedOverride() const;
    };

    struct SequencePlaybackMetadata
    {
        bool isInspectable = false;
        int initialFrameOrdinal = -1;
        int initialFrameIndex = -1;
        int frameCount = 0;
        std::optional<bool> loop;
        std::optional<int> defaultFrameDurationMilliseconds;
        std::optional<int> defaultFramesPerSecond;
        std::optional<XpArtLoader::XpCompositeMode> defaultCompositeMode;
        std::optional<XpArtLoader::XpVisibleLayerMode> defaultVisibleLayerMode;
        bool hasFrameTimingOverrides = false;
        bool hasFrameVisibilityOrCompositeOverrides = false;
        bool hasLinkedFrameSources = false;
        bool hasRetainedFrameDocuments = false;
        std::vector<int> orderedFrameIndices;
    };

    struct InspectionReport
    {
        bool hasSequence = false;
        bool sequenceValid = false;

        std::string manifestPath;
        std::string manifestDirectory;
        int resolvedVersion = 0;

        int frameCount = 0;
        bool hasUniqueFrameIndices = false;
        bool hasContiguousFrameIndicesStartingAtZero = false;
        bool framesStoredInFrameIndexOrder = false;

        std::string sequenceName;
        std::string sequenceLabel;
        std::optional<bool> loop;
        std::optional<int> defaultFrameDurationMilliseconds;
        std::optional<int> defaultFramesPerSecond;
        std::optional<XpArtLoader::XpCompositeMode> defaultCompositeMode;
        std::optional<XpArtLoader::XpVisibleLayerMode> defaultVisibleLayerMode;
        std::vector<int> defaultExplicitVisibleLayerIndices;

        int framesWithDocuments = 0;
        int framesWithSourcePaths = 0;
        int framesWithAnyOverrides = 0;
        int framesMissingResolvedSources = 0;
        int framesEscapingManifestDirectory = 0;

        std::vector<FrameInspection> frames;
        std::vector<XpSequenceLoader::Diagnostic> loadDiagnostics;
        std::vector<XpSequenceValidation::Diagnostic> validationDiagnostics;
        SequencePlaybackMetadata playbackMetadata;
    };

    InspectionReport inspectSequence(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestPath,
        int resolvedVersion = 0,
        const std::vector<XpSequenceLoader::Diagnostic>* loadDiagnostics = nullptr,
        const std::vector<XpSequenceValidation::Diagnostic>* validationDiagnostics = nullptr);

    InspectionReport inspectLoadResult(
        const XpSequenceLoader::LoadResult& loadResult,
        const XpSequenceValidation::ValidationResult* validationResult = nullptr);

    SequencePlaybackMetadata buildPlaybackMetadata(
        const XpArtLoader::XpSequence& sequence);

    std::string formatInspectionSummary(const InspectionReport& report);
    std::string formatManifestFieldSummary(const InspectionReport& report);
    std::string formatSourceResolutionSummary(const InspectionReport& report);
    std::string formatOverrideSummary(const InspectionReport& report);
    std::string formatFrameSummary(const InspectionReport& report, int ordinal);
    std::string formatPlaybackHookSummary(const InspectionReport& report);
}
