#pragma once

#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/ThemeColor.h"

namespace BannerThemes
{
    // =========================================================
    // Titles / headings / banner presentation
    // =========================================================

    inline const Style Title =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(51),
            Color::FromRgb(95, 255, 255)));

    inline const Style TitleShadow =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(240),
            Color::FromRgb(88, 88, 88)));

    inline const Style Subtitle =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)));

    inline const Style SectionHeader =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(226),
            Color::FromRgb(255, 255, 0)));

    inline const Style InfoBanner =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue),
            Color::FromIndexed(24),
            Color::FromRgb(0, 95, 135)));

    inline const Style SuccessBanner =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Green),
            Color::FromIndexed(28),
            Color::FromRgb(0, 135, 0)));

    inline const Style WarningBanner =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(226),
            Color::FromRgb(255, 255, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Red),
            Color::FromIndexed(88),
            Color::FromRgb(135, 0, 0)));

    inline const Style RetroBanner =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightGreen),
            Color::FromIndexed(46),
            Color::FromRgb(0, 255, 95)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)));
}