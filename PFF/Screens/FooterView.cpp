#include "Screens/FooterView.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"

void FooterView::draw(Composition::PageComposer &page, const Rect &footer,
                      const std::string &leagueName, const std::string &clock)
{
    const auto text = PffLeagueFooter::textBounds(footer);
    page.writeLine(text, 0, TextClip::clipUtf8Text(leagueName, text.size.width), PFF_Colors::TextHighlightColor);
    page.writeLine(text, 1, TextClip::clipUtf8Text(clock, text.size.width), PFF_Colors::TextHighlightColor);
}
