#pragma once

#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/ThemeColor.h"

namespace style
{
    struct BoldAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withBold(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct DimAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withDim(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct UnderlineAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withUnderline(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct SlowBlinkAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withSlowBlink(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct FastBlinkAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withFastBlink(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct ReverseAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withReverse(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct InvisibleAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withInvisible(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    struct StrikeAtom
    {
        Style operator()(bool value = true) const
        {
            return Style().withStrike(value);
        }

        operator Style() const
        {
            return operator()();
        }
    };

    inline constexpr BoldAtom Bold{};
    inline constexpr DimAtom Dim{};
    inline constexpr UnderlineAtom Underline{};
    inline constexpr SlowBlinkAtom SlowBlink{};
    inline constexpr FastBlinkAtom FastBlink{};
    inline constexpr ReverseAtom Reverse{};
    inline constexpr InvisibleAtom Invisible{};
    inline constexpr StrikeAtom Strike{};

    Style Fg(const Color& color);
    Style Fg(const ThemeColor& color);

    Style Bg(const Color& color);
    Style Bg(const ThemeColor& color);
}

// NOTE:
// operator+ must live in the same namespace as Style (global namespace),
// not inside namespace style, or expressions like:
//     style::Bold + style::Fg(...)
// will fail to resolve.
Style operator+(const Style& left, const Style& right);