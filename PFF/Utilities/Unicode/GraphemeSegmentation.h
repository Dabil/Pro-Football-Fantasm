#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Rendering/Text/TextCluster.h"

/*
    Purpose:

    Provides future-safe grapheme segmentation for writing and wrapping
*/

namespace GraphemeSegmentation
{
    std::vector<TextCluster> segment(std::u32string_view text);
}