#include "Screens/ContentControlView.h"
#include <algorithm>

ContentControlView::Panes ContentControlView::layout(Composition::PageComposer &page, const Rect &body)
{
    const auto [controls, content] = page.splitRight(body, std::clamp(body.size.width / 3, 28, 34));
    return {content, controls};
}
