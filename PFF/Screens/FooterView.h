// Draws the league name and clock in the standard footer.
#pragma once

#include "Rendering/Composition/PageComposer.h"
#include "Screens/LeagueFooter.h"
#include <string>

namespace FooterView
{
inline constexpr int Height = PffLeagueFooter::Height;

void draw(Composition::PageComposer &page, const Rect &footer,
          const std::string &leagueName, const std::string &clock);
}
