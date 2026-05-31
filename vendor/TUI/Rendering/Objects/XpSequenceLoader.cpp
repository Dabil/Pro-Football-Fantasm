#include "Rendering/Objects/XpSequenceLoader.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    using namespace XpSequenceLoader;

    constexpr int kSupportedManifestVersion = 1;

    struct ParsedFrameLine
    {
        int lineNumber = 0;
        int frameIndex = -1;
        std::string sourcePath;
        std::string label;
        XpArtLoader::XpFrameOverrides overrides;
    };

    struct FrameBlockState
    {
        ParsedFrameLine frame;
        bool active = false;
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

    bool startsWith(std::string_view value, std::string_view prefix)
    {
        return value.size() >= prefix.size() &&
            value.substr(0, prefix.size()) == prefix;
    }

    std::string stripUtf8Bom(std::string_view text)
    {
        if (text.size() >= 3 &&
            static_cast<unsigned char>(text[0]) == 0xEF &&
            static_cast<unsigned char>(text[1]) == 0xBB &&
            static_cast<unsigned char>(text[2]) == 0xBF)
        {
            return std::string(text.substr(3));
        }

        return std::string(text);
    }

    bool isValidUtf8(std::string_view text)
    {
        std::size_t index = 0;
        while (index < text.size())
        {
            const unsigned char byte = static_cast<unsigned char>(text[index]);
            if (byte <= 0x7F)
            {
                ++index;
                continue;
            }

            int continuationCount = 0;
            std::uint32_t codePoint = 0;
            std::uint32_t minimumCodePoint = 0;

            if ((byte & 0xE0u) == 0xC0u)
            {
                continuationCount = 1;
                codePoint = byte & 0x1Fu;
                minimumCodePoint = 0x80u;
            }
            else if ((byte & 0xF0u) == 0xE0u)
            {
                continuationCount = 2;
                codePoint = byte & 0x0Fu;
                minimumCodePoint = 0x800u;
            }
            else if ((byte & 0xF8u) == 0xF0u)
            {
                continuationCount = 3;
                codePoint = byte & 0x07u;
                minimumCodePoint = 0x10000u;
            }
            else
            {
                return false;
            }

            if (index + static_cast<std::size_t>(continuationCount) >= text.size())
            {
                return false;
            }

            for (int offset = 1; offset <= continuationCount; ++offset)
            {
                const unsigned char continuation =
                    static_cast<unsigned char>(text[index + static_cast<std::size_t>(offset)]);
                if ((continuation & 0xC0u) != 0x80u)
                {
                    return false;
                }

                codePoint = (codePoint << 6) | (continuation & 0x3Fu);
            }

            if (codePoint < minimumCodePoint ||
                codePoint > 0x10FFFFu ||
                (codePoint >= 0xD800u && codePoint <= 0xDFFFu))
            {
                return false;
            }

            index += static_cast<std::size_t>(continuationCount + 1);
        }

        return true;
    }

    std::string unquote(std::string value)
    {
        if (value.size() >= 2 &&
            value.front() == '"' &&
            value.back() == '"')
        {
            value.erase(value.begin());
            value.pop_back();
        }

        return value;
    }

    std::vector<std::string> tokenizeFrameArguments(std::string_view text)
    {
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;

        for (char ch : text)
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

    void addDiagnostic(
        LoadResult& result,
        DiagnosticCode code,
        const std::string& message,
        int lineNumber,
        int frameIndex,
        const std::string& sourcePath = std::string(),
        bool isError = true)
    {
        Diagnostic diagnostic;
        diagnostic.code = code;
        diagnostic.isError = isError;
        diagnostic.lineNumber = lineNumber;
        diagnostic.frameIndex = frameIndex;
        diagnostic.manifestPath = result.manifestPath;
        diagnostic.sourcePath = sourcePath;
        diagnostic.message = message;
        result.diagnostics.push_back(std::move(diagnostic));
    }

    LoadResult failResult(
        const std::string& manifestPath,
        DiagnosticCode code,
        const std::string& message,
        int lineNumber,
        int frameIndex,
        const std::string& sourcePath = std::string())
    {
        LoadResult result;
        result.manifestPath = manifestPath;
        result.errorMessage = message;
        addDiagnostic(result, code, message, lineNumber, frameIndex, sourcePath, true);
        return result;
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

    bool tryParseBool(const std::string& text, bool& outValue)
    {
        if (text == "true" || text == "1" || text == "yes")
        {
            outValue = true;
            return true;
        }

        if (text == "false" || text == "0" || text == "no")
        {
            outValue = false;
            return true;
        }

        return false;
    }

    bool tryParseCompositeMode(
        const std::string& text,
        XpArtLoader::XpCompositeMode& outValue)
    {
        if (text == "Phase45BCompatible")
        {
            outValue = XpArtLoader::XpCompositeMode::Phase45BCompatible;
            return true;
        }

        if (text == "StrictOpaqueOverwrite")
        {
            outValue = XpArtLoader::XpCompositeMode::StrictOpaqueOverwrite;
            return true;
        }

        return false;
    }

    bool tryParseVisibleLayerMode(
        const std::string& text,
        XpArtLoader::XpVisibleLayerMode& outValue)
    {
        if (text == "UseDocumentVisibility")
        {
            outValue = XpArtLoader::XpVisibleLayerMode::UseDocumentVisibility;
            return true;
        }

        if (text == "ForceAllLayersVisible")
        {
            outValue = XpArtLoader::XpVisibleLayerMode::ForceAllLayersVisible;
            return true;
        }

        if (text == "UseExplicitVisibleLayerList")
        {
            outValue = XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList;
            return true;
        }

        return false;
    }

    bool tryParseExplicitVisibleLayerList(
        const std::string& text,
        std::vector<int>& outIndices)
    {
        outIndices.clear();

        const std::string trimmed = trimCopy(text);
        if (trimmed.empty())
        {
            return false;
        }

        std::stringstream stream(trimmed);
        std::string item;
        while (std::getline(stream, item, ','))
        {
            const std::string part = trimCopy(item);
            int index = -1;
            if (!tryParseInt(part, index) || index < 0)
            {
                outIndices.clear();
                return false;
            }

            outIndices.push_back(index);
        }

        return !outIndices.empty();
    }

    std::filesystem::path resolveFramePath(
        const std::filesystem::path& manifestPath,
        const std::string& sourcePath)
    {
        const std::filesystem::path candidate(sourcePath);
        if (candidate.is_absolute())
        {
            return candidate.lexically_normal();
        }

        const std::filesystem::path baseDirectory =
            manifestPath.has_parent_path()
            ? manifestPath.parent_path()
            : std::filesystem::current_path();

        return (baseDirectory / candidate).lexically_normal();
    }

    bool parseAssignment(
        const std::string& line,
        std::string& outKey,
        std::string& outValue)
    {
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            return false;
        }

        outKey = trimCopy(line.substr(0, eq));
        outValue = unquote(trimCopy(line.substr(eq + 1)));
        return !outKey.empty();
    }

    bool handleUnknownField(
        const std::string& key,
        const std::string& value,
        int lineNumber,
        int frameIndex,
        bool frameField,
        const LoadOptions& options,
        LoadResult& ioResult)
    {
        const DiagnosticCode strictCode =
            frameField ? DiagnosticCode::InvalidFrameDirective : DiagnosticCode::InvalidDirective;
        const DiagnosticCode ignoredCode =
            frameField ? DiagnosticCode::UnknownFrameDirectiveIgnored : DiagnosticCode::UnknownDirectiveIgnored;
        const DiagnosticCode preservedCode =
            frameField ? DiagnosticCode::UnknownFrameDirectivePreserved : DiagnosticCode::UnknownDirectivePreserved;
        const std::string fieldLabel = frameField ? "frame field" : "manifest directive";
        const std::string message = "Unknown " + fieldLabel + ": " + key;

        if (options.unknownFieldPolicy == UnknownFieldPolicy::StrictError)
        {
            addDiagnostic(ioResult, strictCode, message, lineNumber, frameIndex);
            return false;
        }

        if (options.unknownFieldPolicy == UnknownFieldPolicy::WarnAndIgnore)
        {
            addDiagnostic(
                ioResult,
                ignoredCode,
                "Ignoring unknown " + fieldLabel + ": " + key,
                lineNumber,
                frameIndex,
                std::string(),
                false);
            return true;
        }

        std::string extensionKey = frameField
            ? ("manifest.frame." + std::to_string(frameIndex) + "." + key)
            : ("manifest.sequence." + key);

        if (!ioResult.sequence.metadata.extensionFields.trySet(extensionKey, value))
        {
            addDiagnostic(
                ioResult,
                ignoredCode,
                "Ignoring unknown " + fieldLabel + " because extension storage is full: " + key,
                lineNumber,
                frameIndex,
                std::string(),
                false);
            return true;
        }

        addDiagnostic(
            ioResult,
            preservedCode,
            "Preserved unknown " + fieldLabel + ": " + key,
            lineNumber,
            frameIndex,
            std::string(),
            false);
        return true;
    }

    bool parseFrameField(
        const std::string& key,
        const std::string& value,
        int lineNumber,
        const LoadOptions& options,
        LoadResult& ioResult,
        ParsedFrameLine& ioFrame)
    {
        if (key == "index")
        {
            if (!tryParseInt(value, ioFrame.frameIndex) || ioFrame.frameIndex < 0)
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidFrameIndex,
                    "Frame index must be a non-negative integer.",
                    lineNumber,
                    ioFrame.frameIndex);
                return false;
            }

            return true;
        }

        if (key == "source")
        {
            ioFrame.sourcePath = value;
            return true;
        }

        if (key == "label")
        {
            ioFrame.label = value;
            return true;
        }

        if (key == "duration_ms" || key == "frame_duration_ms")
        {
            int duration = 0;
            if (!tryParseInt(value, duration) || duration < 0)
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidDurationMilliseconds,
                    "Frame duration must be a non-negative integer.",
                    lineNumber,
                    ioFrame.frameIndex);
                return false;
            }

            ioFrame.overrides.durationMilliseconds = duration;
            return true;
        }

        if (key == "composite")
        {
            XpArtLoader::XpCompositeMode compositeMode;
            if (!tryParseCompositeMode(value, compositeMode))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidCompositeMode,
                    "Invalid frame composite value: " + value,
                    lineNumber,
                    ioFrame.frameIndex);
                return false;
            }

            ioFrame.overrides.compositeMode = compositeMode;
            return true;
        }

        if (key == "visible_layers")
        {
            XpArtLoader::XpVisibleLayerMode visibleLayerMode;
            if (!tryParseVisibleLayerMode(value, visibleLayerMode))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidVisibleLayerMode,
                    "Invalid frame visible_layers value: " + value,
                    lineNumber,
                    ioFrame.frameIndex);
                return false;
            }

            ioFrame.overrides.visibleLayerMode = visibleLayerMode;
            return true;
        }

        if (key == "explicit_visible_layers")
        {
            if (!tryParseExplicitVisibleLayerList(
                value,
                ioFrame.overrides.explicitVisibleLayerIndices))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidExplicitVisibleLayerList,
                    "Frame explicit_visible_layers must be a comma-separated list of non-negative integers.",
                    lineNumber,
                    ioFrame.frameIndex);
                return false;
            }

            return true;
        }

        return handleUnknownField(
            key,
            value,
            lineNumber,
            ioFrame.frameIndex,
            true,
            options,
            ioResult);
    }

    bool finalizeFrame(
        LoadResult& ioResult,
        ParsedFrameLine& ioFrame)
    {
        if (ioFrame.frameIndex < 0)
        {
            addDiagnostic(
                ioResult,
                DiagnosticCode::MissingFrameIndex,
                "Frame block is missing index=...",
                ioFrame.lineNumber,
                -1);
            return false;
        }

        if (ioFrame.sourcePath.empty())
        {
            addDiagnostic(
                ioResult,
                DiagnosticCode::MissingFrameSource,
                "Frame block is missing source=...",
                ioFrame.lineNumber,
                ioFrame.frameIndex);
            return false;
        }

        if (ioFrame.overrides.visibleLayerMode.has_value() &&
            *ioFrame.overrides.visibleLayerMode ==
            XpArtLoader::XpVisibleLayerMode::UseExplicitVisibleLayerList &&
            ioFrame.overrides.explicitVisibleLayerIndices.empty())
        {
            addDiagnostic(
                ioResult,
                DiagnosticCode::InvalidExplicitVisibleLayerList,
                "Frame uses visible_layers=UseExplicitVisibleLayerList but does not define explicit_visible_layers.",
                ioFrame.lineNumber,
                ioFrame.frameIndex);
            return false;
        }

        return true;
    }

    bool parseLegacyFrameLine(
        const std::string& line,
        int lineNumber,
        const LoadOptions& options,
        LoadResult& ioResult,
        ParsedFrameLine& outFrame)
    {
        outFrame = ParsedFrameLine{};
        outFrame.lineNumber = lineNumber;

        const std::string remainder = trimCopy(std::string_view(line).substr(5));
        const std::vector<std::string> tokens = tokenizeFrameArguments(remainder);
        if (tokens.empty())
        {
            addDiagnostic(
                ioResult,
                DiagnosticCode::InvalidFrameDirective,
                "Frame directive is missing key=value arguments.",
                lineNumber,
                -1);
            return false;
        }

        for (const std::string& token : tokens)
        {
            const std::size_t eq = token.find('=');
            if (eq == std::string::npos)
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidFrameDirective,
                    "Frame directive token is missing '=': " + token,
                    lineNumber,
                    outFrame.frameIndex);
                return false;
            }

            if (!parseFrameField(
                trimCopy(token.substr(0, eq)),
                unquote(trimCopy(token.substr(eq + 1))),
                lineNumber,
                options,
                ioResult,
                outFrame))
            {
                return false;
            }
        }

        return finalizeFrame(ioResult, outFrame);
    }

    bool finalizeContiguousFrameValidation(
        LoadResult& ioResult,
        const std::vector<ParsedFrameLine>& frames)
    {
        if (frames.empty())
        {
            addDiagnostic(
                ioResult,
                DiagnosticCode::MissingFrame,
                "Manifest does not define any frames.",
                0,
                -1);
            return false;
        }

        std::set<int> indices;
        for (const ParsedFrameLine& frame : frames)
        {
            if (!indices.insert(frame.frameIndex).second)
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::DuplicateFrameIndex,
                    "Manifest defines a duplicate frame index.",
                    frame.lineNumber,
                    frame.frameIndex);
                return false;
            }
        }

        const int minIndex = *indices.begin();
        const int maxIndex = *indices.rbegin();

        if (minIndex != 0)
        {
            addDiagnostic(
                ioResult,
                DiagnosticCode::MissingFrame,
                "Frame indices must start at 0 for deterministic sequence ownership.",
                0,
                minIndex);
            return false;
        }

        for (int index = minIndex; index <= maxIndex; ++index)
        {
            if (indices.find(index) == indices.end())
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::MissingFrame,
                    "Manifest is missing frame index " + std::to_string(index) + ".",
                    0,
                    index);
                return false;
            }
        }

        return true;
    }

    bool parseSequenceField(
        const std::string& key,
        const std::string& value,
        int lineNumber,
        const LoadOptions& options,
        LoadResult& ioResult)
    {
        XpArtLoader::XpSequenceMetadata& metadata = ioResult.sequence.metadata;

        if (key == "name" || key == "sequence_label")
        {
            metadata.name = value;
            metadata.sequenceLabel = value;
            return true;
        }

        if (key == "loop")
        {
            bool loop = false;
            if (!tryParseBool(value, loop))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidBooleanValue,
                    "loop must be true/false, yes/no, or 1/0.",
                    lineNumber,
                    -1);
                return false;
            }

            metadata.loop = loop;
            return true;
        }

        if (key == "default_frame_duration_ms" || key == "default_duration_ms")
        {
            int duration = 0;
            if (!tryParseInt(value, duration) || duration < 0)
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidDurationMilliseconds,
                    "default_frame_duration_ms must be a non-negative integer.",
                    lineNumber,
                    -1);
                return false;
            }

            metadata.defaultFrameDurationMilliseconds = duration;
            return true;
        }

        if (key == "default_fps")
        {
            int fps = 0;
            if (!tryParseInt(value, fps) || fps <= 0)
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidFramesPerSecond,
                    "default_fps must be a positive integer.",
                    lineNumber,
                    -1);
                return false;
            }

            metadata.defaultFramesPerSecond = fps;
            return true;
        }

        if (key == "default_composite")
        {
            XpArtLoader::XpCompositeMode compositeMode;
            if (!tryParseCompositeMode(value, compositeMode))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidCompositeMode,
                    "Invalid default_composite value: " + value,
                    lineNumber,
                    -1);
                return false;
            }

            metadata.defaultCompositeMode = compositeMode;
            return true;
        }

        if (key == "default_visible_layers")
        {
            XpArtLoader::XpVisibleLayerMode visibleLayerMode;
            if (!tryParseVisibleLayerMode(value, visibleLayerMode))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidVisibleLayerMode,
                    "Invalid default_visible_layers value: " + value,
                    lineNumber,
                    -1);
                return false;
            }

            metadata.defaultVisibleLayerMode = visibleLayerMode;
            return true;
        }

        if (key == "default_explicit_visible_layers")
        {
            if (!tryParseExplicitVisibleLayerList(
                value,
                metadata.defaultExplicitVisibleLayerIndices))
            {
                addDiagnostic(
                    ioResult,
                    DiagnosticCode::InvalidExplicitVisibleLayerList,
                    "default_explicit_visible_layers must be a comma-separated list of non-negative integers.",
                    lineNumber,
                    -1);
                return false;
            }

            return true;
        }

        return handleUnknownField(
            key,
            value,
            lineNumber,
            -1,
            false,
            options,
            ioResult);
    }

    bool tryCanonicalPath(
        const std::filesystem::path& path,
        std::filesystem::path& outPath)
    {
        std::error_code error;
        outPath = std::filesystem::weakly_canonical(path, error);
        if (error)
        {
            return false;
        }

        return true;
    }
}

