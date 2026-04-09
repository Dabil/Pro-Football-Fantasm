#pragma once

#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/ThemeColor.h"

namespace UIThemes
{
    inline const Style NormalText = Style();

    inline const Style DisabledText =
        style::Dim
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(243),
            Color::FromRgb(118, 118, 118)));

    inline const Style Label =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)));

    inline const Style Value =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(51),
            Color::FromRgb(95, 255, 255)));

    inline const Style DialogTitle =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue),
            Color::FromIndexed(18),
            Color::FromRgb(0, 0, 135)));

    inline const Style PanelTitle =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(51),
            Color::FromRgb(95, 255, 255)));

    inline const Style StatusBar =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(250),
            Color::FromRgb(210, 210, 210)));

    inline const Style Selection =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightCyan),
            Color::FromIndexed(123),
            Color::FromRgb(135, 255, 255)));

    inline const Style Focused =
        style::Bold
        + style::Underline;

    inline const Style ButtonNormal =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue),
            Color::FromIndexed(19),
            Color::FromRgb(0, 0, 175)));

    inline const Style ButtonHot =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(227),
            Color::FromRgb(255, 255, 95)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Blue),
            Color::FromIndexed(24),
            Color::FromRgb(0, 95, 135)));

    inline const Style ButtonDisabled =
        style::Dim
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(243),
            Color::FromRgb(118, 118, 118)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(233),
            Color::FromRgb(18, 18, 18)));

    inline const Style Border =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlue),
            Color::FromIndexed(39),
            Color::FromRgb(0, 175, 255)));

    inline const Style Header =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightWhite),
            Color::FromIndexed(255),
            Color::FromRgb(255, 255, 255)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightBlack),
            Color::FromIndexed(238),
            Color::FromRgb(68, 68, 68)));

    inline const Style Footer =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::White),
            Color::FromIndexed(250),
            Color::FromRgb(210, 210, 210)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(233),
            Color::FromRgb(18, 18, 18)));

    inline const Style Tooltip =
        style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::Black),
            Color::FromIndexed(16),
            Color::FromRgb(0, 0, 0)))
        + style::Bg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(228),
            Color::FromRgb(255, 255, 135)));

    inline const Style ErrorText =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightRed),
            Color::FromIndexed(196),
            Color::FromRgb(255, 0, 0)));

    inline const Style WarningText =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightYellow),
            Color::FromIndexed(226),
            Color::FromRgb(255, 255, 0)));

    inline const Style SuccessText =
        style::Bold
        + style::Fg(ThemeColor::Basic256Rgb(
            Color::FromBasic(Color::Basic::BrightGreen),
            Color::FromIndexed(46),
            Color::FromRgb(0, 255, 95)));
}