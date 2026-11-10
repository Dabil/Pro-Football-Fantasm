#pragma once

#include "Rendering/Styles/Themes.h"
#include "Rendering/Styles/UIThemes.h"

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

    inline const Style NormalTextHeadingColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(225, 225, 225)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)));

    inline const Style NormalTextColor =
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

    // Formation boards and controls share their side colors.
    inline const Style FormationOffenseColor =
        style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue), Color::FromIndexed(18), Color::FromRgb(18, 44, 119)));

    inline const Style FormationDefenseColor =
        style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Red), Color::FromIndexed(124), Color::FromRgb(155, 0, 0)));

    inline const Style FormationPreferredColor =
        style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Green), Color::FromIndexed(28), Color::FromRgb(0, 112, 43)));

    inline const Style FormationAllowedColor =
        style::Fg(Color::FromBasic(Color::Basic::Black))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Yellow), Color::FromIndexed(220), Color::FromRgb(255, 212, 0)));

    inline const Style FormationInactiveOffenseColor = FormationOffenseColor.withDim(true);
    inline const Style FormationInactiveDefenseColor = FormationDefenseColor.withDim(true);

    // ScrollBox focus fills blank cells as well as text, so keep the fill unlined.
    inline const Style ScrollBoxFocusedStyle = UIThemes::WidgetTextFocused.withUnderline(false);

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

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Forms
    ///

    inline const Style FormPanelColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)));

    inline const Style FormBorderColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)));

    inline const Style FormPanelHeadingColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)));

    inline const Style InputFieldNormalColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Yellow),
            Color::FromIndexed(223),
            Color::FromRgb(254, 235, 180)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(241),
            Color::FromRgb(101, 101, 101)));

    inline const Style InputFieldHighlightColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Yellow),
            Color::FromIndexed(223),
            Color::FromRgb(254, 235, 180)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightRed),
            Color::FromIndexed(160),
            Color::FromRgb(215, 0, 0)));

    inline const Style InputFieldFocusedColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightRed),
            Color::FromIndexed(160),
            Color::FromRgb(215, 0, 0)));

    inline const Style InputFieldNormalHintColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)))
    + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(241),
            Color::FromRgb(101, 101, 101)));

    inline const Style InputFieldFocusedHintColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightRed),
            Color::FromIndexed(160),
            Color::FromRgb(215, 0, 0)));

    inline const Style PanelSliderColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(244),
            Color::FromRgb(69, 69, 69)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(0, 0, 0)));

    inline const Style PanelTrackColor =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(102),
            Color::FromRgb(139, 139, 139)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(0),
            Color::FromRgb(69, 69, 69)));
}