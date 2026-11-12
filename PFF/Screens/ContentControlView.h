// Divides a body into a left content pane and a right control pane.
#pragma once

#include "Rendering/Composition/PageComposer.h"

namespace ContentControlView
{
struct Panes
{
    Rect content;
    Rect controls;
};

struct ControlPaneWidth
{
    int minimum = 28;
    int maximum = 34;
};

Panes layout(Composition::PageComposer &page, const Rect &body, ControlPaneWidth width = {});
}
