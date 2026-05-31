#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/XpArtLoader.h"

namespace XpSequenceLoader
{
    /*
        .xpseq is the manifest-first retained XP sequence format for the
        current roadmap stage. The manifest is UTF-8 text and each frame
        continues to load through the existing .xp retained-document loader.

        Supported grammar:

            xpseq 1
            name = "Intro"
            loop = true
            default_frame_duration_ms = 100
            default_fps = 10
            default_composite = Phase45BCompatible
            default_visible_layers = UseDocumentVisibility
            default_explicit_visible_layers = 0,1

            frame {
                index = 0
                source = "frames/0000.xp"
                label = "Frame 0"
                duration_ms = 100
                composite = StrictOpaqueOverwrite
                visible_layers = UseExplicitVisibleLayerList
                explicit_visible_layers = 0,2
            }

        The loader also accepts the existing single-line legacy frame syntax:
            frame index=0 source="frames/0000.xp" ...
    */
    enum class DiagnosticCode
    {
        None,
        EmptyManifest,
        InvalidUtf8,
        InvalidHeader,
        UnsupportedVersion,
        InvalidDirective,
        InvalidBooleanValue,
        InvalidFramesPerSecond,
        InvalidFrameDirective,
        InvalidFrameBlock,
        UnterminatedFrameBlock,
        MissingFrameIndex,
        InvalidFrameIndex,
        DuplicateFrameIndex,
        MissingFrameSource,
        MissingFrame,
        BadRelativePath,
        InvalidDurationMilliseconds,
        InvalidCompositeMode,
        InvalidVisibleLayerMode,
        InvalidExplicitVisibleLayerList,
        XpFrameLoadFailed,
        UnknownDirectiveIgnored,
        UnknownFrameDirectiveIgnored,
        UnknownDirectivePreserved,
        UnknownFrameDirectivePreserved
    };

    struct Diagnostic
    {
        DiagnosticCode code = DiagnosticCode::None;
        bool isError = true;
        int lineNumber = 0;
        int frameIndex = -1;
        std::string manifestPath;
        std::string sourcePath;
        std::string message;

        bool isValid() const
        {
            return code != DiagnosticCode::None;
        }
    };

    enum class UnknownFieldPolicy
    {
        StrictError,
        WarnAndIgnore,
        PreserveInMetadata
    };

    struct LoadOptions
    {
        XpArtLoader::LoadOptions xpLoadOptions;
        bool requireContiguousFrameIndices = true;
        bool sortFramesByFrameIndex = true;
        UnknownFieldPolicy unknownFieldPolicy = UnknownFieldPolicy::StrictError;
    };

    struct LoadResult
    {
        XpArtLoader::XpSequence sequence;
        bool success = false;
        std::string manifestPath;
        int resolvedVersion = 0;
        int resolvedFrameCount = 0;
        std::vector<Diagnostic> diagnostics;
        std::string errorMessage;
    };

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options = {});
    LoadResult loadFromString(
        std::string_view manifestUtf8,
        const std::string& manifestPath,
        const LoadOptions& options = {});

    const Diagnostic* getFirstDiagnosticByCode(
        const LoadResult& result,
        DiagnosticCode code);

    std::string formatLoadError(const LoadResult& result);
    std::string formatLoadSuccess(const LoadResult& result);

    const char* toString(DiagnosticCode code);
    const char* toString(UnknownFieldPolicy policy);
}
