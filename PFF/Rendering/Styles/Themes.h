#pragma once

#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/ThemeColor.h"

/*
    These themes preserve the ANSI / DOS / BBS feel of the project first.

    Richer ThemeColor definitions are used selectively so:
        - Basic16 still looks correct and intentional
        - Indexed256 and Rgb24 can gain a little extra polish
        - the visual identity stays retro rather than drifting modern
*/

/*
    Can also create styles like this:

    inline Style PanelTitle()
    {
        return style::Bold
            + style::Fg(Color::FromBasic(Color::Basic::BrightCyan));
    }

    But then you will have to use them like this:

    buffer.writeString(4, 0, "[ Some Text Here ]", Themes::PanelTitle());

    Personally I prefer the current ANSI retro feel.
*/


namespace Themes
{
    // =========================================================
    // Base surfaces / page backgrounds
    // =========================================================

    inline const Style Background =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(250),
            Color::FromRgb(230, 230, 230)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(12, 12, 12)));

    inline const Style Window =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue),
            Color::FromIndexed(18),
            Color::FromRgb(0, 0, 135)));

    inline const Style Panel =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(233),
            Color::FromRgb(18, 18, 18)));

    inline const Style AccentSurface =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue),
            Color::FromIndexed(24),
            Color::FromRgb(0, 95, 135)));

    inline const Style RetroTerminal =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightGreen),
            Color::FromIndexed(46),
            Color::FromRgb(0, 255, 95)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)));

    // =========================================================
    // Neutral UI text styles
    // =========================================================
    // When using Foreground only styles, make sure you are
    // placing the glyphs on a cell where the background is
    // already owned. (ie a cell where the background has already
    // been directly specified)

    inline const Style PanelTitle =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)));

    inline const Style Text =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(252),
            Color::FromRgb(238, 238, 238)));

    inline const Style MutedText =
        style::Dim
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(245),
            Color::FromRgb(148, 148, 148)));

    inline const Style Frame =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(246),
            Color::FromRgb(168, 168, 168)));

    inline const Style Focused =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(227),
            Color::FromRgb(255, 255, 95)));

    inline const Style Selected =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(153),
            Color::FromRgb(175, 215, 255)));

    inline const Style Disabled =
        style::Dim
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(243),
            Color::FromRgb(118, 118, 118)));

    // =========================================================
    // Titles / headings / labels
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

    inline const Style Emphasis =
        style::Bold
        + style::Underline
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(87),
            Color::FromRgb(95, 255, 255)));

    // =========================================================
    // Informational / feedback / status styles
    // =========================================================

    inline const Style Info =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(51),
            Color::FromRgb(95, 255, 255)));

    inline const Style Success =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightGreen),
            Color::FromIndexed(46),
            Color::FromRgb(0, 255, 95)));

    inline const Style Warning =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(226),
            Color::FromRgb(255, 255, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Red),
            Color::FromIndexed(52),
            Color::FromRgb(95, 0, 0)));

    inline const Style Danger =
        style::Bold
        + style::FastBlink
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Red),
            Color::FromIndexed(160),
            Color::FromRgb(215, 0, 0)));

    inline const Style Hightlight =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(228),
            Color::FromRgb(255, 255, 135)));

    inline const Style Dimmed =
        style::Dim
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(245),
            Color::FromRgb(148, 148, 148)));

    inline const Style SlowBlink =
        style::SlowBlink
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(250),
            Color::FromRgb(210, 210, 210)));

    inline const Style FastBlink =
        style::FastBlink
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)));

    // =========================================================
    // Generic framework demo / object styles
    // =========================================================

    inline const Style Accent =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightMagenta),
            Color::FromIndexed(213),
            Color::FromRgb(255, 135, 255)));

    inline const Style DemoObject =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightGreen),
            Color::FromIndexed(46),
            Color::FromRgb(0, 255, 95)));

    inline const Style DemoObjectAlt =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(51),
            Color::FromRgb(95, 255, 255)));
}