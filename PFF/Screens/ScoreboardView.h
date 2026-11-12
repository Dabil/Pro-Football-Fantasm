// Draws the GameTime matchup, situation, and quarter scores.
#pragma once

#include "Rendering/Composition/PageComposer.h"

struct GameTimeDisplayState;

namespace ScoreboardView
{
void draw(Composition::PageComposer &page, const Rect &area, const GameTimeDisplayState &display);
}
