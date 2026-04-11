#include "Rendering/Objects/XpSequenceInspection.h"

#include <algorithm>
#include <filesystem>
#include <sstream>

namespace
{
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

    std::string normalizePathString(const std::filesystem::path& path)
    {
        return path.lexically_normal().string();
    }

    std::string resolveSourcePath(
        const std::string& manifestPath,
        const std::string& sourcePath)
    {
        if (sourcePath.empty())
        {
            return std::string();
        }

        const std::filesystem::path source(sourcePath);
        if (source.is_absolute())
        {
            return normalizePathString(source);
        }

        if (manifestPath.empty())
        {
            return normalizePathString(source);
        }

        const std::filesystem::path manifestDirectory =
            std::filesystem::path(manifestPath).parent_path();
        return normalizePathString(manifestDirectory / source);
    }

    bool pathEscapesManifestDirectory(
        const std::string& manifestPath,
        const std::string& sourcePath)
    {
        if (manifestPath.empty() || sourcePath.empty())
        {
            return false;
        }

        const std::filesystem::path source(sourcePath);
        if (source.is_absolute())
        {
            return false;
        }

        const std::filesystem::path normalized = source.lexically_normal();
        if (normalized.empty())
        {
            return false;
        }

        const std::string text = normalized.generic_string();
        return text == ".." || text.rfind("../", 0) == 0;
    }

    XpSequenceInspection::FrameInspection buildFrameInspection(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestPath,
        const int ordinal,
        const XpArtLoader::XpFrame& frame)
    {
        XpSequenceInspection::FrameInspection inspection;
        inspection.ordinal = ordinal;
        inspection.frameIndex = frame.frameIndex;
        inspection.label = frame.label;

        inspection.sourcePath = frame.sourcePath;
        inspection.hasSourcePath = frame.hasSourcePath();
        inspection.sourcePathIsRelative =
            inspection.hasSourcePath &&
            !std::filesystem::path(frame.sourcePath).is_absolute();
        inspection.sourcePathEscapesManifestDirectory =
            pathEscapesManifestDirectory(manifestPath, frame.sourcePath);
        inspection.resolvedSourcePath = resolveSourcePath(manifestPath, frame.sourcePath);

        if (!inspection.resolvedSourcePath.empty())
        {
            std::error_code error;
            inspection.sourceExistsOnDisk =
                std::filesystem::exists(std::filesystem::path(inspection.resolvedSourcePath), error) &&
                !error;
        }

        if (const XpArtLoader::XpDocument* document = frame.getDocument())
        {
            inspection.hasDocument = true;
            inspection.documentWidth = document->width;
            inspection.documentHeight = document->height;
            inspection.documentLayerCount = document->getLayerCount();
        }

        inspection.hasDurationOverride = frame.overrides.durationMilliseconds.has_value();
        inspection.durationOverrideMilliseconds = frame.overrides.durationMilliseconds;
        inspection.resolvedDurationMilliseconds =
            frame.resolveDurationMilliseconds(sequence.metadata);

        inspection.hasCompositeOverride = frame.overrides.compositeMode.has_value();
        inspection.resolvedCompositeMode = frame.resolveCompositeMode(sequence.metadata);

        inspection.hasVisibleLayerModeOverride = frame.overrides.visibleLayerMode.has_value();
        inspection.resolvedVisibleLayerMode =
            frame.resolveVisibleLayerMode(sequence.metadata);

        inspection.hasExplicitVisibleLayerOverride =
            frame.overrides.usesExplicitVisibleLayerList();
        inspection.explicitVisibleLayerIndices = frame.overrides.explicitVisibleLayerIndices;
        inspection.resolvedExplicitVisibleLayerIndices =
            frame.resolveExplicitVisibleLayerIndices(sequence.metadata);

        if (const XpArtLoader::XpDocument* document = frame.getDocument())
        {
            inspection.documentSummary =
                XpDiagnosticsFormatting::formatXpDocumentSummary(*document);
            inspection.visibilityCompositeSummary =
                XpDiagnosticsFormatting::formatXpVisibilityCompositeSummary(
                    frame,
                    sequence.metadata);

            const std::vector<XpDiagnosticsFormatting::LayerInspection> layerInspections =
                XpDiagnosticsFormatting::inspectLayers(
                    *document,
                    frame,
                    sequence.metadata);

            inspection.layerDetails.reserve(layerInspections.size());
            for (const XpDiagnosticsFormatting::LayerInspection& layerInspection : layerInspections)
            {
                inspection.layerDetails.push_back(
                    XpDiagnosticsFormatting::formatXpLayerDetails(
                        *document,
                        frame,
                        sequence.metadata,
                        layerInspection.layerIndex));

                if (layerInspection.resolvedVisible)
                {
                    ++inspection.resolvedVisibleLayerCount;
                }
                else
                {
                    ++inspection.resolvedHiddenLayerCount;
                }
            }
        }

        return inspection;
    }
}

