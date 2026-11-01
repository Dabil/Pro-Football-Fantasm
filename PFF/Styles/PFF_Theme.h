#pragma once

#include "Rendering/Styles/Themes.h"

namespace PFF_Colors
{
///////////////////////////////////////////////////////////////
// C O L O R   A U G M E N T A T I O N S
//    
    inline const Style TextHighlightColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Yellow),
            Color::FromIndexed(223),
            Color::FromRgb(255, 235, 178)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Green),
            Color::FromIndexed(58),
            Color::FromRgb(51, 89, 2)));

    ///////////////////////////////////////////////////////////////
    // M A I N   U I   C O L O R S
    //    

    inline const Style MainBackgroundColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightGreen),
            Color::FromIndexed(65),
            Color::FromRgb(65, 96, 65)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Green),
            Color::FromIndexed(58),
            Color::FromRgb(51, 89, 2)));

    inline const Style MainBorderColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Green),
            Color::FromIndexed(58),
            Color::FromRgb(51, 89, 2)));

    inline const Style HeaderOuterColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)));

    inline const Style HeaderInnerColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(244),
            Color::FromRgb(139, 139, 139)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(102),
            Color::FromRgb(65, 65, 65)));

    inline const Style ProFootballTitleColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(175, 175, 175)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)));

    inline const Style FantasmTitleColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Red),
            Color::FromIndexed(124),
            Color::FromRgb(155, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(102),
            Color::FromRgb(0, 0, 0)));

    inline const Style ButtonNormalColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(252),
            Color::FromRgb(208, 207, 207)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Red),
            Color::FromIndexed(124),
            Color::FromRgb(155, 0, 0)));

    inline const Style ButtonHoverColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightRed),
            Color::FromIndexed(160),
            Color::FromRgb(215, 0, 0)));

    inline const Style ButtonPressedColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(241),
            Color::FromRgb(102, 101, 100)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(52),
            Color::FromRgb(69, 1, 0)));

    inline const Style ButtonFrameColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)));
}