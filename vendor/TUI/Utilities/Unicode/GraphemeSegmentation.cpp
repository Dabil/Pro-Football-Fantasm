#include "Utilities/Unicode/GraphemeSegmentation.h"

#include <algorithm>

#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"

/*
    Purpose:

    Provides pragmatic grapheme-cluster segmentation for writing and wrapping.

    This is intentionally not a generated full Unicode text-segmentation engine.
    It covers the cluster forms the TUI renderer/layout systems must not split:
        - combining-mark sequences
        - variation-selector sequences
        - emoji modifier sequences
        - zero-width-joiner emoji sequences
        - regional-indicator flag pairs
*/

namespace
{
    int toDisplayWidth(CellWidth width)
    {
        switch (width)
        {
        case CellWidth::Zero:
            return 0;

        case CellWidth::Two:
            return 2;

        case CellWidth::One:
        default:
            return 1;
        }
    }

    bool isVariationSelector(char32_t codePoint)
    {
        return (codePoint >= 0xFE00 && codePoint <= 0xFE0F) ||
            (codePoint >= 0xE0100 && codePoint <= 0xE01EF);
    }

    bool isEmojiModifier(char32_t codePoint)
    {
        return codePoint >= 0x1F3FB && codePoint <= 0x1F3FF;
    }

    bool isZeroWidthJoiner(char32_t codePoint)
    {
        return codePoint == 0x200D;
    }

    bool isRegionalIndicator(char32_t codePoint)
    {
        return codePoint >= 0x1F1E6 && codePoint <= 0x1F1FF;
    }

    bool isClusterExtender(char32_t codePoint)
    {
        return UnicodeWidth::isCombiningMark(codePoint) ||
            isVariationSelector(codePoint) ||
            isEmojiModifier(codePoint);
    }

    int measureClusterWidth(std::u32string_view cluster)
    {
        int width = 0;

        for (char32_t codePoint : cluster)
        {
            codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);
            width = std::max(width, toDisplayWidth(UnicodeWidth::measureCodePointWidth(codePoint)));
        }

        return width;
    }

    TextCluster makeCluster(std::u32string codePoints)
    {
        TextCluster cluster;
        cluster.codePoints = std::move(codePoints);
        cluster.displayWidth = measureClusterWidth(cluster.codePoints);
        return cluster;
    }
}

namespace GraphemeSegmentation
{
    std::vector<TextCluster> segment(std::u32string_view text)
    {
        std::vector<TextCluster> clusters;
        clusters.reserve(text.size());

        std::u32string current;
        bool previousWasZeroWidthJoiner = false;
        int regionalIndicatorRunLength = 0;

        const auto flushCurrent = [&]()
            {
                if (!current.empty())
                {
                    clusters.push_back(makeCluster(std::move(current)));
                    current.clear();
                }

                previousWasZeroWidthJoiner = false;
                regionalIndicatorRunLength = 0;
            };

        for (char32_t rawCodePoint : text)
        {
            const char32_t codePoint = UnicodeConversion::sanitizeCodePoint(rawCodePoint);

            if (current.empty())
            {
                current.push_back(codePoint);
                previousWasZeroWidthJoiner = isZeroWidthJoiner(codePoint);
                regionalIndicatorRunLength = isRegionalIndicator(codePoint) ? 1 : 0;
                continue;
            }

            if (previousWasZeroWidthJoiner)
            {
                current.push_back(codePoint);
                previousWasZeroWidthJoiner = isZeroWidthJoiner(codePoint);
                regionalIndicatorRunLength = 0;
                continue;
            }

            if (isZeroWidthJoiner(codePoint))
            {
                current.push_back(codePoint);
                previousWasZeroWidthJoiner = true;
                regionalIndicatorRunLength = 0;
                continue;
            }

            if (isClusterExtender(codePoint))
            {
                current.push_back(codePoint);
                previousWasZeroWidthJoiner = false;
                continue;
            }

            if (isRegionalIndicator(codePoint))
            {
                if (regionalIndicatorRunLength == 1)
                {
                    current.push_back(codePoint);
                    regionalIndicatorRunLength = 2;
                    previousWasZeroWidthJoiner = false;
                    continue;
                }

                flushCurrent();
                current.push_back(codePoint);
                regionalIndicatorRunLength = 1;
                previousWasZeroWidthJoiner = false;
                continue;
            }

            flushCurrent();
            current.push_back(codePoint);
            previousWasZeroWidthJoiner = false;
            regionalIndicatorRunLength = 0;
        }

        flushCurrent();
        return clusters;
    }
}

