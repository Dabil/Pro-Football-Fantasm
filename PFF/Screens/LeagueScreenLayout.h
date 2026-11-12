// Shared FIGlet header, league footer, and two-pane geometry for league screens.
#pragma once

#include "Rendering/Composition/PageComposer.h"
#include "Screens/LeagueFooter.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"
#include <algorithm>
#include <string>
#include <string_view>

namespace PffLeagueScreen
{
struct Panes
{
    Rect screen;
    Rect body;
    Rect content;
    Rect controls;
    Rect footer;
};

inline Panes drawFrame(Composition::PageComposer &page, const TextObject &banner, std::string_view title)
{
    auto &buffer = page.getTarget();
    buffer.clear(PFF_Colors::MainBackgroundColor);
    page.createFullScreenRegion("Screen");
    const auto screen = page.resolveRegion("Screen");
    const auto [header, rest] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(rest, PffLeagueFooter::Height);
    const auto [controls, content] = page.splitRight(body, std::clamp(body.size.width / 3, 28, 34));

    page.createRegion("Header", header);
    page.createInsetRegion("HeaderInner", "Header", 2, 1, 2, 1);
    page.createInsetRegion("Title", "HeaderInner", 4, 2, 2, 1);
    const auto &outer = PFF_Colors::HeaderOuterColor;
    page.drawPanel("Header", outer, outer, ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("HeaderInner"), U' ', PFF_Colors::HeaderInnerColor);
    const auto titleArea = page.resolveRegion("Title");
    if (!banner.isEmpty() && banner.getWidth() <= titleArea.size.width &&
        banner.getHeight() <= titleArea.size.height)
    {
        page.writeObjectInRegion(banner, "Title", Composition::Align::center());
    }
    else
    {
        const int titleWidth = TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(title));
        const int inset = std::max(0, (titleArea.size.width - titleWidth) / 2);
        page.writeLine(titleArea, 0, std::string(inset, ' ') + std::string(title), PFF_Colors::ProFootballTitleColor);
    }
    return {screen, body, content, controls, footer};
}

inline void drawFooter(Composition::PageComposer &page, const Rect &footer,
                       const std::string &leagueName, const std::string &clock)
{
    const auto text = PffLeagueFooter::textBounds(footer);
    page.writeLine(text, 0, TextClip::clipUtf8Text(leagueName, text.size.width), PFF_Colors::TextHighlightColor);
    page.writeLine(text, 1, TextClip::clipUtf8Text(clock, text.size.width), PFF_Colors::TextHighlightColor);
}
} // namespace PffLeagueScreen
