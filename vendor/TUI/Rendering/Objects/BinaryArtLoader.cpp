#include "Rendering/Objects/BinaryArtLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Objects/TextObjectBuilder.h"
#include "Rendering/Styles/Color.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace
{
    std::string toLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char ch)
            {
                return static_cast<char>(std::tolower(ch));
            });

        return value;
    }

    std::string getFileExtensionLower(const std::string& filePath)
    {
        const std::size_t dot = filePath.find_last_of('.');
        if (dot == std::string::npos)
        {
            return {};
        }

        return toLowerCopy(filePath.substr(dot));
    }

    bool readAllBytes(const std::string& filePath, std::string& outBytes)
    {
        std::ifstream file(filePath, std::ios::binary);
        if (!file)
        {
            return false;
        }

        outBytes.assign(
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>());

        return true;
    }

    std::uint16_t readLe16(std::string_view bytes, std::size_t offset, bool& ok)
    {
        if (offset + 1 >= bytes.size())
        {
            ok = false;
            return 0;
        }

        ok = true;
        return static_cast<std::uint16_t>(
            static_cast<unsigned char>(bytes[offset]) |
            (static_cast<unsigned char>(bytes[offset + 1]) << 8));
    }

    std::uint32_t readLe32(std::string_view bytes, std::size_t offset, bool& ok)
    {
        if (offset + 3 >= bytes.size())
        {
            ok = false;
            return 0;
        }

        ok = true;
        return static_cast<std::uint32_t>(
            static_cast<unsigned char>(bytes[offset]) |
            (static_cast<unsigned char>(bytes[offset + 1]) << 8) |
            (static_cast<unsigned char>(bytes[offset + 2]) << 16) |
            (static_cast<unsigned char>(bytes[offset + 3]) << 24));
    }

    bool startsWithAdfSignature(std::string_view bytes)
    {
        return bytes.size() >= 3 &&
            ((bytes[0] == 'A' && bytes[1] == 'D' && bytes[2] == 'F') ||
                (bytes[0] == 'a' && bytes[1] == 'd' && bytes[2] == 'f'));
    }

    bool isCellOrTextChunkId(std::string_view chunkId)
    {
        return chunkId == "CELL" || chunkId == "cell" ||
            chunkId == "TEXT" || chunkId == "text";
    }

    bool isSizeChunkId(std::string_view chunkId)
    {
        return chunkId == "SIZE" || chunkId == "size";
    }

    bool looksLikeAdf(std::string_view bytes)
    {
        if (!startsWithAdfSignature(bytes))
        {
            return false;
        }

        std::size_t offset = 3;
        if (offset < bytes.size() && static_cast<unsigned char>(bytes[offset]) == 0x1A)
        {
            ++offset;
        }

        if (offset + 4 <= bytes.size())
        {
            const std::string_view maybeMain = bytes.substr(offset, 4);
            if (maybeMain == "MAIN" || maybeMain == "main")
            {
                offset += 4;
            }
        }

        if (offset + 8 > bytes.size())
        {
            return false;
        }

        const std::string_view chunkId = bytes.substr(offset, 4);
        if (!isCellOrTextChunkId(chunkId) && !isSizeChunkId(chunkId))
        {
            return false;
        }

        bool ok = false;
        const std::uint32_t chunkSize = readLe32(bytes, offset + 4, ok);
        if (!ok)
        {
            return false;
        }

        const std::size_t chunkDataOffset = offset + 8;
        if (chunkDataOffset + static_cast<std::size_t>(chunkSize) > bytes.size())
        {
            return false;
        }

        if (isCellOrTextChunkId(chunkId))
        {
            if (chunkSize < 4)
            {
                return false;
            }

            const std::uint16_t width = readLe16(bytes, chunkDataOffset + 0, ok);
            if (!ok || width == 0)
            {
                return false;
            }

            const std::uint16_t height = readLe16(bytes, chunkDataOffset + 2, ok);
            if (!ok || height == 0)
            {
                return false;
            }

            const std::size_t expectedCellBytes =
                static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height) * 2u;

            if (4u + expectedCellBytes > static_cast<std::size_t>(chunkSize))
            {
                return false;
            }

            return true;
        }

        if (isSizeChunkId(chunkId))
        {
            if (chunkSize < 4)
            {
                return false;
            }

            const std::uint16_t width = readLe16(bytes, chunkDataOffset + 0, ok);
            if (!ok || width == 0)
            {
                return false;
            }

            const std::uint16_t height = readLe16(bytes, chunkDataOffset + 2, ok);
            if (!ok || height == 0)
            {
                return false;
            }

            return true;
        }

        return false;
    }

    char32_t decodeCp437Byte(unsigned char value)
    {
        if (value <= 0x7F)
        {
            return UnicodeConversion::sanitizeCodePoint(static_cast<char32_t>(value));
        }

        static const char32_t kCp437Table[128] =
        {
            U'\u00C7', U'\u00FC', U'\u00E9', U'\u00E2', U'\u00E4', U'\u00E0', U'\u00E5', U'\u00E7',
            U'\u00EA', U'\u00EB', U'\u00E8', U'\u00EF', U'\u00EE', U'\u00EC', U'\u00C4', U'\u00C5',
            U'\u00C9', U'\u00E6', U'\u00C6', U'\u00F4', U'\u00F6', U'\u00F2', U'\u00FB', U'\u00F9',
            U'\u00FF', U'\u00D6', U'\u00DC', U'\u00A2', U'\u00A3', U'\u00A5', U'\u20A7', U'\u0192',
            U'\u00E1', U'\u00ED', U'\u00F3', U'\u00FA', U'\u00F1', U'\u00D1', U'\u00AA', U'\u00BA',
            U'\u00BF', U'\u2310', U'\u00AC', U'\u00BD', U'\u00BC', U'\u00A1', U'\u00AB', U'\u00BB',
            U'\u2591', U'\u2592', U'\u2593', U'\u2502', U'\u2524', U'\u2561', U'\u2562', U'\u2556',
            U'\u2555', U'\u2563', U'\u2551', U'\u2557', U'\u255D', U'\u255C', U'\u255B', U'\u2510',
            U'\u2514', U'\u2534', U'\u252C', U'\u251C', U'\u2500', U'\u253C', U'\u255E', U'\u255F',
            U'\u255A', U'\u2554', U'\u2569', U'\u2566', U'\u2560', U'\u2550', U'\u256C', U'\u2567',
            U'\u2568', U'\u2564', U'\u2565', U'\u2559', U'\u2558', U'\u2552', U'\u2553', U'\u256B',
            U'\u256A', U'\u2518', U'\u250C', U'\u2588', U'\u2584', U'\u258C', U'\u2590', U'\u2580',
            U'\u03B1', U'\u00DF', U'\u0393', U'\u03C0', U'\u03A3', U'\u03C3', U'\u00B5', U'\u03C4',
            U'\u03A6', U'\u0398', U'\u03A9', U'\u03B4', U'\u221E', U'\u03C6', U'\u03B5', U'\u2229',
            U'\u2261', U'\u00B1', U'\u2265', U'\u2264', U'\u2320', U'\u2321', U'\u00F7', U'\u2248',
            U'\u00B0', U'\u2219', U'\u00B7', U'\u221A', U'\u207F', U'\u00B2', U'\u25A0', U'\u00A0'
        };

        return UnicodeConversion::sanitizeCodePoint(kCp437Table[value - 0x80]);
    }

    Color::Basic dosBasicColor(int value)
    {
        switch (value & 0x0F)
        {
        case 0x0: return Color::Basic::Black;
        case 0x1: return Color::Basic::Blue;
        case 0x2: return Color::Basic::Green;
        case 0x3: return Color::Basic::Cyan;
        case 0x4: return Color::Basic::Red;
        case 0x5: return Color::Basic::Magenta;
        case 0x6: return Color::Basic::Yellow;
        case 0x7: return Color::Basic::White;
        case 0x8: return Color::Basic::BrightBlack;
        case 0x9: return Color::Basic::BrightBlue;
        case 0xA: return Color::Basic::BrightGreen;
        case 0xB: return Color::Basic::BrightCyan;
        case 0xC: return Color::Basic::BrightRed;
        case 0xD: return Color::Basic::BrightMagenta;
        case 0xE: return Color::Basic::BrightYellow;
        case 0xF: return Color::Basic::BrightWhite;
        default: return Color::Basic::White;
        }
    }

    Style styleFromDosAttribute(
        unsigned char attribute,
        const std::optional<Style>& baseStyle,
        bool iceColors)
    {
        Style style = baseStyle.value_or(Style{});

        const int foreground = attribute & 0x0F;
        style = style.withForeground(Color::FromBasic(dosBasicColor(foreground)));

        if (iceColors)
        {
            const int background = (attribute >> 4) & 0x0F;
            style = style.withBackground(Color::FromBasic(dosBasicColor(background)));
        }
        else
        {
            const int background = (attribute >> 4) & 0x07;
            style = style.withBackground(Color::FromBasic(dosBasicColor(background)));

            if ((attribute & 0x80) != 0)
            {
                style = style.withSlowBlink(true);
            }
        }

        return style;
    }

    void addWarning(
        BinaryArtLoader::LoadResult& result,
        BinaryArtLoader::LoadWarningCode code,
        const std::string& message,
        std::size_t byteOffset,
        const BinaryArtLoader::SourcePosition& position)
    {
        BinaryArtLoader::LoadWarning warning;
        warning.code = code;
        warning.message = message;
        warning.byteOffset = byteOffset;
        warning.sourcePosition = position;
        result.warnings.push_back(warning);
    }

    BinaryArtLoader::LoadResult fail(
        BinaryArtLoader::FileType fileType,
        const std::string& message,
        std::size_t byteOffset = 0,
        char32_t failingCodePoint = U'\0',
        const BinaryArtLoader::SourcePosition& position = {})
    {
        BinaryArtLoader::LoadResult result;
        result.success = false;
        result.detectedFileType = fileType;
        result.hasParseFailure = true;
        result.failingByteOffset = byteOffset;
        result.firstFailingCodePoint = failingCodePoint;
        result.firstFailingPosition = position;
        result.errorMessage = message;
        return result;
    }

    void appendSauceWarnings(
        BinaryArtLoader::LoadResult& result,
        const BinaryArtLoader::LoadOptions& options,
        const decltype(SauceSupport::parse(std::string_view{}))& sauceRecord)
    {
        result.sauce = sauceRecord.metadata;

        if (result.sauce.present)
        {
            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::SauceMetadataPresent,
                "SAUCE metadata was detected and parsed.",
                0,
                {});
        }

        if (options.importSauceComments && !result.sauce.comments.empty())
        {
            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::SauceCommentsImported,
                "SAUCE comments were imported into metadata.",
                0,
                {});
        }

        if (sauceRecord.invalidCommentBlock)
        {
            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::InvalidSauceCommentBlockIgnored,
                "SAUCE declared a comment block, but the COMNT marker was invalid and the block was ignored.",
                sauceRecord.contentSize,
                {});
        }

        if (sauceRecord.truncated)
        {
            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::TruncatedSauceIgnored,
                "SAUCE metadata or comments appeared truncated and were partially ignored.",
                sauceRecord.contentSize,
                {});
        }
    }

    BinaryArtLoader::LoadResult decodeCellStream(
        std::string_view bytes,
        int width,
        const BinaryArtLoader::LoadOptions& options,
        BinaryArtLoader::FileType fileType,
        bool usedIceColors,
        const BinaryArtLoader::SauceMetadata& sauce,
        std::size_t baseByteOffset = 0)
    {
        BinaryArtLoader::LoadResult result;
        result.detectedFileType = fileType;
        result.usedIceColors = usedIceColors;
        result.sauce = sauce;

        if (width <= 0)
        {
            return fail(fileType, "Binary art width must be greater than zero.");
        }

        if ((bytes.size() % 2u) != 0u)
        {
            return fail(fileType, "Binary art byte stream must contain glyph/attribute pairs.", baseByteOffset + bytes.size());
        }

        const std::size_t cellCount = bytes.size() / 2u;
        if ((cellCount % static_cast<std::size_t>(width)) != 0u)
        {
            return fail(fileType, "Binary art cell count does not align with the resolved width.");
        }

        const int height = static_cast<int>(cellCount / static_cast<std::size_t>(width));
        TextObjectBuilder builder(width, height);
        builder.fillAuthoredSpace(options.baseStyle);

        std::size_t offset = 0;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                const unsigned char glyphByte = static_cast<unsigned char>(bytes[offset++]);
                const unsigned char attrByte = static_cast<unsigned char>(bytes[offset++]);

                const std::optional<Style> style =
                    styleFromDosAttribute(attrByte, options.baseStyle, usedIceColors);

                const char32_t glyph = decodeCp437Byte(glyphByte);

                const bool wroteCell = (glyph == U' ')
                    ? builder.setAuthoredSpace(x, y, style)
                    : builder.setGlyph(x, y, glyph, style);

                if (!wroteCell)
                {
                    return fail(
                        fileType,
                        "Failed to populate TextObjectBuilder for binary art.",
                        baseByteOffset + offset,
                        glyph,
                        { x, y });
                }
            }
        }

        result.object = builder.build();
        result.success = true;
        result.resolvedWidth = width;
        result.resolvedHeight = height;
        return result;
    }

    bool decodeCompressedXBinCells(
        std::string_view bytes,
        std::size_t payloadOffset,
        int width,
        int height,
        std::vector<unsigned char>& outCellBytes,
        std::size_t& outConsumedBytes,
        std::size_t& outFailingByteOffset)
    {
        outCellBytes.clear();
        outConsumedBytes = 0;
        outFailingByteOffset = payloadOffset;

        if (width <= 0 || height <= 0)
        {
            return false;
        }

        const std::size_t expectedBytes =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) * 2u;

        outCellBytes.reserve(expectedBytes);

        std::size_t cursor = payloadOffset;
        while (outCellBytes.size() < expectedBytes)
        {
            if (cursor >= bytes.size())
            {
                outFailingByteOffset = cursor;
                return false;
            }

            const unsigned char control = static_cast<unsigned char>(bytes[cursor++]);
            const std::size_t runLength = static_cast<std::size_t>((control & 0x3F) + 1u);
            const unsigned char mode = static_cast<unsigned char>(control >> 6);

            if (mode == 0)
            {
                const std::size_t needed = runLength * 2u;
                if (cursor + needed > bytes.size())
                {
                    outFailingByteOffset = cursor;
                    return false;
                }

                if (outCellBytes.size() + needed > expectedBytes)
                {
                    outFailingByteOffset = cursor;
                    return false;
                }

                outCellBytes.insert(
                    outCellBytes.end(),
                    bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
                    bytes.begin() + static_cast<std::ptrdiff_t>(cursor + needed));

                cursor += needed;
                continue;
            }

            if (mode == 1)
            {
                if (cursor + runLength + 1 > bytes.size())
                {
                    outFailingByteOffset = cursor;
                    return false;
                }

                const unsigned char attr = static_cast<unsigned char>(bytes[cursor++]);
                if (outCellBytes.size() + (runLength * 2u) > expectedBytes)
                {
                    outFailingByteOffset = cursor;
                    return false;
                }

                for (std::size_t i = 0; i < runLength; ++i)
                {
                    outCellBytes.push_back(static_cast<unsigned char>(bytes[cursor++]));
                    outCellBytes.push_back(attr);
                }

                continue;
            }

            if (mode == 2)
            {
                if (cursor + runLength + 1 > bytes.size())
                {
                    outFailingByteOffset = cursor;
                    return false;
                }

                const unsigned char glyph = static_cast<unsigned char>(bytes[cursor++]);
                if (outCellBytes.size() + (runLength * 2u) > expectedBytes)
                {
                    outFailingByteOffset = cursor;
                    return false;
                }

                for (std::size_t i = 0; i < runLength; ++i)
                {
                    outCellBytes.push_back(glyph);
                    outCellBytes.push_back(static_cast<unsigned char>(bytes[cursor++]));
                }

                continue;
            }

            if (cursor + 2 > bytes.size())
            {
                outFailingByteOffset = cursor;
                return false;
            }

            const unsigned char glyph = static_cast<unsigned char>(bytes[cursor++]);
            const unsigned char attr = static_cast<unsigned char>(bytes[cursor++]);

            if (outCellBytes.size() + (runLength * 2u) > expectedBytes)
            {
                outFailingByteOffset = cursor;
                return false;
            }

            for (std::size_t i = 0; i < runLength; ++i)
            {
                outCellBytes.push_back(glyph);
                outCellBytes.push_back(attr);
            }
        }

        outConsumedBytes = cursor - payloadOffset;
        return true;
    }

    BinaryArtLoader::LoadResult loadBin(
        std::string_view bytes,
        const BinaryArtLoader::LoadOptions& options,
        const BinaryArtLoader::SauceMetadata& sauce)
    {
        int width = std::max(1, options.defaultColumns);
        if (options.preferSauceWidth && sauce.present && sauce.tInfo1 > 0)
        {
            width = static_cast<int>(sauce.tInfo1);
        }

        BinaryArtLoader::LoadResult result =
            decodeCellStream(
                bytes,
                width,
                options,
                BinaryArtLoader::FileType::Bin,
                options.enableIceColors,
                sauce);

        if (result.success &&
            options.preferSauceWidth &&
            sauce.present &&
            sauce.tInfo1 > 0)
        {
            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::SauceWidthOverrideApplied,
                "SAUCE width metadata was applied to binary art import.",
                0,
                {});
        }

        return result;
    }

    BinaryArtLoader::LoadResult loadXBin(
        std::string_view bytes,
        const BinaryArtLoader::LoadOptions& options,
        const BinaryArtLoader::SauceMetadata& sauce)
    {
        if (bytes.size() < 11)
        {
            return fail(BinaryArtLoader::FileType::XBin, "XBIN file is too small to contain a valid header.");
        }

        if (!(bytes[0] == 'X' &&
            bytes[1] == 'B' &&
            bytes[2] == 'I' &&
            bytes[3] == 'N' &&
            static_cast<unsigned char>(bytes[4]) == 0x1A))
        {
            return fail(BinaryArtLoader::FileType::XBin, "Missing XBIN signature.");
        }

        bool ok = false;
        const std::uint16_t width = readLe16(bytes, 5, ok);
        if (!ok)
        {
            return fail(BinaryArtLoader::FileType::XBin, "Failed to read XBIN width.", 5);
        }

        const std::uint16_t height = readLe16(bytes, 7, ok);
        if (!ok)
        {
            return fail(BinaryArtLoader::FileType::XBin, "Failed to read XBIN height.", 7);
        }

        const unsigned char fontHeight = static_cast<unsigned char>(bytes[9]);
        const unsigned char flags = static_cast<unsigned char>(bytes[10]);

        if (width == 0 || height == 0)
        {
            return fail(BinaryArtLoader::FileType::XBin, "XBIN width and height must be non-zero.", 5);
        }

        const bool hasPalette = (flags & 0x01) != 0;
        const bool hasFont = (flags & 0x02) != 0;
        const bool isCompressed = (flags & 0x04) != 0;
        const bool nonBlinkMode = (flags & 0x08) != 0;
        const bool has512Chars = (flags & 0x10) != 0;

        BinaryArtLoader::LoadResult result;
        result.detectedFileType = BinaryArtLoader::FileType::XBin;
        result.sauce = sauce;
        result.usedIceColors = nonBlinkMode || options.enableIceColors;

        std::size_t offset = 11;

        if (hasPalette)
        {
            if (offset + 48 > bytes.size())
            {
                return fail(BinaryArtLoader::FileType::XBin, "XBIN palette section is truncated.", offset);
            }

            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::PaletteDataIgnored,
                "XBIN palette data was present but is not yet imported into engine palette state.",
                offset,
                {});

            offset += 48;
        }

        if (hasFont)
        {
            const std::size_t glyphCount = has512Chars ? 512u : 256u;
            const std::size_t fontBytes = glyphCount * static_cast<std::size_t>(fontHeight);

            if (offset + fontBytes > bytes.size())
            {
                return fail(BinaryArtLoader::FileType::XBin, "XBIN font section is truncated.", offset);
            }

            addWarning(
                result,
                BinaryArtLoader::LoadWarningCode::FontDataIgnored,
                "XBIN font data was present but is not yet imported into engine glyph/font policy.",
                offset,
                {});

            offset += fontBytes;
        }

        if (isCompressed)
        {
            if (options.xbinCompressionSupport == BinaryArtLoader::XBinCompressionSupport::RejectCompressed)
            {
                return fail(
                    BinaryArtLoader::FileType::XBin,
                    "Compressed XBIN is disabled by current loader options.",
                    offset);
            }

            std::vector<unsigned char> decodedCellBytes;
            std::size_t consumedBytes = 0;
            std::size_t failingByteOffset = offset;

            if (!decodeCompressedXBinCells(
                bytes,
                offset,
                static_cast<int>(width),
                static_cast<int>(height),
                decodedCellBytes,
                consumedBytes,
                failingByteOffset))
            {
                return fail(
                    BinaryArtLoader::FileType::XBin,
                    "Failed to decode compressed XBIN payload.",
                    failingByteOffset);
            }

            BinaryArtLoader::LoadResult decoded =
                decodeCellStream(
                    std::string_view(
                        reinterpret_cast<const char*>(decodedCellBytes.data()),
                        decodedCellBytes.size()),
                    static_cast<int>(width),
                    options,
                    BinaryArtLoader::FileType::XBin,
                    result.usedIceColors,
                    sauce,
                    offset);

            decoded.warnings.insert(
                decoded.warnings.begin(),
                result.warnings.begin(),
                result.warnings.end());

            addWarning(
                decoded,
                BinaryArtLoader::LoadWarningCode::CompressedXBinDecoded,
                "Compressed XBIN payload was decoded successfully.",
                offset,
                {});

            const std::size_t payloadEnd = offset + consumedBytes;
            if (payloadEnd < bytes.size())
            {
                addWarning(
                    decoded,
                    BinaryArtLoader::LoadWarningCode::ExtraTrailingBytesIgnored,
                    "Extra trailing bytes after XBIN payload were ignored.",
                    payloadEnd,
                    {});
            }

            return decoded;
        }

        const std::size_t expectedCellBytes =
            static_cast<std::size_t>(width) *
            static_cast<std::size_t>(height) * 2u;

        if (offset + expectedCellBytes > bytes.size())
        {
            return fail(BinaryArtLoader::FileType::XBin, "XBIN cell payload is truncated.", offset);
        }

        BinaryArtLoader::LoadResult decoded =
            decodeCellStream(
                bytes.substr(offset, expectedCellBytes),
                static_cast<int>(width),
                options,
                BinaryArtLoader::FileType::XBin,
                result.usedIceColors,
                sauce,
                offset);

        decoded.warnings.insert(
            decoded.warnings.begin(),
            result.warnings.begin(),
            result.warnings.end());

        if (offset + expectedCellBytes < bytes.size())
        {
            addWarning(
                decoded,
                BinaryArtLoader::LoadWarningCode::ExtraTrailingBytesIgnored,
                "Extra trailing bytes after XBIN payload were ignored.",
                offset + expectedCellBytes,
                {});
        }

        return decoded;
    }

    BinaryArtLoader::LoadResult loadAdf(
        std::string_view bytes,
        const BinaryArtLoader::LoadOptions& options,
        const BinaryArtLoader::SauceMetadata& sauce)
    {
        BinaryArtLoader::LoadResult result;
        result.detectedFileType = BinaryArtLoader::FileType::Adf;
        result.sauce = sauce;
        result.usedIceColors = options.enableIceColors;

        if (bytes.empty())
        {
            return fail(BinaryArtLoader::FileType::Adf, "ADF file is empty.");
        }

        std::size_t offset = 0;
        bool ok = false;

        int width = 0;
        int height = 0;
        bool hasExplicitCellBlock = false;
        std::string_view cellBytes;

        const bool looksLikeChunkedAdf =
            bytes.size() >= 12 &&
            ((bytes[0] == 'A' && bytes[1] == 'D' && bytes[2] == 'F') ||
                (bytes[0] == 'a' && bytes[1] == 'd' && bytes[2] == 'f'));

        if (looksLikeChunkedAdf)
        {
            offset = 3;

            if (offset < bytes.size() && static_cast<unsigned char>(bytes[offset]) == 0x1A)
            {
                ++offset;
            }

            if (offset + 4 <= bytes.size())
            {
                const std::string_view magic = bytes.substr(offset, 4);
                if (magic == "MAIN" || magic == "main")
                {
                    offset += 4;
                }
            }

            while (offset + 8 <= bytes.size())
            {
                const std::string chunkId(bytes.substr(offset, 4));
                offset += 4;

                const std::uint32_t chunkSize = readLe32(bytes, offset, ok);
                if (!ok)
                {
                    return fail(BinaryArtLoader::FileType::Adf, "Failed to read ADF chunk size.", offset);
                }
                offset += 4;

                if (offset + chunkSize > bytes.size())
                {
                    return fail(BinaryArtLoader::FileType::Adf, "ADF chunk extends past end of file.", offset);
                }

                const std::string_view chunkData = bytes.substr(offset, chunkSize);

                if (chunkId == "CELL" || chunkId == "cell" || chunkId == "TEXT" || chunkId == "text")
                {
                    if (chunkSize < 4)
                    {
                        return fail(BinaryArtLoader::FileType::Adf, "ADF cell chunk is too small.", offset);
                    }

                    const std::uint16_t chunkWidth = readLe16(chunkData, 0, ok);
                    if (!ok)
                    {
                        return fail(BinaryArtLoader::FileType::Adf, "Failed to read ADF cell width.", offset);
                    }

                    const std::uint16_t chunkHeight = readLe16(chunkData, 2, ok);
                    if (!ok)
                    {
                        return fail(BinaryArtLoader::FileType::Adf, "Failed to read ADF cell height.", offset + 2);
                    }

                    if (chunkWidth == 0 || chunkHeight == 0)
                    {
                        return fail(BinaryArtLoader::FileType::Adf, "ADF width and height must be non-zero.", offset);
                    }

                    const std::size_t expectedBytes =
                        static_cast<std::size_t>(chunkWidth) *
                        static_cast<std::size_t>(chunkHeight) * 2u;

                    if (chunkSize < 4u + expectedBytes)
                    {
                        return fail(BinaryArtLoader::FileType::Adf, "ADF cell chunk payload is truncated.", offset + 4);
                    }

                    width = static_cast<int>(chunkWidth);
                    height = static_cast<int>(chunkHeight);
                    cellBytes = chunkData.substr(4, expectedBytes);
                    hasExplicitCellBlock = true;

                    if (4u + expectedBytes < chunkSize)
                    {
                        addWarning(
                            result,
                            BinaryArtLoader::LoadWarningCode::ExtraTrailingBytesIgnored,
                            "Extra bytes inside ADF cell chunk were ignored.",
                            offset + 4u + expectedBytes,
                            {});
                    }
                }
                else if (chunkId == "SIZE" || chunkId == "size")
                {
                    if (chunkSize >= 4)
                    {
                        const std::uint16_t chunkWidth = readLe16(chunkData, 0, ok);
                        if (!ok)
                        {
                            return fail(BinaryArtLoader::FileType::Adf, "Failed to read ADF SIZE width.", offset);
                        }

                        const std::uint16_t chunkHeight = readLe16(chunkData, 2, ok);
                        if (!ok)
                        {
                            return fail(BinaryArtLoader::FileType::Adf, "Failed to read ADF SIZE height.", offset + 2);
                        }

                        width = static_cast<int>(chunkWidth);
                        height = static_cast<int>(chunkHeight);
                    }
                }
                else
                {
                    addWarning(
                        result,
                        BinaryArtLoader::LoadWarningCode::AdfUnsupportedChunkIgnored,
                        "Unsupported ADF chunk was ignored: " + chunkId,
                        offset,
                        {});
                }

                offset += chunkSize;
            }
        }

        if (hasExplicitCellBlock)
        {
            BinaryArtLoader::LoadResult decoded =
                decodeCellStream(
                    cellBytes,
                    width,
                    options,
                    BinaryArtLoader::FileType::Adf,
                    options.enableIceColors,
                    sauce,
                    0);

            decoded.warnings.insert(
                decoded.warnings.begin(),
                result.warnings.begin(),
                result.warnings.end());

            return decoded;
        }

        if (options.preferSauceWidth && sauce.present && sauce.tInfo1 > 0)
        {
            width = static_cast<int>(sauce.tInfo1);
        }

        if (width <= 0)
        {
            width = std::max(1, options.defaultColumns);
        }

        if (height > 0)
        {
            const std::size_t expectedBytes =
                static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height) * 2u;

            if (expectedBytes <= bytes.size())
            {
                BinaryArtLoader::LoadResult decoded =
                    decodeCellStream(
                        bytes.substr(0, expectedBytes),
                        width,
                        options,
                        BinaryArtLoader::FileType::Adf,
                        options.enableIceColors,
                        sauce);

                decoded.warnings.insert(
                    decoded.warnings.begin(),
                    result.warnings.begin(),
                    result.warnings.end());

                if (expectedBytes < bytes.size())
                {
                    addWarning(
                        decoded,
                        BinaryArtLoader::LoadWarningCode::ExtraTrailingBytesIgnored,
                        "Extra trailing bytes after ADF payload were ignored.",
                        expectedBytes,
                        {});
                }

                return decoded;
            }
        }

        BinaryArtLoader::LoadResult fallback =
            decodeCellStream(
                bytes,
                width,
                options,
                BinaryArtLoader::FileType::Adf,
                options.enableIceColors,
                sauce);

        fallback.warnings.insert(
            fallback.warnings.begin(),
            result.warnings.begin(),
            result.warnings.end());

        return fallback;
    }
}

