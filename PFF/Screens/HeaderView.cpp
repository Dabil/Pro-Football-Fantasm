#include "Screens/HeaderView.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"
#include <string>

void HeaderView::draw(Composition::PageComposer &page, const Rect &header, const TextObject &banner,
                      std::string_view title)
{
    page.createRegion("Header", header);
    page.createInsetRegion("HeaderInner", "Header", 2, 1, 2, 1);
    page.createInsetRegion("Title", "HeaderInner", 4, 2, 2, 1);
    const auto &outer = PFF_Colors::HeaderOuterColor;
    page.drawPanel("Header", outer, outer, ObjectFactory::doubleLineBorder());
    page.getTarget().fillRect(page.resolveRegion("HeaderInner"), U' ', PFF_Colors::HeaderInnerColor);
    const auto titleArea = page.resolveRegion("Title");
    if (!banner.isEmpty() && banner.getWidth() <= titleArea.size.width &&
        banner.getHeight() <= titleArea.size.height)
    {
        page.writeObjectInRegion(banner, "Title", Composition::Align::center());
    }
    else
    {
        const auto clippedTitle = TextClip::clipUtf8Text(title, titleArea.size.width);
        const int titleWidth = TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(clippedTitle));
        const int inset = (titleArea.size.width - titleWidth) / 2;
        page.writeLine(titleArea, 0, std::string(inset, ' ') + clippedTitle, PFF_Colors::ProFootballTitleColor);
    }
}
