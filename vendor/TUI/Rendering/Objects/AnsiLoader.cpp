#include "Rendering/Objects/AnsiLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "Rendering/Objects/TextObjectBuilder.h"
#include "Rendering/Styles/Color.h"
#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"

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

    Color::Basic ansiBasicToColor(int value)
    {
        switch (value)
        {
        case 0: return Color::Basic::Black;
        case 1: return Color::Basic::Red;
        case 2: return Color::Basic::Green;
        case 3: return Color::Basic::Yellow;
        case 4: return Color::Basic::Blue;
        case 5: return Color::Basic::Magenta;
        case 6: return Color::Basic::Cyan;
        case 7: return Color::Basic::White;
        default: return Color::Basic::White;
        }
    }

    Color::Basic ansiBrightBasicToColor(int value)
    {
        switch (value)
        {
        case 0: return Color::Basic::BrightBlack;
        case 1: return Color::Basic::BrightRed;
        case 2: return Color::Basic::BrightGreen;
        case 3: return Color::Basic::BrightYellow;
        case 4: return Color::Basic::BrightBlue;
        case 5: return Color::Basic::BrightMagenta;
        case 6: return Color::Basic::BrightCyan;
        case 7: return Color::Basic::BrightWhite;
        default: return Color::Basic::BrightWhite;
        }
    }

    void addWarning(
        AnsiLoader::LoadResult& result,
        AnsiLoader::LoadWarningCode code,
        const std::string& message,
        std::size_t byteOffset,
        const AnsiLoader::SourcePosition& position)
    {
        AnsiLoader::LoadWarning warning;
        warning.code = code;
        warning.message = message;
        warning.byteOffset = byteOffset;
        warning.sourcePosition = position;
        result.warnings.push_back(warning);
    }

    class Parser
    {
    public:
        Parser(
            const AnsiLoader::LoadOptions& options,
            const SauceSupport::SauceParseResult& sauce)
            : m_options(options)
            , m_sauce(sauce)
        {
            m_width = options.defaultColumns;
            if (options.preferSauceWidth && sauce.metadata.present && sauce.metadata.tInfo1 > 0)
            {
                m_width = static_cast<int>(sauce.metadata.tInfo1);
            }

            if (m_width <= 0)
            {
                m_width = 80;
            }

            if (m_width > m_options.maxColumns)
            {
                m_width = m_options.maxColumns;
            }

            m_cursorStyle = options.baseStyle;
        }

        AnsiLoader::LoadResult parse(std::string_view bytes)
        {
            preScanDimensions(bytes);

            if (m_height <= 0)
            {
                m_height = 1;
            }

            TextObjectBuilder builder(m_width, m_height);
            builder.fillAuthoredSpace(m_cursorStyle);

            for (std::size_t i = 0; i < bytes.size(); ++i)
            {
                const unsigned char byte = static_cast<unsigned char>(bytes[i]);

                if (byte == 0x1B)
                {
                    if (!consumeEscape(bytes, i))
                    {
                        return fail("Malformed or unsupported ANSI escape sequence.", i);
                    }

                    continue;
                }

                if (byte == '\r')
                {
                    m_cursorX = 0;
                    continue;
                }

                if (byte == '\n')
                {
                    if (m_options.treatBareLfAsNewLine)
                    {
                        m_cursorX = 0;
                        ++m_cursorY;
                        clampCursor(i);
                    }

                    continue;
                }

                if (byte == '\b')
                {
                    m_cursorX = std::max(0, m_cursorX - 1);
                    continue;
                }

                if (byte == '\t')
                {
                    m_cursorX = ((m_cursorX / 8) + 1) * 8;
                    clampCursor(i);
                    continue;
                }

                const char32_t glyph = decodeCp437Byte(byte);
                if (!writeGlyph(builder, glyph, i))
                {
                    return fail("ANSI content exceeded configured canvas bounds.", i, glyph);
                }
            }

            AnsiLoader::LoadResult result;
            result.object = builder.build();
            result.success = true;
            result.detectedFileType = AnsiLoader::FileType::Ans;
            result.resolvedWidth = result.object.getWidth();
            result.resolvedHeight = result.object.getHeight();
            result.sauce = m_sauce.metadata;

            if (m_sauce.metadata.present)
            {
                addWarning(
                    result,
                    AnsiLoader::LoadWarningCode::SauceMetadataPresent,
                    "SAUCE metadata was detected and parsed.",
                    m_sauce.contentSize,
                    {});

                if (m_options.preferSauceWidth && m_sauce.metadata.tInfo1 > 0)
                {
                    addWarning(
                        result,
                        AnsiLoader::LoadWarningCode::SauceWidthOverrideApplied,
                        "SAUCE width metadata was applied to ANSI canvas width.",
                        m_sauce.contentSize,
                        {});
                }

                if (m_options.importSauceComments && !m_sauce.metadata.comments.empty())
                {
                    addWarning(
                        result,
                        AnsiLoader::LoadWarningCode::SauceCommentsImported,
                        "SAUCE comments were parsed and imported into metadata.",
                        m_sauce.contentSize,
                        {});
                }

                if (m_sauce.invalidCommentBlock)
                {
                    addWarning(
                        result,
                        AnsiLoader::LoadWarningCode::InvalidSauceCommentBlockIgnored,
                        "SAUCE declared a comment block, but the COMNT marker was invalid and the block was ignored.",
                        m_sauce.contentSize,
                        {});
                }
            }

            if (m_sauce.truncated)
            {
                addWarning(
                    result,
                    AnsiLoader::LoadWarningCode::TruncatedSauceIgnored,
                    "SAUCE comment metadata appeared truncated and was partially ignored.",
                    m_sauce.contentSize,
                    {});
            }

            for (const LoadWarning& warning : m_warnings)
            {
                result.warnings.push_back(warning);
            }

            return result;
        }

    private:
        using LoadWarning = AnsiLoader::LoadWarning;
        using LoadWarningCode = AnsiLoader::LoadWarningCode;
        using SourcePosition = AnsiLoader::SourcePosition;

        void addRuntimeWarning(
            LoadWarningCode code,
            const std::string& message,
            std::size_t byteOffset,
            const SourcePosition& position)
        {
            LoadWarning warning;
            warning.code = code;
            warning.message = message;
            warning.byteOffset = byteOffset;
            warning.sourcePosition = position;
            m_warnings.push_back(warning);
        }

        void preScanDimensions(std::string_view bytes)
        {
            int x = 0;
            int y = 0;

            m_height = 1;

            for (std::size_t i = 0; i < bytes.size(); ++i)
            {
                const unsigned char byte = static_cast<unsigned char>(bytes[i]);

                if (byte == 0x1B)
                {
                    if (i + 1 < bytes.size() && bytes[i + 1] == '[')
                    {
                        std::size_t cursor = i + 2;
                        while (cursor < bytes.size())
                        {
                            const unsigned char ch = static_cast<unsigned char>(bytes[cursor]);
                            if (ch >= 0x40 && ch <= 0x7E)
                            {
                                i = cursor;
                                break;
                            }

                            ++cursor;
                        }

                        if (cursor >= bytes.size())
                        {
                            break;
                        }
                    }
                    else if (i + 1 < bytes.size())
                    {
                        ++i;
                    }

                    continue;
                }

                if (byte == '\r')
                {
                    x = 0;
                    continue;
                }

                if (byte == '\n')
                {
                    if (m_options.treatBareLfAsNewLine)
                    {
                        x = 0;
                        ++y;
                        m_height = std::max(m_height, y + 1);
                    }

                    continue;
                }

                if (byte == '\b')
                {
                    x = std::max(0, x - 1);
                    continue;
                }

                if (byte == '\t')
                {
                    x = ((x / 8) + 1) * 8;
                    if (m_options.wrapAtColumnBoundary && x >= m_width)
                    {
                        x = 0;
                        ++y;
                        m_height = std::max(m_height, y + 1);
                    }
                    continue;
                }

                const char32_t glyph = decodeCp437Byte(byte);
                const CellWidth measuredWidth = UnicodeWidth::measureCodePointWidth(glyph);
                if (measuredWidth == CellWidth::Zero)
                {
                    continue;
                }

                const int glyphWidth = (measuredWidth == CellWidth::Two) ? 2 : 1;

                if (m_options.wrapAtColumnBoundary && x + glyphWidth > m_width && x > 0)
                {
                    x = 0;
                    ++y;
                }

                x += glyphWidth;
                m_height = std::max(m_height, y + 1);
            }

            if (m_height > m_options.maxRows)
            {
                m_height = m_options.maxRows;
            }
        }

        bool consumeEscape(std::string_view bytes, std::size_t& ioIndex)
        {
            if (ioIndex + 1 >= bytes.size())
            {
                addRuntimeWarning(
                    LoadWarningCode::BareEscapeIgnored,
                    "Trailing ESC byte was ignored.",
                    ioIndex,
                    currentPosition());

                if (m_options.strictUnsupportedCommands)
                {
                    return false;
                }

                return true;
            }

            const unsigned char next = static_cast<unsigned char>(bytes[ioIndex + 1]);
            if (next != '[')
            {
                addRuntimeWarning(
                    LoadWarningCode::UnsupportedSequenceIgnored,
                    "Non-CSI ESC sequence was ignored.",
                    ioIndex,
                    currentPosition());

                if (m_options.strictUnsupportedCommands)
                {
                    return false;
                }

                ++ioIndex;
                return true;
            }

            std::size_t cursor = ioIndex + 2;
            std::string payload;

            while (cursor < bytes.size())
            {
                const unsigned char ch = static_cast<unsigned char>(bytes[cursor]);
                if (ch >= 0x40 && ch <= 0x7E)
                {
                    const char finalByte = static_cast<char>(ch);
                    ioIndex = cursor;
                    return applyCsi(payload, finalByte, ioIndex);
                }

                payload.push_back(static_cast<char>(ch));
                ++cursor;
            }

            return false;
        }

        bool applyCsi(const std::string& payload, char finalByte, std::size_t byteOffset)
        {
            bool privateMode = false;
            std::string body = payload;

            if (!body.empty() && (body[0] == '?' || body[0] == '>' || body[0] == '='))
            {
                privateMode = true;
                body.erase(body.begin());
            }

            if (privateMode)
            {
                addRuntimeWarning(
                    LoadWarningCode::UnsupportedPrivateSequenceIgnored,
                    "ANSI private CSI sequence was ignored.",
                    byteOffset,
                    currentPosition());

                return !m_options.strictUnsupportedCommands;
            }

            const std::vector<int> params = parseParams(body);

            switch (finalByte)
            {
            case 'm':
                return applySgr(params, byteOffset);

            case 'A':
                moveRelative(0, -firstOr(params, 1), byteOffset);
                return true;

            case 'B':
                moveRelative(0, firstOr(params, 1), byteOffset);
                return true;

            case 'C':
                moveRelative(firstOr(params, 1), 0, byteOffset);
                return true;

            case 'D':
                moveRelative(-firstOr(params, 1), 0, byteOffset);
                return true;

            case 'H':
            case 'f':
                moveAbsolute(firstOr(params, 1) - 1, getParam(params, 1, 1) - 1, byteOffset);
                return true;

            case 'G':
                setColumn(firstOr(params, 1) - 1, byteOffset);
                return true;

            case 'J':
                return eraseDisplay(firstOr(params, 0));

            case 'K':
                return eraseLine(firstOr(params, 0));

            case 's':
                m_savedCursorX = m_cursorX;
                m_savedCursorY = m_cursorY;
                m_savedStyle = m_cursorStyle;
                return true;

            case 'u':
                m_cursorX = m_savedCursorX;
                m_cursorY = m_savedCursorY;
                m_cursorStyle = m_savedStyle;
                clampCursor(byteOffset);
                return true;

            default:
                addRuntimeWarning(
                    LoadWarningCode::UnsupportedSequenceIgnored,
                    std::string("Unsupported CSI final byte ignored: ") + finalByte,
                    byteOffset,
                    currentPosition());

                return !m_options.strictUnsupportedCommands;
            }
        }

        std::vector<int> parseParams(const std::string& body) const
        {
            if (body.empty())
            {
                return {};
            }

            std::vector<int> params;
            std::string current;

            for (char ch : body)
            {
                if (ch == ';')
                {
                    params.push_back(current.empty() ? 0 : std::stoi(current));
                    current.clear();
                    continue;
                }

                if (std::isdigit(static_cast<unsigned char>(ch)))
                {
                    current.push_back(ch);
                    continue;
                }

                params.push_back(0);
                current.clear();
            }

            params.push_back(current.empty() ? 0 : std::stoi(current));
            return params;
        }

        int firstOr(const std::vector<int>& params, int fallback) const
        {
            return params.empty() ? fallback : (params[0] == 0 ? fallback : params[0]);
        }

        int getParam(const std::vector<int>& params, std::size_t index, int fallback) const
        {
            if (index >= params.size())
            {
                return fallback;
            }

            return params[index] == 0 ? fallback : params[index];
        }

        void moveRelative(int dx, int dy, std::size_t byteOffset)
        {
            m_cursorX += dx;
            m_cursorY += dy;
            clampCursor(byteOffset);
        }

        void moveAbsolute(int row, int column, std::size_t byteOffset)
        {
            m_cursorY = std::max(0, row);
            m_cursorX = std::max(0, column);
            clampCursor(byteOffset);
        }

        void setColumn(int column, std::size_t byteOffset)
        {
            m_cursorX = std::max(0, column);
            clampCursor(byteOffset);
        }

        void clampCursor(std::size_t byteOffset)
        {
            if (!m_options.clampCursorToBounds)
            {
                return;
            }

            const int oldX = m_cursorX;
            const int oldY = m_cursorY;

            m_cursorX = std::clamp(m_cursorX, 0, std::max(0, m_width - 1));
            m_cursorY = std::clamp(m_cursorY, 0, std::max(0, m_height - 1));

            if (m_cursorX != oldX || m_cursorY != oldY)
            {
                addRuntimeWarning(
                    LoadWarningCode::CursorClamped,
                    "Cursor movement exceeded configured bounds and was clamped.",
                    byteOffset,
                    currentPosition());
            }
        }

        bool eraseDisplay(int mode)
        {
            if (!m_builderInitialized)
            {
                return false;
            }

            if (mode == 2)
            {
                for (int y = 0; y < m_height; ++y)
                {
                    for (int x = 0; x < m_width; ++x)
                    {
                        m_builder->setGlyph(x, y, U' ', m_cursorStyle);
                    }
                }

                return true;
            }

            if (mode == 1)
            {
                for (int y = 0; y <= m_cursorY; ++y)
                {
                    const int rowEnd = (y == m_cursorY) ? (m_cursorX + 1) : m_width;
                    for (int x = 0; x < rowEnd; ++x)
                    {
                        m_builder->setGlyph(x, y, U' ', m_cursorStyle);
                    }
                }

                return true;
            }

            for (int y = m_cursorY; y < m_height; ++y)
            {
                const int rowStart = (y == m_cursorY) ? m_cursorX : 0;
                for (int x = rowStart; x < m_width; ++x)
                {
                    m_builder->setGlyph(x, y, U' ', m_cursorStyle);
                }
            }

            return true;
        }

        bool eraseLine(int mode)
        {
            if (!m_builderInitialized)
            {
                return false;
            }

            if (mode == 2)
            {
                for (int x = 0; x < m_width; ++x)
                {
                    m_builder->setGlyph(x, m_cursorY, U' ', m_cursorStyle);
                }

                return true;
            }

            if (mode == 1)
            {
                for (int x = 0; x <= m_cursorX && x < m_width; ++x)
                {
                    m_builder->setGlyph(x, m_cursorY, U' ', m_cursorStyle);
                }

                return true;
            }

            for (int x = m_cursorX; x < m_width; ++x)
            {
                m_builder->setGlyph(x, m_cursorY, U' ', m_cursorStyle);
            }

            return true;
        }

        bool applySgr(const std::vector<int>& params, std::size_t byteOffset)
        {
            if (params.empty())
            {
                m_cursorStyle = m_options.baseStyle;
                return true;
            }

            for (std::size_t i = 0; i < params.size(); ++i)
            {
                const int p = params[i];

                switch (p)
                {
                case 0:
                    m_cursorStyle = m_options.baseStyle;
                    break;

                case 1:
                    m_cursorStyle = resolveStyle().withBold(true);
                    break;

                case 2:
                    m_cursorStyle = resolveStyle().withDim(true);
                    break;

                case 4:
                    m_cursorStyle = resolveStyle().withUnderline(true);
                    break;

                case 5:
                    m_cursorStyle = resolveStyle().withSlowBlink(true);
                    break;

                case 6:
                    m_cursorStyle = resolveStyle().withFastBlink(true);
                    break;

                case 7:
                    m_cursorStyle = resolveStyle().withReverse(true);
                    break;

                case 8:
                    m_cursorStyle = resolveStyle().withInvisible(true);
                    break;

                case 9:
                    m_cursorStyle = resolveStyle().withStrike(true);
                    break;

                case 22:
                    m_cursorStyle = resolveStyle().withoutBold().withoutDim();
                    break;

                case 24:
                    m_cursorStyle = resolveStyle().withoutUnderline();
                    break;

                case 25:
                    m_cursorStyle = resolveStyle().withoutSlowBlink().withoutFastBlink();
                    break;

                case 27:
                    m_cursorStyle = resolveStyle().withoutReverse();
                    break;

                case 28:
                    m_cursorStyle = resolveStyle().withoutInvisible();
                    break;

                case 29:
                    m_cursorStyle = resolveStyle().withoutStrike();
                    break;

                case 39:
                    m_cursorStyle = resolveStyle().withoutForeground();
                    break;

                case 49:
                    m_cursorStyle = resolveStyle().withoutBackground();
                    break;

                default:
                    if (p >= 30 && p <= 37)
                    {
                        m_cursorStyle = resolveStyle().withForeground(
                            Color::FromBasic(ansiBasicToColor(p - 30)));
                    }
                    else if (p >= 40 && p <= 47)
                    {
                        m_cursorStyle = resolveStyle().withBackground(
                            Color::FromBasic(ansiBasicToColor(p - 40)));
                    }
                    else if (p >= 90 && p <= 97)
                    {
                        m_cursorStyle = resolveStyle().withForeground(
                            Color::FromBasic(ansiBrightBasicToColor(p - 90)));
                    }
                    else if (p >= 100 && p <= 107)
                    {
                        m_cursorStyle = resolveStyle().withBackground(
                            Color::FromBasic(ansiBrightBasicToColor(p - 100)));
                    }
                    else if (p == 38 || p == 48)
                    {
                        const bool foreground = (p == 38);
                        if (!applyExtendedColor(params, i, foreground))
                        {
                            addRuntimeWarning(
                                LoadWarningCode::UnsupportedSgrIgnored,
                                "Malformed extended SGR color sequence ignored.",
                                byteOffset,
                                currentPosition());

                            if (m_options.strictUnsupportedCommands)
                            {
                                return false;
                            }
                        }
                    }
                    else
                    {
                        addRuntimeWarning(
                            LoadWarningCode::UnsupportedSgrIgnored,
                            "Unsupported SGR attribute ignored.",
                            byteOffset,
                            currentPosition());

                        if (m_options.strictUnsupportedCommands)
                        {
                            return false;
                        }
                    }

                    break;
                }
            }

            return true;
        }

        bool applyExtendedColor(const std::vector<int>& params, std::size_t& ioIndex, bool foreground)
        {
            if (ioIndex + 1 >= params.size())
            {
                return false;
            }

            const int mode = params[++ioIndex];

            if (mode == 5)
            {
                if (ioIndex + 1 >= params.size())
                {
                    return false;
                }

                const int value = params[++ioIndex];
                if (value < 0 || value > 255)
                {
                    return false;
                }

                if (foreground)
                {
                    m_cursorStyle = resolveStyle().withForeground(
                        Color::FromIndexed(static_cast<std::uint8_t>(value)));
                }
                else
                {
                    m_cursorStyle = resolveStyle().withBackground(
                        Color::FromIndexed(static_cast<std::uint8_t>(value)));
                }

                return true;
            }

            if (mode == 2)
            {
                if (ioIndex + 3 >= params.size())
                {
                    return false;
                }

                const int r = params[++ioIndex];
                const int g = params[++ioIndex];
                const int b = params[++ioIndex];

                if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
                {
                    return false;
                }

                if (foreground)
                {
                    m_cursorStyle = resolveStyle().withForeground(
                        Color::FromRgb(
                            static_cast<std::uint8_t>(r),
                            static_cast<std::uint8_t>(g),
                            static_cast<std::uint8_t>(b)));
                }
                else
                {
                    m_cursorStyle = resolveStyle().withBackground(
                        Color::FromRgb(
                            static_cast<std::uint8_t>(r),
                            static_cast<std::uint8_t>(g),
                            static_cast<std::uint8_t>(b)));
                }

                return true;
            }

            return false;
        }

        Style resolveStyle() const
        {
            return m_cursorStyle.value_or(Style{});
        }

        bool writeGlyph(TextObjectBuilder& builder, char32_t glyph, std::size_t byteOffset)
        {
            m_builder = &builder;
            m_builderInitialized = true;

            glyph = UnicodeConversion::sanitizeCodePoint(glyph);

            const CellWidth measuredWidth = UnicodeWidth::measureCodePointWidth(glyph);
            if (measuredWidth == CellWidth::Zero)
            {
                return true;
            }

            const int glyphWidth = (measuredWidth == CellWidth::Two) ? 2 : 1;

            if (m_options.wrapAtColumnBoundary && m_cursorX + glyphWidth > m_width && m_cursorX > 0)
            {
                m_cursorX = 0;
                ++m_cursorY;
                clampCursor(byteOffset);
            }

            if (m_cursorX < 0 || m_cursorY < 0 || m_cursorX >= m_width || m_cursorY >= m_height)
            {
                return false;
            }

            if (measuredWidth == CellWidth::Two)
            {
                if (m_cursorX + 1 >= m_width)
                {
                    if (!m_options.wrapAtColumnBoundary)
                    {
                        return false;
                    }

                    m_cursorX = 0;
                    ++m_cursorY;
                    clampCursor(byteOffset);

                    if (m_cursorX + 1 >= m_width || m_cursorY >= m_height)
                    {
                        return false;
                    }
                }

                if (!builder.setWideGlyph(m_cursorX, m_cursorY, glyph, m_cursorStyle))
                {
                    return false;
                }
            }
            else
            {
                if (!builder.setGlyph(m_cursorX, m_cursorY, glyph, m_cursorStyle))
                {
                    return false;
                }
            }

            m_cursorX += glyphWidth;
            return true;
        }

        SourcePosition currentPosition() const
        {
            SourcePosition position;
            position.x = m_cursorX;
            position.y = m_cursorY;
            return position;
        }

        AnsiLoader::LoadResult fail(
            const std::string& message,
            std::size_t byteOffset,
            char32_t firstFailingCodePoint = U'\0')
        {
            AnsiLoader::LoadResult result;
            result.success = false;
            result.detectedFileType = AnsiLoader::FileType::Ans;
            result.sauce = m_sauce.metadata;
            result.resolvedWidth = m_width;
            result.resolvedHeight = m_height;
            result.hasParseFailure = true;
            result.failingByteOffset = byteOffset;
            result.firstFailingCodePoint = firstFailingCodePoint;
            result.firstFailingPosition = currentPosition();
            result.errorMessage = message;
            result.warnings = m_warnings;

            if (m_sauce.metadata.present && m_sauce.invalidCommentBlock)
            {
                addWarning(
                    result,
                    LoadWarningCode::InvalidSauceCommentBlockIgnored,
                    "SAUCE declared a comment block, but the COMNT marker was invalid and the block was ignored.",
                    m_sauce.contentSize,
                    {});
            }

            if (m_sauce.truncated)
            {
                addWarning(
                    result,
                    LoadWarningCode::TruncatedSauceIgnored,
                    "SAUCE comment metadata appeared truncated and was partially ignored.",
                    m_sauce.contentSize,
                    {});
            }

            return result;
        }

    private:
        const AnsiLoader::LoadOptions& m_options;
        const SauceSupport::SauceParseResult& m_sauce;

        int m_width = 80;
        int m_height = 1;

        int m_cursorX = 0;
        int m_cursorY = 0;

        int m_savedCursorX = 0;
        int m_savedCursorY = 0;

        std::optional<Style> m_cursorStyle;
        std::optional<Style> m_savedStyle;

        TextObjectBuilder* m_builder = nullptr;
        bool m_builderInitialized = false;

        std::vector<LoadWarning> m_warnings;
    };
}

