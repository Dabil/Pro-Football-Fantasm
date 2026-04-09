#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/SauceSupport.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Styles/Style.h"

namespace BinaryArtLoader
{
    enum class FileType
    {
        Auto,
        Unknown,
        Bin,
        XBin,
        Adf
    };

    enum class XBinCompressionSupport
    {
        DecodeCompressed,
        RejectCompressed
    };

    enum class LoadWarningCode
    {
        None,

        SauceMetadataPresent,
        SauceCommentsImported,
        SauceWidthOverrideApplied,
        InvalidSauceCommentBlockIgnored,
        TruncatedSauceIgnored,

        PaletteDataIgnored,
        FontDataIgnored,
        ExtraTrailingBytesIgnored,

        CompressedXBinDecoded,
        AdfUnsupportedChunkIgnored
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

    using SauceMetadata = SauceSupport::SauceMetadata;

    struct LoadOptions
    {
        FileType fileType = FileType::Auto;

        int defaultColumns = 80;
        bool enableIceColors = false;
        bool preferSauceWidth = true;

        bool strictSizeValidation = true;
        bool strictUnsupportedFeatures = false;

        XBinCompressionSupport xbinCompressionSupport =
            XBinCompressionSupport::DecodeCompressed;

        bool importSauceComments = true;

        std::optional<Style> baseStyle;
    };

    struct LoadResult
    {
        TextObject object;
        bool success = false;

        FileType detectedFileType = FileType::Unknown;
        int resolvedWidth = 0;
        int resolvedHeight = 0;
        bool usedIceColors = false;

        SauceMetadata sauce;

        std::vector<LoadWarning> warnings;

        bool hasParseFailure = false;
        std::size_t failingByteOffset = 0;
        char32_t firstFailingCodePoint = U'\0';
        SourcePosition firstFailingPosition;

        std::string errorMessage;
    };

    FileType detectFileType(const std::string& filePath);

    LoadResult loadFromFile(const std::string& filePath);
    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options);
    LoadResult loadFromFile(const std::string& filePath, const Style& style);

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject);
    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const LoadOptions& options);
    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const Style& style);

    LoadResult loadFromBytes(std::string_view bytes, const LoadOptions& options = {});

    bool hasWarning(const LoadResult& result, LoadWarningCode code);

    const LoadWarning* getWarningByCode(
        const LoadResult& result,
        LoadWarningCode code);

    std::string formatLoadError(const LoadResult& result);
    std::string formatLoadSuccess(const LoadResult& result);

    const char* toString(FileType fileType);
    const char* toString(LoadWarningCode warningCode);
    const char* toString(XBinCompressionSupport mode);
}