namespace BinaryArtLoader
{
    FileType detectFileType(const std::string& filePath)
    {
        const std::string extension = getFileExtensionLower(filePath);

        if (extension == ".bin")
        {
            return FileType::Bin;
        }

        if (extension == ".xbin" || extension == ".xb")
        {
            return FileType::XBin;
        }

        if (extension == ".adf")
        {
            return FileType::Adf;
        }

        return FileType::Unknown;
    }

    LoadResult loadFromFile(const std::string& filePath)
    {
        return loadFromFile(filePath, LoadOptions{});
    }

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options)
    {
        std::string bytes;
        if (!readAllBytes(filePath, bytes))
        {
            LoadResult result;
            result.success = false;
            result.detectedFileType = detectFileType(filePath);
            result.errorMessage = "Failed to open binary art file.";
            return result;
        }

        LoadOptions resolvedOptions = options;
        if (resolvedOptions.fileType == FileType::Auto)
        {
            resolvedOptions.fileType = detectFileType(filePath);
        }

        LoadResult result = loadFromBytes(bytes, resolvedOptions);

        if (result.detectedFileType == FileType::Unknown)
        {
            result.detectedFileType = detectFileType(filePath);
        }

        return result;
    }

    LoadResult loadFromFile(const std::string& filePath, const Style& style)
    {
        LoadOptions options;
        options.baseStyle = style;
        return loadFromFile(filePath, options);
    }

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject)
    {
        const LoadResult result = loadFromFile(filePath);
        if (!result.success)
        {
            return false;
        }

        outObject = result.object;
        return true;
    }

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const LoadOptions& options)
    {
        const LoadResult result = loadFromFile(filePath, options);
        if (!result.success)
        {
            return false;
        }

        outObject = result.object;
        return true;
    }

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const Style& style)
    {
        const LoadResult result = loadFromFile(filePath, style);
        if (!result.success)
        {
            return false;
        }

        outObject = result.object;
        return true;
    }

    LoadResult loadFromBytes(std::string_view bytes, const LoadOptions& options)
    {
        const auto sauceRecord = SauceSupport::parse(bytes);
        const std::string_view content = bytes.substr(0, sauceRecord.contentSize);

        LoadResult result;
        const FileType type = options.fileType;

        const bool isXBin =
            content.size() >= 5 &&
            content[0] == 'X' &&
            content[1] == 'B' &&
            content[2] == 'I' &&
            content[3] == 'N' &&
            static_cast<unsigned char>(content[4]) == 0x1A;

        const bool isLikelyAdf = looksLikeAdf(content);

        if (type == FileType::XBin || (type == FileType::Auto && isXBin))
        {
            result = loadXBin(content, options, sauceRecord.metadata);
        }
        else if (type == FileType::Adf || (type == FileType::Auto && isLikelyAdf))
        {
            result = loadAdf(content, options, sauceRecord.metadata);
        }
        else if (type == FileType::Bin || type == FileType::Auto || type == FileType::Unknown)
        {
            result = loadBin(content, options, sauceRecord.metadata);
        }
        else
        {
            result.success = false;
            result.detectedFileType = FileType::Unknown;
            result.errorMessage = "Unsupported binary art file type.";
        }

        appendSauceWarnings(result, options, sauceRecord);

        if (result.success &&
            options.preferSauceWidth &&
            result.sauce.present &&
            result.detectedFileType != FileType::XBin &&
            result.sauce.tInfo1 > 0)
        {
            addWarning(
                result,
                LoadWarningCode::SauceWidthOverrideApplied,
                "SAUCE width metadata was applied to binary art import.",
                0,
                {});
        }

        return result;
    }

    bool hasWarning(const LoadResult& result, LoadWarningCode code)
    {
        for (const LoadWarning& warning : result.warnings)
        {
            if (warning.code == code)
            {
                return true;
            }
        }

        return false;
    }

    const LoadWarning* getWarningByCode(const LoadResult& result, LoadWarningCode code)
    {
        for (const LoadWarning& warning : result.warnings)
        {
            if (warning.code == code)
            {
                return &warning;
            }
        }

        return nullptr;
    }

    std::string formatLoadError(const LoadResult& result)
    {
        std::ostringstream stream;
        stream << "Binary art load failed";
        if (!result.errorMessage.empty())
        {
            stream << ": " << result.errorMessage;
        }

        if (result.hasParseFailure)
        {
            stream << " [byte=" << result.failingByteOffset;

            if (result.firstFailingPosition.isValid())
            {
                stream << ", x=" << result.firstFailingPosition.x
                    << ", y=" << result.firstFailingPosition.y;
            }

            if (result.firstFailingCodePoint != U'\0')
            {
                stream << ", codepoint=U+"
                    << std::hex
                    << std::uppercase
                    << static_cast<std::uint32_t>(result.firstFailingCodePoint)
                    << std::dec;
            }

            stream << "]";
        }

        return stream.str();
    }

    std::string formatLoadSuccess(const LoadResult& result)
    {
        std::ostringstream stream;
        stream << "Binary art load succeeded: "
            << result.resolvedWidth << "x" << result.resolvedHeight
            << ", type=" << toString(result.detectedFileType);

        if (result.usedIceColors)
        {
            stream << ", iceColors=true";
        }

        if (result.sauce.present)
        {
            stream << ", SAUCE title=\"" << result.sauce.title << "\"";
        }

        if (!result.sauce.comments.empty())
        {
            stream << ", sauceComments=" << result.sauce.comments.size();
        }

        if (!result.warnings.empty())
        {
            stream << ", warnings=" << result.warnings.size();
        }

        return stream.str();
    }

    const char* toString(FileType fileType)
    {
        switch (fileType)
        {
        case FileType::Auto: return "Auto";
        case FileType::Unknown: return "Unknown";
        case FileType::Bin: return "Bin";
        case FileType::XBin: return "XBin";
        case FileType::Adf: return "Adf";
        default: return "Unknown";
        }
    }

    const char* toString(LoadWarningCode warningCode)
    {
        switch (warningCode)
        {
        case LoadWarningCode::None: return "None";
        case LoadWarningCode::SauceMetadataPresent: return "SauceMetadataPresent";
        case LoadWarningCode::SauceCommentsImported: return "SauceCommentsImported";
        case LoadWarningCode::SauceWidthOverrideApplied: return "SauceWidthOverrideApplied";
        case LoadWarningCode::InvalidSauceCommentBlockIgnored: return "InvalidSauceCommentBlockIgnored";
        case LoadWarningCode::TruncatedSauceIgnored: return "TruncatedSauceIgnored";
        case LoadWarningCode::PaletteDataIgnored: return "PaletteDataIgnored";
        case LoadWarningCode::FontDataIgnored: return "FontDataIgnored";
        case LoadWarningCode::ExtraTrailingBytesIgnored: return "ExtraTrailingBytesIgnored";
        case LoadWarningCode::CompressedXBinDecoded: return "CompressedXBinDecoded";
        case LoadWarningCode::AdfUnsupportedChunkIgnored: return "AdfUnsupportedChunkIgnored";
        default: return "Unknown";
        }
    }

    const char* toString(XBinCompressionSupport mode)
    {
        switch (mode)
        {
        case XBinCompressionSupport::DecodeCompressed: return "DecodeCompressed";
        case XBinCompressionSupport::RejectCompressed: return "RejectCompressed";
        default: return "Unknown";
        }
    }
}