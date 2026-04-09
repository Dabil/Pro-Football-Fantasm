#pragma once

#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/StyleBuilder.h"

namespace AppThemes
{
    // =========================================================
    // Base application surfaces
    // =========================================================

    inline const Style Background =
        style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Black));

    inline const Style Window =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Blue));

    inline const Style Panel =
        style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Black));

    inline const Style AccentSurface =
        style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Blue));

    inline const Style RetroTerminal =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightGreen))
        + style::Bg(Color::FromBasic(Color::Basic::Black));

    // =========================================================
    // Application feedback / status styles
    // =========================================================

    inline const Style Info =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightCyan));

    inline const Style Success =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightGreen));

    inline const Style Warning =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightYellow))
        + style::Bg(Color::FromBasic(Color::Basic::Red));

    inline const Style Danger =
        style::Bold
        + style::FastBlink
        + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Red));

    inline const Style Highlight =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::Black))
        + style::Bg(Color::FromBasic(Color::Basic::BrightYellow));

    inline const Style Dimmed =
        style::Dim
        + style::Fg(Color::FromBasic(Color::Basic::White));
}