namespace XpSequenceInspection
{
    bool FrameInspection::hasAnyRetainedOverride() const
    {
        return hasDurationOverride ||
            hasCompositeOverride ||
            hasVisibleLayerModeOverride ||
            hasExplicitVisibleLayerOverride;
    }

    InspectionReport inspectSequence(
        const XpArtLoader::XpSequence& sequence,
        const std::string& manifestPath,
        const int resolvedVersion,
        const std::vector<XpSequenceLoader::Diagnostic>* loadDiagnostics,
        const std::vector<XpSequenceValidation::Diagnostic>* validationDiagnostics)
    {
        InspectionReport report;
        report.hasSequence = true;
        report.sequenceValid = sequence.isValid();
        report.manifestPath = manifestPath;
        report.manifestDirectory = std::filesystem::path(manifestPath).parent_path().string();
        report.resolvedVersion = resolvedVersion;

        report.frameCount = sequence.getFrameCount();
        report.hasUniqueFrameIndices = sequence.hasUniqueFrameIndices();
        report.hasContiguousFrameIndicesStartingAtZero =
            sequence.hasContiguousFrameIndicesStartingAtZero();
        report.framesStoredInFrameIndexOrder =
            sequence.areFramesStoredInFrameIndexOrder();

        report.sequenceName = sequence.metadata.name;
        report.sequenceLabel = sequence.metadata.sequenceLabel;
        report.loop = sequence.metadata.loop;
        report.defaultFrameDurationMilliseconds =
            sequence.metadata.defaultFrameDurationMilliseconds;
        report.defaultFramesPerSecond = sequence.metadata.defaultFramesPerSecond;
        report.defaultCompositeMode = sequence.metadata.defaultCompositeMode;
        report.defaultVisibleLayerMode = sequence.metadata.defaultVisibleLayerMode;
        report.defaultExplicitVisibleLayerIndices =
            sequence.metadata.defaultExplicitVisibleLayerIndices;

        report.frames.reserve(sequence.frames.size());
        for (std::size_t ordinal = 0; ordinal < sequence.frames.size(); ++ordinal)
        {
            FrameInspection inspection = buildFrameInspection(
                sequence,
                manifestPath,
                static_cast<int>(ordinal),
                sequence.frames[ordinal]);

            if (inspection.hasDocument)
            {
                ++report.framesWithDocuments;
            }

            if (inspection.hasSourcePath)
            {
                ++report.framesWithSourcePaths;
            }

            if (inspection.hasAnyRetainedOverride())
            {
                ++report.framesWithAnyOverrides;
            }

            if (inspection.hasSourcePath && !inspection.sourceExistsOnDisk)
            {
                ++report.framesMissingResolvedSources;
            }

            if (inspection.sourcePathEscapesManifestDirectory)
            {
                ++report.framesEscapingManifestDirectory;
            }

            report.frames.push_back(std::move(inspection));
        }

        if (loadDiagnostics != nullptr)
        {
            report.loadDiagnostics = *loadDiagnostics;
        }

        if (validationDiagnostics != nullptr)
        {
            report.validationDiagnostics = *validationDiagnostics;
        }

        report.playbackMetadata = buildPlaybackMetadata(sequence);
        return report;
    }

    InspectionReport inspectLoadResult(
        const XpSequenceLoader::LoadResult& loadResult,
        const XpSequenceValidation::ValidationResult* validationResult)
    {
        return inspectSequence(
            loadResult.sequence,
            loadResult.manifestPath,
            loadResult.resolvedVersion,
            &loadResult.diagnostics,
            validationResult != nullptr ? &validationResult->diagnostics : nullptr);
    }

