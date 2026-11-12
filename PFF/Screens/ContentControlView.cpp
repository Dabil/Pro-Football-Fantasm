#include "Screens/ContentControlView.h"
#include <algorithm>

ContentControlView::Panes ContentControlView::layout(Composition::PageComposer &page, const Rect &body,
                                                     ControlPaneWidth width)
{
    const auto [controls, content] = page.splitRight(body,
        std::clamp(body.size.width / 3, width.minimum, width.maximum));
    return {content, controls};
}
