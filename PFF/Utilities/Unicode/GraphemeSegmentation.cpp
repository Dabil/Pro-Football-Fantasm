#include "Utilities/Unicode/GraphemeSegmentation.h"

#include "Utilities/Unicode/UnicodeConversion.h"
#include "Utilities/Unicode/UnicodeWidth.h"

/*
    Purpose:

    Provides future-safe grapheme-like segmentation for writing and wrapping

    Checklist:
        - Add segmentation function
        - Keep this as a helper layer, not part of ScreenCell
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
}

namespace GraphemeSegmentation
{
    std::vector<TextCluster> segment(std::u32string_view text)
    {
        std::vector<TextCluster> clusters;
        clusters.reserve(text.size());

        for (char32_t codePoint : text)
        {
            codePoint = UnicodeConversion::sanitizeCodePoint(codePoint);

            if (UnicodeWidth::isCombiningMark(codePoint))
            {
                if (!clusters.empty())
                {
                    clusters.back().codePoints.push_back(codePoint);
                }
                else
                {
                    TextCluster cluster;
                    cluster.codePoints.push_back(codePoint);
                    cluster.displayWidth = 0;
                    clusters.push_back(cluster);
                }

                continue;
            }

            TextCluster cluster;
            cluster.codePoints.push_back(codePoint);
            cluster.displayWidth =
                toDisplayWidth(UnicodeWidth::measureCodePointWidth(codePoint));

            clusters.push_back(cluster);
        }

        return clusters;
    }
}