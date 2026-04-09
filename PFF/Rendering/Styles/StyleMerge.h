#pragma once

#include "Rendering/Styles/Style.h"

enum class StyleMergeMode
{
    Replace,
    PreserveDestination,
    MergePreserveDestination
};

class StyleMerge
{
public:
    static Style merge(
        const Style& destination,
        const Style& source,
        StyleMergeMode mode);

private:
    static Style mergePreserveDestination(
        const Style& destination,
        const Style& source);
};