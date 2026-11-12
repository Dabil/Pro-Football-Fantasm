#include "Screens/HeaderView.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>
#include <string>

void HeaderView::draw(Composition::PageComposer &page, const Rect &header, const TextObject &banner,
                      std::string_view title, TitleFallback fallback)
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
    else if (fallback == TitleFallback::Wrapped)
    {
        const auto lines = wrapText(title, std::max(1, titleArea.size.width));
        const auto count = std::min(lines.size(), static_cast<std::size_t>(std::max(0, titleArea.size.height)));
        for (std::size_t row = 0; row < count; ++row)
            page.writeLine(titleArea, static_cast<int>(row), lines[row], PFF_Colors::ProFootballTitleColor);
    }
    else
    {
        const int titleWidth = TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(title));
        const int inset = std::max(0, (titleArea.size.width - titleWidth) / 2);
        page.writeLine(titleArea, 0, std::string(inset, ' ') + std::string(title), PFF_Colors::ProFootballTitleColor);
    }
}
