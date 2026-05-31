#include "Rendering/Objects/PlainTextLoader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

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

    bool startsWithUtf8Bom(std::string_view bytes)
    {
        return bytes.size() >= 3 &&
            static_cast<unsigned char>(bytes[0]) == 0xEF &&
            static_cast<unsigned char>(bytes[1]) == 0xBB &&
            static_cast<unsigned char>(bytes[2]) == 0xBF;
    }

    bool isContinuationByte(unsigned char byte)
    {
        return (byte & 0xC0) == 0x80;
    }

    bool isValidUtf8(std::string_view bytes)
    {
        std::size_t i = 0;

        while (i < bytes.size())
        {
            const unsigned char b0 = static_cast<unsigned char>(bytes[i]);

            if (b0 <= 0x7F)
            {
                ++i;
                continue;
            }

            if ((b0 & 0xE0) == 0xC0)
            {
                if (i + 1 >= bytes.size())
                {
                    return false;
                }

                const unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
                if (!isContinuationByte(b1))
                {
                    return false;
                }

                const char32_t codePoint =
                    (static_cast<char32_t>(b0 & 0x1F) << 6) |
                    static_cast<char32_t>(b1 & 0x3F);

                if (codePoint < 0x80)
                {
                    return false;
                }

                i += 2;
                continue;
            }

            if ((b0 & 0xF0) == 0xE0)
            {
                if (i + 2 >= bytes.size())
                {
                    return false;
                }

                const unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
                const unsigned char b2 = static_cast<unsigned char>(bytes[i + 2]);

                if (!isContinuationByte(b1) || !isContinuationByte(b2))
                {
                    return false;
                }

                const char32_t codePoint =
                    (static_cast<char32_t>(b0 & 0x0F) << 12) |
                    (static_cast<char32_t>(b1 & 0x3F) << 6) |
                    static_cast<char32_t>(b2 & 0x3F);

                if (codePoint < 0x800)
                {
                    return false;
                }

                if (codePoint >= 0xD800 && codePoint <= 0xDFFF)
                {
                    return false;
                }

                i += 3;
                continue;
            }

            if ((b0 & 0xF8) == 0xF0)
            {
                if (i + 3 >= bytes.size())
                {
                    return false;
                }

                const unsigned char b1 = static_cast<unsigned char>(bytes[i + 1]);
                const unsigned char b2 = static_cast<unsigned char>(bytes[i + 2]);
                const unsigned char b3 = static_cast<unsigned char>(bytes[i + 3]);

                if (!isContinuationByte(b1) ||
                    !isContinuationByte(b2) ||
                    !isContinuationByte(b3))
                {
                    return false;
                }

                const char32_t codePoint =
                    (static_cast<char32_t>(b0 & 0x07) << 18) |
                    (static_cast<char32_t>(b1 & 0x3F) << 12) |
                    (static_cast<char32_t>(b2 & 0x3F) << 6) |
                    static_cast<char32_t>(b3 & 0x3F);

                if (codePoint < 0x10000 || codePoint > 0x10FFFF)
                {
                    return false;
                }

                i += 4;
                continue;
            }

            return false;
        }

        return true;
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

    std::u32string decodeAscii(std::string_view bytes)
    {
        std::u32string result;
        result.reserve(bytes.size());

        for (unsigned char byte : bytes)
        {
            if (byte <= 0x7F)
            {
                result.push_back(UnicodeConversion::sanitizeCodePoint(static_cast<char32_t>(byte)));
            }
            else
            {
                result.push_back(U'\uFFFD');
            }
        }

        return result;
    }

    std::u32string decodeLatin1(std::string_view bytes)
    {
        std::u32string result;
        result.reserve(bytes.size());

        for (unsigned char byte : bytes)
        {
            result.push_back(UnicodeConversion::sanitizeCodePoint(static_cast<char32_t>(byte)));
        }

        return result;
    }

    std::u32string decodeCp437(std::string_view bytes)
    {
        std::u32string result;
        result.reserve(bytes.size());

        for (unsigned char byte : bytes)
        {
            result.push_back(decodeCp437Byte(byte));
        }

        return result;
    }

    std::u32string normalizeLineEndings(std::u32string_view text, bool& changed)
    {
        changed = false;

        std::u32string result;
        result.reserve(text.size());

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            const char32_t ch = text[i];

            if (ch == U'\r')
            {
                changed = true;
                result.push_back(U'\n');

                if (i + 1 < text.size() && text[i + 1] == U'\n')
                {
                    ++i;
                }

                continue;
            }

            result.push_back(ch);
        }

        return result;
    }

    std::u32string trimTrailingSpaces(std::u32string_view text)
    {
        std::u32string result;
        result.reserve(text.size());

        std::u32string currentLine;

        for (char32_t ch : text)
        {
            if (ch == U'\n')
            {
                while (!currentLine.empty() && currentLine.back() == U' ')
                {
                    currentLine.pop_back();
                }

                result.append(currentLine);
                result.push_back(U'\n');
                currentLine.clear();
                continue;
            }

            currentLine.push_back(ch);
        }

        while (!currentLine.empty() && currentLine.back() == U' ')
        {
            currentLine.pop_back();
        }

        result.append(currentLine);
        return result;
    }

    std::u32string expandTabs(std::u32string_view text, int tabWidth)
    {
        if (tabWidth <= 0)
        {
            return std::u32string(text);
        }

        std::u32string result;
        result.reserve(text.size());

        int column = 0;

        for (char32_t ch : text)
        {
            if (ch == U'\n')
            {
                result.push_back(ch);
                column = 0;
                continue;
            }

            if (ch == U'\t')
            {
                const int spacesToAdd = tabWidth - (column % tabWidth);
                for (int i = 0; i < spacesToAdd; ++i)
                {
                    result.push_back(U' ');
                }

                column += spacesToAdd;
                continue;
            }

            result.push_back(ch);
            ++column;
        }

        return result;
    }

    PlainTextLoader::Encoding resolveEncoding(
        std::string_view bytes,
        PlainTextLoader::FileType fileType,
        PlainTextLoader::Encoding requestedEncoding,
        bool& hadUtf8Bom)
    {
        hadUtf8Bom = false;

        if (requestedEncoding != PlainTextLoader::Encoding::Auto)
        {
            if (requestedEncoding == PlainTextLoader::Encoding::Utf8 && startsWithUtf8Bom(bytes))
            {
                hadUtf8Bom = true;
            }

            return requestedEncoding;
        }

        if (startsWithUtf8Bom(bytes))
        {
            hadUtf8Bom = true;
            return PlainTextLoader::Encoding::Utf8;
        }

        if (isValidUtf8(bytes))
        {
            return PlainTextLoader::Encoding::Utf8;
        }

        if (fileType == PlainTextLoader::FileType::Nfo ||
            fileType == PlainTextLoader::FileType::Diz)
        {
            return PlainTextLoader::Encoding::Cp437;
        }

        return PlainTextLoader::Encoding::Ascii;
    }

    std::u32string decodeBytes(
        std::string_view bytes,
        PlainTextLoader::Encoding encoding,
        bool hadUtf8Bom)
    {
        std::string_view payload = bytes;

        if (encoding == PlainTextLoader::Encoding::Utf8 && hadUtf8Bom && bytes.size() >= 3)
        {
            payload.remove_prefix(3);
        }

        switch (encoding)
        {
        case PlainTextLoader::Encoding::Utf8:
            return UnicodeConversion::utf8ToU32(payload);

        case PlainTextLoader::Encoding::Ascii:
            return decodeAscii(payload);

        case PlainTextLoader::Encoding::Latin1:
            return decodeLatin1(payload);

        case PlainTextLoader::Encoding::Cp437:
            return decodeCp437(payload);

        case PlainTextLoader::Encoding::Auto:
        default:
            return UnicodeConversion::utf8ToU32(payload);
        }
    }

    TextObject makeTextObject(std::u32string_view text, const PlainTextLoader::LoadOptions& options)
    {
        if (options.style.has_value())
        {
            return TextObject::fromU32(text, *options.style);
        }

        return TextObject::fromU32(text);
    }
}

