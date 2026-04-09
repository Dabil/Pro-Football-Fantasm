#include "Rendering/Objects/SauceSupport.h"

namespace SauceSupport
{
    std::string trimRightAscii(const std::string& value)
    {
        std::size_t end = value.size();
        while (end > 0 && (value[end - 1] == ' ' || value[end - 1] == '\0'))
        {
            --end;
        }

        return value.substr(0, end);
    }

    static std::uint16_t readLe16(std::string_view bytes, std::size_t offset, bool& ok)
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

    SauceParseResult parse(std::string_view bytes)
    {
        SauceParseResult result;
        result.contentSize = bytes.size();

        if (bytes.size() < 128)
        {
            return result;
        }

        const std::size_t offset = bytes.size() - 128;
        const std::string_view sauce = bytes.substr(offset, 128);

        if (sauce.substr(0, 5) != "SAUCE")
        {
            return result;
        }

        result.found = true;
        result.contentSize = offset;

        auto& m = result.metadata;
        m.present = true;

        m.title = trimRightAscii(std::string(sauce.substr(7, 35)));
        m.author = trimRightAscii(std::string(sauce.substr(42, 20)));
        m.group = trimRightAscii(std::string(sauce.substr(62, 20)));
        m.date = trimRightAscii(std::string(sauce.substr(82, 8)));

        m.dataType = static_cast<std::uint8_t>(sauce[94]);
        m.fileType = static_cast<std::uint8_t>(sauce[95]);

        bool ok = false;
        m.tInfo1 = readLe16(sauce, 96, ok);
        m.tInfo2 = readLe16(sauce, 98, ok);

        m.commentLineCount = static_cast<std::uint8_t>(sauce[104]);
        m.flags = static_cast<std::uint8_t>(sauce[105]);

        m.fontName = trimRightAscii(std::string(sauce.substr(106, 22)));

        if (m.commentLineCount > 0)
        {
            const std::size_t commentBytes =
                5u + (static_cast<std::size_t>(m.commentLineCount) * 64u);

            if (result.contentSize < commentBytes)
            {
                result.truncated = true;
                return result;
            }

            const std::size_t commentOffset = result.contentSize - commentBytes;
            const std::string_view marker = bytes.substr(commentOffset, 5);

            if (marker == "COMNT")
            {
                std::size_t ptr = commentOffset + 5;

                for (std::uint8_t i = 0; i < m.commentLineCount; ++i)
                {
                    if (ptr + 64 > bytes.size())
                    {
                        result.truncated = true;
                        m.comments.clear();
                        return result;
                    }

                    std::string line =
                        trimRightAscii(std::string(bytes.substr(ptr, 64)));

                    m.comments.push_back(line);
                    ptr += 64;
                }

                result.contentSize = commentOffset;
            }
            else
            {
                result.invalidCommentBlock = true;
            }
        }

        return result;
    }
}