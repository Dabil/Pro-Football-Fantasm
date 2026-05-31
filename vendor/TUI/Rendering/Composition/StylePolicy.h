#pragma once

namespace Composition
{
    /*
        StylePolicy answers:

            "If a participating source cell is considered, should source style
             metadata be applied to the destination?"

        Semantics:
        - None:
            Do not write style from the source.
        - Apply:
            Apply source style to the destination subject to overwrite rules.
    */
    enum class StylePolicy
    {
        None,
        Apply
    };
}