namespace PlainTextLoader
{
    FileType detectFileType(const std::string& filePath)
    {
        const std::string ext = getFileExtensionLower(filePath);

        if (ext == ".asc")
        {
            return FileType::Asc;
        }

        if (ext == ".txt")
        {
            return FileType::Txt;
        }

        if (ext == ".nfo")
        {
            return FileType::Nfo;
        }

        if (ext == ".diz")
        {
            return FileType::Diz;
        }

        return FileType::Unknown;
    }

    LoadResult loadFromFile(const std::string& filePath)
    {
        return loadFromFile(filePath, LoadOptions{});
    }

    LoadResult loadFromFile(const std::string& filePath, const Style& style)
    {
        LoadOptions options;
        options.style = style;
        return loadFromFile(filePath, options);
    }

    LoadResult loadFromFile(const std::string& filePath, const LoadOptions& options)
    {
        LoadResult result;
        result.detectedFileType =
            options.fileType == FileType::Auto
            ? detectFileType(filePath)
            : options.fileType;

        std::string bytes;
        if (!readAllBytes(filePath, bytes))
        {
            return result;
        }

        bool hadUtf8Bom = false;
        result.resolvedEncoding =
            resolveEncoding(bytes, result.detectedFileType, options.encoding, hadUtf8Bom);
        result.hadUtf8Bom = hadUtf8Bom;

        std::u32string text = decodeBytes(bytes, result.resolvedEncoding, result.hadUtf8Bom);

        if (options.normalizeLineEndings)
        {
            bool normalized = false;
            text = normalizeLineEndings(text, normalized);
            result.normalizedLineEndings = normalized;
        }

        if (options.expandTabs)
        {
            text = ::expandTabs(text, options.tabWidth);
        }

        if (!options.preserveTrailingSpaces)
        {
            text = trimTrailingSpaces(text);
        }

        result.object = makeTextObject(text, options);
        result.success = result.object.isLoaded();
        return result;
    }

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject)
    {
        return tryLoadFromFile(filePath, outObject, LoadOptions{});
    }

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const Style& style)
    {
        LoadOptions options;
        options.style = style;
        return tryLoadFromFile(filePath, outObject, options);
    }

    bool tryLoadFromFile(const std::string& filePath, TextObject& outObject, const LoadOptions& options)
    {
        const LoadResult result = loadFromFile(filePath, options);
        outObject = result.object;
        return result.success;
    }

    LoadResult loadFromBytesResult(std::string_view bytes, const LoadOptions& options)
    {
        LoadResult result;
        result.detectedFileType =
            options.fileType == FileType::Auto
            ? FileType::Unknown
            : options.fileType;

        bool hadUtf8Bom = false;
        result.resolvedEncoding =
            resolveEncoding(bytes, result.detectedFileType, options.encoding, hadUtf8Bom);
        result.hadUtf8Bom = hadUtf8Bom;

        std::u32string text = decodeBytes(bytes, result.resolvedEncoding, result.hadUtf8Bom);

        if (options.normalizeLineEndings)
        {
            bool normalized = false;
            text = ::normalizeLineEndings(text, normalized);
            result.normalizedLineEndings = normalized;
        }

        if (options.expandTabs)
        {
            text = ::expandTabs(text, options.tabWidth);
        }

        if (!options.preserveTrailingSpaces)
        {
            text = trimTrailingSpaces(text);
        }

        result.object = makeTextObject(text, options);
        result.success = result.object.isLoaded();
        return result;
    }

    TextObject loadFromBytes(std::string_view bytes, const LoadOptions& options)
    {
        return loadFromBytesResult(bytes, options).object;
    }

    std::u32string decodeToU32(std::string_view bytes, const LoadOptions& options)
    {
        const FileType fileType =
            options.fileType == FileType::Auto
            ? FileType::Unknown
            : options.fileType;

        bool hadUtf8Bom = false;
        const Encoding encoding = resolveEncoding(bytes, fileType, options.encoding, hadUtf8Bom);

        std::u32string text = decodeBytes(bytes, encoding, hadUtf8Bom);

        if (options.normalizeLineEndings)
        {
            bool normalized = false;
            text = ::normalizeLineEndings(text, normalized);
        }

        if (options.expandTabs)
        {
            text = ::expandTabs(text, options.tabWidth);
        }

        if (!options.preserveTrailingSpaces)
        {
            text = trimTrailingSpaces(text);
        }

        return text;
    }

    const char* toString(FileType fileType)
    {
        switch (fileType)
        {
        case FileType::Auto:
            return "Auto";
        case FileType::Unknown:
            return "Unknown";
        case FileType::Asc:
            return "ASC";
        case FileType::Txt:
            return "TXT";
        case FileType::Nfo:
            return "NFO";
        case FileType::Diz:
            return "DIZ";
        default:
            return "Unknown";
        }
    }

    const char* toString(Encoding encoding)
    {
        switch (encoding)
        {
        case Encoding::Auto:
            return "Auto";
        case Encoding::Utf8:
            return "UTF-8";
        case Encoding::Ascii:
            return "ASCII";
        case Encoding::Latin1:
            return "Latin-1";
        case Encoding::Cp437:
            return "CP437";
        default:
            return "Unknown";
        }
    }
}