#include "Screens/FrameView.h"

FrameView::Regions FrameView::begin(Composition::PageComposer &page, int headerHeight, int footerHeight,
                                    const Style &background)
{
    page.getTarget().clear(background);
    page.createFullScreenRegion("Screen");
    const auto screen = page.resolveRegion("Screen");
    const auto [header, rest] = page.splitTop(screen, headerHeight);
    const auto [footer, body] = page.splitBottom(rest, footerHeight);
    return {screen, header, body, footer};
}