    SequencePlaybackMetadata buildPlaybackMetadata(
        const XpArtLoader::XpSequence& sequence)
    {
        SequencePlaybackMetadata metadata;
        metadata.isInspectable = sequence.isValid();
        metadata.frameCount = sequence.getFrameCount();
        metadata.loop = sequence.metadata.loop;
        metadata.defaultFrameDurationMilliseconds =
            sequence.metadata.defaultFrameDurationMilliseconds;
        metadata.defaultFramesPerSecond = sequence.metadata.defaultFramesPerSecond;
        metadata.defaultCompositeMode = sequence.metadata.defaultCompositeMode;
        metadata.defaultVisibleLayerMode = sequence.metadata.defaultVisibleLayerMode;

        metadata.orderedFrameIndices.reserve(sequence.frames.size());
        for (std::size_t ordinal = 0; ordinal < sequence.frames.size(); ++ordinal)
        {
            const XpArtLoader::XpFrame& frame = sequence.frames[ordinal];
            metadata.orderedFrameIndices.push_back(frame.frameIndex);
            metadata.hasFrameTimingOverrides =
                metadata.hasFrameTimingOverrides || frame.overrides.durationMilliseconds.has_value();
            metadata.hasFrameVisibilityOrCompositeOverrides =
                metadata.hasFrameVisibilityOrCompositeOverrides ||
                frame.overrides.compositeMode.has_value() ||
                frame.overrides.visibleLayerMode.has_value() ||
                frame.overrides.usesExplicitVisibleLayerList();
            metadata.hasLinkedFrameSources =
                metadata.hasLinkedFrameSources || frame.hasSourcePath();
            metadata.hasRetainedFrameDocuments =
                metadata.hasRetainedFrameDocuments || frame.hasDocument();
        }

        if (const XpArtLoader::XpFrame* defaultFrame = sequence.getDefaultFrame())
        {
            metadata.initialFrameIndex = defaultFrame->frameIndex;
            for (std::size_t ordinal = 0; ordinal < sequence.frames.size(); ++ordinal)
            {
                if (&sequence.frames[ordinal] == defaultFrame)
                {
                    metadata.initialFrameOrdinal = static_cast<int>(ordinal);
                    break;
                }
            }
        }

        return metadata;
    }

    std::string formatInspectionSummary(const InspectionReport& report)
    {
        if (!report.hasSequence)
        {
            return "XP sequence inspection: unavailable";
        }

        std::ostringstream stream;
        stream << "XP sequence inspection: frames=" << report.frameCount
            << ", uniqueIndices=" << (report.hasUniqueFrameIndices ? "true" : "false")
            << ", contiguous=" << (report.hasContiguousFrameIndicesStartingAtZero ? "true" : "false")
            << ", storedInOrder=" << (report.framesStoredInFrameIndexOrder ? "true" : "false")
            << ", retainedDocs=" << report.framesWithDocuments << '/' << report.frameCount
            << ", sourcePaths=" << report.framesWithSourcePaths << '/' << report.frameCount
            << ", overrideFrames=" << report.framesWithAnyOverrides
            << ", loadDiagnostics=" << report.loadDiagnostics.size()
            << ", validationDiagnostics=" << report.validationDiagnostics.size();

        if (!report.manifestPath.empty())
        {
            stream << ", manifest=" << report.manifestPath;
        }

        return stream.str();
    }

    std::string formatManifestFieldSummary(const InspectionReport& report)
    {
        if (!report.hasSequence)
        {
            return "Manifest fields: unavailable";
        }

        std::ostringstream stream;
        stream << "Manifest fields: version=" << report.resolvedVersion;

        if (!report.sequenceName.empty())
        {
            stream << ", name=" << report.sequenceName;
        }

        if (!report.sequenceLabel.empty())
        {
            stream << ", label=" << report.sequenceLabel;
        }

        if (report.loop.has_value())
        {
            stream << ", loop=" << (*report.loop ? "true" : "false");
        }

        if (report.defaultFrameDurationMilliseconds.has_value())
        {
            stream << ", defaultDurationMs="
                << *report.defaultFrameDurationMilliseconds;
        }

        if (report.defaultFramesPerSecond.has_value())
        {
            stream << ", defaultFps=" << *report.defaultFramesPerSecond;
        }

        if (report.defaultCompositeMode.has_value())
        {
            stream << ", defaultComposite="
                << XpArtLoader::toString(*report.defaultCompositeMode);
        }

        if (report.defaultVisibleLayerMode.has_value())
        {
            stream << ", defaultVisibleLayers="
                << XpArtLoader::toString(*report.defaultVisibleLayerMode);
        }

        if (!report.defaultExplicitVisibleLayerIndices.empty())
        {
            stream << ", defaultExplicitVisibleLayers="
                << joinIntegers(report.defaultExplicitVisibleLayerIndices);
        }

        return stream.str();
    }

    std::string formatSourceResolutionSummary(const InspectionReport& report)
    {
        if (!report.hasSequence)
        {
            return "Source resolution: unavailable";
        }

        std::ostringstream stream;
        stream << "Source resolution: manifestDir=";
        if (report.manifestDirectory.empty())
        {
            stream << "<none>";
        }
        else
        {
            stream << report.manifestDirectory;
        }

        stream << ", linkedFrames=" << report.framesWithSourcePaths
            << ", retainedFrames=" << report.framesWithDocuments
            << ", missingResolvedSources=" << report.framesMissingResolvedSources
            << ", manifestEscapes=" << report.framesEscapingManifestDirectory;

        return stream.str();
    }