namespace AnsiLoader
{
    FileType detectFileType(const std::string& filePath)
    {
        const std::string extension = getFileExtensionLower(filePath);

        if (extension == ".ans")
        {
            return FileType::Ans;
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
            result.errorMessage = "Failed to open ANSI file.";
            return result;
        }

        LoadResult result = loadFromBytes(bytes, options);
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
        const SauceSupport::SauceParseResult sauce = SauceSupport::parse(bytes);
        const std::string_view content = bytes.substr(0, sauce.contentSize);

        Parser parser(options, sauce);
        LoadResult result = parser.parse(content);

        if (result.detectedFileType == FileType::Unknown)
        {
            result.detectedFileType = FileType::Ans;
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
        if (result.success)
        {
            return {};
        }

        std::ostringstream message;
        message << "ANSI load failed";

        if (!result.errorMessage.empty())
        {
            message << ": " << result.errorMessage;
        }

        if (result.hasParseFailure)
        {
            message << " [byte=" << result.failingByteOffset << "]";

            if (result.firstFailingCodePoint != U'\0')
            {
                message << " [codepoint=U+"
                    << std::uppercase << std::hex
                    << static_cast<std::uint32_t>(result.firstFailingCodePoint)
                    << std::dec << "]";
            }

            if (result.firstFailingPosition.isValid())
            {
                message << " [position=("
                    << result.firstFailingPosition.x
                    << ", "
                    << result.firstFailingPosition.y
                    << ")]";
            }
        }

        if (!result.warnings.empty())
        {
            message << " [warnings=" << result.warnings.size() << "]";
        }

        return message.str();
    }

    std::string formatLoadSuccess(const LoadResult& result)
    {
        if (!result.success)
        {
            return {};
        }

        std::ostringstream message;
        message << "ANSI load succeeded";
        message << ". FileType=" << toString(result.detectedFileType) << ".";
        message << " Size=" << result.resolvedWidth << "x" << result.resolvedHeight << ".";

        if (result.sauce.present)
        {
            message << " SAUCE=true.";
            if (!result.sauce.title.empty())
            {
                message << " Title=\"" << result.sauce.title << "\".";
            }
        }
        else
        {
            message << " SAUCE=false.";
        }

        if (result.warnings.empty())
        {
            message << " No warnings.";
        }
        else
        {
            message << " Warnings=" << result.warnings.size() << ".";
        }

        return message.str();
    }

    const char* toString(FileType fileType)
    {
        switch (fileType)
        {
        case FileType::Auto:
            return "Auto";
        case FileType::Unknown:
            return "Unknown";
        case FileType::Ans:
            return "ANS";
        default:
            return "Unknown";
        }
    }

    const char* toString(LoadWarningCode warningCode)
    {
        switch (warningCode)
        {
        case LoadWarningCode::None:
            return "None";
        case LoadWarningCode::SauceMetadataPresent:
            return "SauceMetadataPresent";
        case LoadWarningCode::SauceCommentsImported:
            return "SauceCommentsImported";
        case LoadWarningCode::SauceWidthOverrideApplied:
            return "SauceWidthOverrideApplied";
        case LoadWarningCode::InvalidSauceCommentBlockIgnored:
            return "InvalidSauceCommentBlockIgnored";
        case LoadWarningCode::TruncatedSauceIgnored:
            return "TruncatedSauceIgnored";
        case LoadWarningCode::UnsupportedSequenceIgnored:
            return "UnsupportedSequenceIgnored";
        case LoadWarningCode::UnsupportedPrivateSequenceIgnored:
            return "UnsupportedPrivateSequenceIgnored";
        case LoadWarningCode::UnsupportedSgrIgnored:
            return "UnsupportedSgrIgnored";
        case LoadWarningCode::BareEscapeIgnored:
            return "BareEscapeIgnored";
        case LoadWarningCode::CursorClamped:
            return "CursorClamped";
        default:
            return "Unknown";
        }
    }
}