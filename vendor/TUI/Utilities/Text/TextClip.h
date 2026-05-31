#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "Utilities/Unicode/GraphemeSegmentation.h"
#include "Utilities/Unicode/UnicodeConversion.h"

namespace TextClip
{
    inline int measureDisplayWidth(std::u32string_view text)
    {
        int width = 0;

        for (const TextCluster& cluster : GraphemeSegmentation::segment(text))
        {
            width += std::max(0, static_cast<int>(cluster.displayWidth));
        }

        return width;
    }

    inline std::u32string clipText(std::u32string_view text, int maxWidth)
    {
        if (maxWidth <= 0)
        {
            return {};
        }

        if (measureDisplayWidth(text) <= maxWidth)
        {
            return std::u32string(text);
        }

        constexpr std::u32string_view Ellipsis = U"...";
        constexpr int EllipsisWidth = 3;

        const bool useEllipsis = maxWidth > EllipsisWidth;
        const int contentWidthLimit = useEllipsis
            ? maxWidth - EllipsisWidth
            : maxWidth;

        std::u32string clipped;
        int usedWidth = 0;

        for (const TextCluster& cluster : GraphemeSegmentation::segment(text))
        {
            const int clusterWidth =
                std::max(0, static_cast<int>(cluster.displayWidth));

            if (clusterWidth == 0)
            {
                if (!clipped.empty())
                {
                    clipped += cluster.codePoints;
                }

                continue;
            }

            if (usedWidth + clusterWidth > contentWidthLimit)
            {
                break;
            }

            clipped += cluster.codePoints;
            usedWidth += clusterWidth;
        }

        if (useEllipsis)
        {
            clipped += Ellipsis;
        }

        return clipped;
    }

    inline std::string clipUtf8Text(std::string_view utf8Text, int maxWidth)
    {
        const std::u32string text = UnicodeConversion::utf8ToU32(utf8Text);
        return UnicodeConversion::u32ToUtf8(clipText(text, maxWidth));
    }

    inline std::string clipUtf8Text(const std::string& utf8Text, int maxWidth)
    {
        return clipUtf8Text(std::string_view(utf8Text), maxWidth);
    }

    inline std::string clipUtf8Text(const char* utf8Text, int maxWidth)
    {
        if (utf8Text == nullptr)
        {
            return {};
        }

        return clipUtf8Text(std::string_view(utf8Text), maxWidth);
    }
}