    std::string formatOverrideSummary(const InspectionReport& report)
    {
        if (!report.hasSequence)
        {
            return "Resolved overrides: unavailable";
        }

        int durationOverrideCount = 0;
        int compositeOverrideCount = 0;
        int visibilityOverrideCount = 0;
        int explicitLayerOverrideCount = 0;

        for (const FrameInspection& frame : report.frames)
        {
            durationOverrideCount += frame.hasDurationOverride ? 1 : 0;
            compositeOverrideCount += frame.hasCompositeOverride ? 1 : 0;
            visibilityOverrideCount += frame.hasVisibleLayerModeOverride ? 1 : 0;
            explicitLayerOverrideCount += frame.hasExplicitVisibleLayerOverride ? 1 : 0;
        }

        std::ostringstream stream;
        stream << "Resolved overrides: duration=" << durationOverrideCount
            << ", composite=" << compositeOverrideCount
            << ", visibilityMode=" << visibilityOverrideCount
            << ", explicitVisibleLayers=" << explicitLayerOverrideCount;

        return stream.str();
    }

    std::string formatFrameSummary(const InspectionReport& report, const int ordinal)
    {
        if (ordinal < 0 || ordinal >= static_cast<int>(report.frames.size()))
        {
            return "Frame inspection: unavailable";
        }

        const FrameInspection& frame = report.frames[ordinal];

        std::ostringstream stream;
        stream << "Frame " << frame.ordinal
            << " -> index=" << frame.frameIndex;

        if (!frame.label.empty())
        {
            stream << ", label=" << frame.label;
        }

        if (frame.hasSourcePath)
        {
            stream << ", source=" << frame.sourcePath
                << ", relative=" << (frame.sourcePathIsRelative ? "true" : "false")
                << ", escapesManifestDir=" << (frame.sourcePathEscapesManifestDirectory ? "true" : "false")
                << ", exists=" << (frame.sourceExistsOnDisk ? "true" : "false");
        }

        if (!frame.resolvedSourcePath.empty())
        {
            stream << ", resolvedSource=" << frame.resolvedSourcePath;
        }

        stream << ", retainedDocument=" << (frame.hasDocument ? "true" : "false");
        if (frame.hasDocument)
        {
            stream << ", canvas=" << frame.documentWidth << 'x' << frame.documentHeight
                << ", layers=" << frame.documentLayerCount;
        }

        if (frame.resolvedDurationMilliseconds.has_value())
        {
            stream << ", durationMs=" << *frame.resolvedDurationMilliseconds;
        }

        stream << ", composite=" << XpArtLoader::toString(frame.resolvedCompositeMode)
            << ", visibleLayers=" << XpArtLoader::toString(frame.resolvedVisibleLayerMode)
            << ", hasOverrides=" << (frame.hasAnyRetainedOverride() ? "true" : "false");

        if (!frame.resolvedExplicitVisibleLayerIndices.empty())
        {
            stream << ", explicitVisibleLayers="
                << joinIntegers(frame.resolvedExplicitVisibleLayerIndices);
        }

        return stream.str();
    }

    std::string formatPlaybackHookSummary(const InspectionReport& report)
    {
        const SequencePlaybackMetadata& playback = report.playbackMetadata;
        if (!playback.isInspectable)
        {
            return "Playback hook metadata: unavailable";
        }

        std::ostringstream stream;
        stream << "Playback hook metadata: initialFrameOrdinal=" << playback.initialFrameOrdinal
            << ", initialFrameIndex=" << playback.initialFrameIndex
            << ", frameCount=" << playback.frameCount;

        if (playback.loop.has_value())
        {
            stream << ", loop=" << (*playback.loop ? "true" : "false");
        }

        if (playback.defaultFrameDurationMilliseconds.has_value())
        {
            stream << ", defaultDurationMs="
                << *playback.defaultFrameDurationMilliseconds;
        }

        if (playback.defaultFramesPerSecond.has_value())
        {
            stream << ", defaultFps=" << *playback.defaultFramesPerSecond;
        }

        if (playback.defaultCompositeMode.has_value())
        {
            stream << ", defaultComposite="
                << XpArtLoader::toString(*playback.defaultCompositeMode);
        }

        if (playback.defaultVisibleLayerMode.has_value())
        {
            stream << ", defaultVisibleLayers="
                << XpArtLoader::toString(*playback.defaultVisibleLayerMode);
        }

        stream << ", frameTimingOverrides="
            << (playback.hasFrameTimingOverrides ? "true" : "false")
            << ", visibilityOrCompositeOverrides="
            << (playback.hasFrameVisibilityOrCompositeOverrides ? "true" : "false")
            << ", linkedFrameSources="
            << (playback.hasLinkedFrameSources ? "true" : "false")
            << ", retainedFrameDocuments="
            << (playback.hasRetainedFrameDocuments ? "true" : "false");

        if (!playback.orderedFrameIndices.empty())
        {
            stream << ", orderedFrameIndices="
                << joinIntegers(playback.orderedFrameIndices);
        }

        return stream.str();
    }
}
