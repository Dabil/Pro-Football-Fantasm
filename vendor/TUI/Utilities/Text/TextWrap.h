#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace TextWrapDetail
{
    inline bool isWrapWhitespaceText(std::u32string_view text)
    {
        if (text.empty())
        {
            return false;
        }

        for (char32_t ch : text)
        {
            if (ch != U' ' && ch != U'\t' && ch != U'\v' && ch != U'\f')
            {
                return false;
            }
        }

        return true;
    }

    inline std::vector<std::u32string> splitLines(std::u32string_view text)
    {
        std::vector<std::u32string> lines;
        std::u32string current;

        for (std::size_t i = 0; i < text.length(); ++i)
        {
            const char32_t ch = text[i];

            if (ch == U'\r')
            {
                lines.push_back(current);
                current.clear();

                if ((i + 1) < text.length() && text[i + 1] == U'\n')
                {
                    ++i;
                }

                continue;
            }

            if (ch == U'\n')
            {
                lines.push_back(current);
                current.clear();
                continue;
            }

            current.push_back(ch);
        }

        lines.push_back(current);
        return lines;
    }

    inline std::u32string trimTrailingWrapWhitespace(std::u32string_view text)
    {
        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(text);

        std::size_t end = clusters.size();
        while (end > 0 && isWrapWhitespaceText(clusters[end - 1].codePoints))
        {
            --end;
        }

        std::u32string result;
        for (std::size_t i = 0; i < end; ++i)
        {
            result += clusters[i].codePoints;
        }

        return result;
    }

    inline std::u32string buildClusterRange(
        const std::vector<TextCluster>& clusters,
        std::size_t startIndex,
        std::size_t endIndexExclusive,
        bool trimTrailingWhitespace)
    {
        std::u32string result;

        for (std::size_t i = startIndex; i < endIndexExclusive; ++i)
        {
            result += clusters[i].codePoints;
        }

        if (trimTrailingWhitespace)
        {
            result = trimTrailingWrapWhitespace(result);
        }

        return result;
    }

    inline std::vector<std::u32string> wrapSingleLineToU32Lines(
        std::u32string_view text,
        int maxWidth)
    {
        std::vector<std::u32string> lines;

        if (maxWidth <= 0)
        {
            return lines;
        }

        const std::vector<TextCluster> clusters = GraphemeSegmentation::segment(text);
        if (clusters.empty())
        {
            lines.push_back(U"");
            return lines;
        }

        std::size_t lineStart = 0;

        while (lineStart < clusters.size())
        {
            while (lineStart < clusters.size() &&
                isWrapWhitespaceText(clusters[lineStart].codePoints))
            {
                ++lineStart;
            }

            if (lineStart >= clusters.size())
            {
                lines.push_back(U"");
                break;
            }

            std::size_t index = lineStart;
            std::size_t lastBreak = static_cast<std::size_t>(-1);
            int currentWidth = 0;

            while (index < clusters.size())
            {
                const int clusterWidth =
                    std::max(0, static_cast<int>(clusters[index].displayWidth));

                const bool breakableWhitespace =
                    isWrapWhitespaceText(clusters[index].codePoints);

                if (currentWidth > 0 && (currentWidth + clusterWidth) > maxWidth)
                {
                    break;
                }

                currentWidth += clusterWidth;

                if (breakableWhitespace)
                {
                    lastBreak = index;
                }

                ++index;
            }

            if (index >= clusters.size())
            {
                lines.push_back(
                    buildClusterRange(clusters, lineStart, clusters.size(), true));
                break;
            }

            if (currentWidth == 0)
            {
                lines.push_back(
                    buildClusterRange(clusters, lineStart, lineStart + 1, false));
                lineStart = lineStart + 1;
                continue;
            }

            if (lastBreak != static_cast<std::size_t>(-1) && lastBreak >= lineStart)
            {
                lines.push_back(
                    buildClusterRange(clusters, lineStart, lastBreak, true));
                lineStart = lastBreak + 1;
                continue;
            }

            lines.push_back(
                buildClusterRange(clusters, lineStart, index, true));
            lineStart = index;
        }

        if (lines.empty())
        {
            lines.push_back(U"");
        }

        return lines;
    }
}

inline std::vector<std::u32string> wrapText(std::u32string_view text, int maxWidth)
{
    std::vector<std::u32string> wrappedLines;

    if (maxWidth <= 0)
    {
        return wrappedLines;
    }

    const std::vector<std::u32string> sourceLines =
        TextWrapDetail::splitLines(text);

    for (const std::u32string& sourceLine : sourceLines)
    {
        std::vector<std::u32string> paragraphLines =
            TextWrapDetail::wrapSingleLineToU32Lines(sourceLine, maxWidth);

        if (paragraphLines.empty())
        {
            wrappedLines.push_back(U"");
            continue;
        }

        wrappedLines.insert(
            wrappedLines.end(),
            paragraphLines.begin(),
            paragraphLines.end());
    }

    return wrappedLines;
}

inline std::vector<std::string> wrapText(std::string_view utf8Text, int maxWidth)
{
    const std::vector<std::u32string> u32Lines =
        wrapText(UnicodeConversion::utf8ToU32(utf8Text), maxWidth);

    std::vector<std::string> utf8Lines;
    utf8Lines.reserve(u32Lines.size());

    for (const std::u32string& line : u32Lines)
    {
        utf8Lines.push_back(UnicodeConversion::u32ToUtf8(line));
    }

    return utf8Lines;
}

inline std::vector<std::string> wrapText(const std::string& utf8Text, int maxWidth)
{
    return wrapText(std::string_view(utf8Text), maxWidth);
}

inline std::vector<std::string> wrapText(const char* utf8Text, int maxWidth)
{
    if (utf8Text == nullptr)
    {
        return {};
    }

    return wrapText(std::string_view(utf8Text), maxWidth);
}