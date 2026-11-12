// Draws the away and home team statistics for the selected game state.
#pragma once

#include "Rendering/Composition/PageComposer.h"

struct WorldTestRuntimeState;

namespace StatisticsView
{
void draw(Composition::PageComposer &page, const Rect &area, const WorldTestRuntimeState &state);
}