namespace XpSequenceLoader
{
    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options)
    {
        std::string manifestText;
        if (!readAllText(filePath, manifestText))
        {
            return failResult(
                filePath,
                DiagnosticCode::BadRelativePath,
                "Failed to read .xpseq manifest file: " + filePath,
                0,
                -1);
        }

        return loadFromString(manifestText, filePath, options);
    }

    LoadResult loadFromString(
        std::string_view manifestUtf8,
        const std::string& manifestPath,
        const LoadOptions& options)
    {
        LoadResult result;
        result.manifestPath = manifestPath;

        const std::string manifestText = stripUtf8Bom(manifestUtf8);
        if (!isValidUtf8(manifestText))
        {
            addDiagnostic(
                result,
                DiagnosticCode::InvalidUtf8,
                "Manifest is not valid UTF-8.",
                0,
                -1);
            result.errorMessage = "Invalid UTF-8 in .xpseq manifest.";
            return result;
        }

        std::istringstream stream(manifestText);
        std::string line;
        int lineNumber = 0;
        bool sawHeader = false;
        std::vector<ParsedFrameLine> parsedFrames;
        FrameBlockState currentFrame;

        while (std::getline(stream, line))
        {
            ++lineNumber;
            const std::string trimmed = trimCopy(line);

            if (trimmed.empty() || startsWith(trimmed, "#"))
            {
                continue;
            }

            if (!sawHeader)
            {
                std::stringstream headerStream(trimmed);
                std::string magic;
                std::string versionText;
                headerStream >> magic >> versionText;

                int version = 0;
                if (magic != "xpseq")
                {
                    addDiagnostic(
                        result,
                        DiagnosticCode::InvalidHeader,
                        "First non-comment line must be 'xpseq 1'.",
                        lineNumber,
                        -1);
                    result.errorMessage = "Invalid .xpseq header.";
                    return result;
                }

                if (!tryParseInt(versionText, version))
                {
                    addDiagnostic(
                        result,
                        DiagnosticCode::InvalidHeader,
                        "Malformed .xpseq header. Expected 'xpseq 1'.",
                        lineNumber,
                        -1);
                    result.errorMessage = "Malformed .xpseq header.";
                    return result;
                }

                if (version != kSupportedManifestVersion)
                {
                    addDiagnostic(
                        result,
                        DiagnosticCode::UnsupportedVersion,
                        "Unsupported .xpseq version: " + std::to_string(version) + ".",
                        lineNumber,
                        -1);
                    result.errorMessage = "Unsupported .xpseq version.";
                    return result;
                }

                result.resolvedVersion = version;
                sawHeader = true;
                continue;
            }

            if (currentFrame.active)
            {
                if (trimmed == "}")
                {
                    if (!finalizeFrame(result, currentFrame.frame))
                    {
                        result.errorMessage = "Invalid .xpseq frame block.";
                        return result;
                    }

                    parsedFrames.push_back(std::move(currentFrame.frame));
                    currentFrame = FrameBlockState{};
                    continue;
                }

                std::string key;
                std::string value;
                if (!parseAssignment(trimmed, key, value))
                {
                    addDiagnostic(
                        result,
                        DiagnosticCode::InvalidFrameBlock,
                        "Frame block directives must be key=value assignments.",
                        lineNumber,
                        currentFrame.frame.frameIndex);
                    result.errorMessage = "Invalid .xpseq frame block.";
                    return result;
                }

                if (!parseFrameField(key, value, lineNumber, options, result, currentFrame.frame))
                {
                    result.errorMessage = "Invalid .xpseq frame block.";
                    return result;
                }

                continue;
            }

            if (trimmed == "frame" || trimmed == "frame{")
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::InvalidFrameBlock,
                    "Frame blocks must use 'frame {' on a single line.",
                    lineNumber,
                    -1);
                result.errorMessage = "Invalid .xpseq frame block.";
                return result;
            }

            if (trimmed == "frame {" || trimmed == "frame{")
            {
                currentFrame = FrameBlockState{};
                currentFrame.active = true;
                currentFrame.frame.lineNumber = lineNumber;
                continue;
            }

            if (startsWith(trimmed, "frame "))
            {
                ParsedFrameLine frame;
                if (!parseLegacyFrameLine(trimmed, lineNumber, options, result, frame))
                {
                    result.errorMessage = "Invalid .xpseq frame directive.";
                    return result;
                }

                parsedFrames.push_back(std::move(frame));
                continue;
            }

            std::string key;
            std::string value;
            if (!parseAssignment(trimmed, key, value))
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::InvalidDirective,
                    "Invalid manifest directive: " + trimmed,
                    lineNumber,
                    -1);
                result.errorMessage = "Invalid .xpseq directive.";
                return result;
            }

            if (!parseSequenceField(key, value, lineNumber, options, result))
            {
                result.errorMessage = "Invalid .xpseq directive.";
                return result;
            }
        }

        if (!sawHeader)
        {
            addDiagnostic(
                result,
                DiagnosticCode::EmptyManifest,
                "Manifest does not contain a valid xpseq header.",
                0,
                -1);
            result.errorMessage = "Empty or invalid .xpseq manifest.";
            return result;
        }

        if (currentFrame.active)
        {
            addDiagnostic(
                result,
                DiagnosticCode::UnterminatedFrameBlock,
                "Frame block was not closed with '}'.",
                currentFrame.frame.lineNumber,
                currentFrame.frame.frameIndex);
            result.errorMessage = "Unterminated .xpseq frame block.";
            return result;
        }

        if (options.requireContiguousFrameIndices &&
            !finalizeContiguousFrameValidation(result, parsedFrames))
        {
            result.errorMessage = "Frame index validation failed.";
            return result;
        }

        std::filesystem::path manifestFsPath =
            manifestPath.empty()
            ? (std::filesystem::current_path() / "in_memory.xpseq")
            : std::filesystem::path(manifestPath);

        result.sequence.metadata.sourceManifestPath = manifestFsPath.lexically_normal().string();

        if (result.sequence.metadata.usesExplicitVisibleLayerList() &&
            result.sequence.metadata.defaultExplicitVisibleLayerIndices.empty())
        {
            addDiagnostic(
                result,
                DiagnosticCode::InvalidExplicitVisibleLayerList,
                "Manifest uses default_visible_layers=UseExplicitVisibleLayerList but does not define default_explicit_visible_layers.",
                0,
                -1);
            result.errorMessage = "Missing default_explicit_visible_layers for explicit sequence default.";
            return result;
        }

        for (const ParsedFrameLine& parsedFrame : parsedFrames)
        {
            if (parsedFrame.sourcePath.empty())
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::MissingFrameSource,
                    "Frame source path is empty.",
                    parsedFrame.lineNumber,
                    parsedFrame.frameIndex);
                result.errorMessage = "Missing frame source path.";
                return result;
            }

            const std::filesystem::path resolvedPath =
                resolveFramePath(manifestFsPath, parsedFrame.sourcePath);

            std::filesystem::path canonicalPath;
            if (!tryCanonicalPath(resolvedPath, canonicalPath))
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::BadRelativePath,
                    "Failed to resolve frame source path: " + resolvedPath.string(),
                    parsedFrame.lineNumber,
                    parsedFrame.frameIndex,
                    resolvedPath.string());
                result.errorMessage = "Failed to resolve frame source path.";
                return result;
            }

            std::error_code existsError;
            const bool exists = std::filesystem::exists(canonicalPath, existsError);
            if (existsError || !exists)
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::BadRelativePath,
                    "Resolved frame source does not exist: " + canonicalPath.string(),
                    parsedFrame.lineNumber,
                    parsedFrame.frameIndex,
                    canonicalPath.string());
                result.errorMessage = "Resolved frame source path does not exist.";
                return result;
            }

            XpArtLoader::LoadOptions xpOptions = options.xpLoadOptions;
            xpOptions.flattenLayers = false;

            const XpArtLoader::LoadResult xpLoadResult =
                XpArtLoader::loadFromFile(canonicalPath.string(), xpOptions);

            if (!xpLoadResult.success || !xpLoadResult.hasRetainedDocument)
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::XpFrameLoadFailed,
                    xpLoadResult.errorMessage.empty()
                    ? "Failed to load referenced XP frame."
                    : xpLoadResult.errorMessage,
                    parsedFrame.lineNumber,
                    parsedFrame.frameIndex,
                    canonicalPath.string());
                result.errorMessage = "Failed to load referenced XP frame.";
                return result;
            }

            XpArtLoader::XpFrame frame;
            frame.frameIndex = parsedFrame.frameIndex;
            frame.label = parsedFrame.label;
            frame.sourcePath = canonicalPath.string();
            frame.document = std::make_shared<XpArtLoader::XpDocument>(
                xpLoadResult.retainedDocument);
            frame.overrides = parsedFrame.overrides;

            if (!frame.isValid())
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::InvalidFrameDirective,
                    "Loaded frame overrides are invalid for the referenced XP document.",
                    parsedFrame.lineNumber,
                    parsedFrame.frameIndex,
                    canonicalPath.string());
                result.errorMessage = "Invalid frame overrides for referenced XP document.";
                return result;
            }

            if (!result.sequence.metadata.isValidForDocument(frame.getDocument()))
            {
                addDiagnostic(
                    result,
                    DiagnosticCode::InvalidExplicitVisibleLayerList,
                    "Sequence default visible-layer overrides are invalid for a referenced XP document.",
                    parsedFrame.lineNumber,
                    parsedFrame.frameIndex,
                    canonicalPath.string());
                result.errorMessage = "Invalid sequence default visible-layer overrides.";
                return result;
            }

            result.sequence.frames.push_back(std::move(frame));
        }

        if (options.sortFramesByFrameIndex)
        {
            result.sequence.sortFramesByFrameIndex();
        }

        if (!result.sequence.isValid())
        {
            result.errorMessage = "Constructed .xpseq retained sequence is invalid.";
            return result;
        }

        result.resolvedFrameCount = result.sequence.getFrameCount();
        result.success = true;
        return result;
    }

    const Diagnostic* getFirstDiagnosticByCode(
        const LoadResult& result,
        DiagnosticCode code)
    {
        for (const Diagnostic& diagnostic : result.diagnostics)
        {
            if (diagnostic.code == code)
            {
                return &diagnostic;
            }
        }

        return nullptr;
    }

    std::string formatLoadError(const LoadResult& result)
    {
        std::ostringstream stream;
        stream << ".xpseq load failed";

        if (!result.errorMessage.empty())
        {
            stream << ": " << result.errorMessage;
        }

        if (!result.diagnostics.empty())
        {
            const Diagnostic& diagnostic = result.diagnostics.front();
            if (diagnostic.lineNumber > 0)
            {
                stream << " (line " << diagnostic.lineNumber << ")";
            }

            if (diagnostic.frameIndex >= 0)
            {
                stream << " [frame " << diagnostic.frameIndex << "]";
            }
        }

        return stream.str();
    }

    std::string formatLoadSuccess(const LoadResult& result)
    {
        std::ostringstream stream;
        stream << ".xpseq load success: "
            << "frames=" << result.resolvedFrameCount
            << ", version=" << result.resolvedVersion;

        if (!result.sequence.metadata.name.empty())
        {
            stream << ", name=" << result.sequence.metadata.name;
        }
        else if (!result.sequence.metadata.sequenceLabel.empty())
        {
            stream << ", name=" << result.sequence.metadata.sequenceLabel;
        }

        if (result.sequence.metadata.loop.has_value())
        {
            stream << ", loop=" << (*result.sequence.metadata.loop ? "true" : "false");
        }

        if (!result.sequence.metadata.sourceManifestPath.empty())
        {
            stream << ", manifest=" << result.sequence.metadata.sourceManifestPath;
        }

        return stream.str();
    }

    const char* toString(DiagnosticCode code)
    {
        switch (code)
        {
        case DiagnosticCode::None:
            return "None";
        case DiagnosticCode::EmptyManifest:
            return "EmptyManifest";
        case DiagnosticCode::InvalidUtf8:
            return "InvalidUtf8";
        case DiagnosticCode::InvalidHeader:
            return "InvalidHeader";
        case DiagnosticCode::UnsupportedVersion:
            return "UnsupportedVersion";
        case DiagnosticCode::InvalidDirective:
            return "InvalidDirective";
        case DiagnosticCode::InvalidBooleanValue:
            return "InvalidBooleanValue";
        case DiagnosticCode::InvalidFramesPerSecond:
            return "InvalidFramesPerSecond";
        case DiagnosticCode::InvalidFrameDirective:
            return "InvalidFrameDirective";
        case DiagnosticCode::InvalidFrameBlock:
            return "InvalidFrameBlock";
        case DiagnosticCode::UnterminatedFrameBlock:
            return "UnterminatedFrameBlock";
        case DiagnosticCode::MissingFrameIndex:
            return "MissingFrameIndex";
        case DiagnosticCode::InvalidFrameIndex:
            return "InvalidFrameIndex";
        case DiagnosticCode::DuplicateFrameIndex:
            return "DuplicateFrameIndex";
        case DiagnosticCode::MissingFrameSource:
            return "MissingFrameSource";
        case DiagnosticCode::MissingFrame:
            return "MissingFrame";
        case DiagnosticCode::BadRelativePath:
            return "BadRelativePath";
        case DiagnosticCode::InvalidDurationMilliseconds:
            return "InvalidDurationMilliseconds";
        case DiagnosticCode::InvalidCompositeMode:
            return "InvalidCompositeMode";
        case DiagnosticCode::InvalidVisibleLayerMode:
            return "InvalidVisibleLayerMode";
        case DiagnosticCode::InvalidExplicitVisibleLayerList:
            return "InvalidExplicitVisibleLayerList";
        case DiagnosticCode::XpFrameLoadFailed:
            return "XpFrameLoadFailed";
        case DiagnosticCode::UnknownDirectiveIgnored:
            return "UnknownDirectiveIgnored";
        case DiagnosticCode::UnknownFrameDirectiveIgnored:
            return "UnknownFrameDirectiveIgnored";
        case DiagnosticCode::UnknownDirectivePreserved:
            return "UnknownDirectivePreserved";
        case DiagnosticCode::UnknownFrameDirectivePreserved:
            return "UnknownFrameDirectivePreserved";
        default:
            return "Unknown";
        }
    }

    const char* toString(UnknownFieldPolicy policy)
    {
        switch (policy)
        {
        case UnknownFieldPolicy::StrictError:
            return "StrictError";
        case UnknownFieldPolicy::WarnAndIgnore:
            return "WarnAndIgnore";
        case UnknownFieldPolicy::PreserveInMetadata:
            return "PreserveInMetadata";
        default:
            return "Unknown";
        }
    }
}
