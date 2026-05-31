#pragma once

namespace Composition
{
    /*
        OverwriteRule answers:

            "When a write is otherwise allowed, under what destination condition
             may it replace existing destination state?"

        This enum is intentionally explicit so glyph and style replacement behavior
        can be described without hidden booleans.

        Semantics:
        - Never:
            Do not overwrite destination state.
        - IfTargetEmpty:
            Overwrite only when the destination is logically empty.
        - IfTargetNonEmpty:
            Overwrite only when the destination is logically non-empty.
        - Always:
            Always overwrite when the write is otherwise eligible.
    */
    enum class OverwriteRule
    {
        Never,
        IfTargetEmpty,
        IfTargetNonEmpty,
        Always
    };
}