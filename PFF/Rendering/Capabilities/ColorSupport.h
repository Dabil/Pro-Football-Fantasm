#pragma once

/*
    Purpose:

    ColorSupport is a renderer-reported color capability tier.

    It answers only one question:
        what color family can the active render path present?

    It does not answer:
        - how authored ThemeColor should be resolved
        - how richer authored color should be downgraded
        - what policy should be chosen for a particular backend

    Those decisions belong later in ColorResolver / StylePolicy.

    The renderer/backend should only report capability honestly and
    conservatively for the active output path.
*/

enum class ColorSupport
{
    None = 0,
    Basic16,
    Indexed256,
    Rgb24
};