// Composes the standard FIGlet header, two-pane body, and league footer.
#pragma once

#include "Screens/ContentControlView.h"
#include "Screens/FooterView.h"
#include "Screens/FrameView.h"
#include "Screens/HeaderView.h"
#include "Styles/PFF_Theme.h"

namespace ContentControlFrameView
{
struct Panes
{
    Rect screen;
    Rect body;
    Rect content;
    Rect controls;
    Rect footer;
};

using TitleFallback = HeaderView::TitleFallback;

inline Panes drawFrame(Composition::PageComposer &page, const TextObject &banner, std::string_view title,
                       TitleFallback fallback = TitleFallback::Centered,
                       ContentControlView::ControlPaneWidth controlWidth = {},
                       HeaderView::BannerPlacement placement = HeaderView::BannerPlacement::TitleArea)
{
    const auto frame = FrameView::begin(page, HeaderView::Height, FooterView::Height,
                                        PFF_Colors::MainBackgroundColor);
    HeaderView::draw(page, frame.header, banner, title, fallback, placement);
    const auto [content, controls] = ContentControlView::layout(page, frame.body, controlWidth);
    return {frame.screen, frame.body, content, controls, frame.footer};
}

inline void drawFooter(Composition::PageComposer &page, const Rect &footer,
                       const std::string &leagueName, const std::string &clock)
{
    FooterView::draw(page, footer, leagueName, clock);
}
} // namespace ContentControlFrameView
