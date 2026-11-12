// Draws the GameTime matchup, situation, and quarter scores.
#pragma once

#include "Rendering/Composition/PageComposer.h"
#include <vector>

namespace Assets { class AssetLibrary; }
struct GameTimeDisplayState;

namespace ScoreboardView
{
struct Artwork
{
    TextObject board;
    std::vector<TextObject> digits;
};

Artwork loadArtwork(Assets::AssetLibrary &assets);
Size artworkSummarySize(const Artwork &artwork, const GameTimeDisplayState &display);
int boxScoreWidth(int quarter);
std::vector<TextObject> buildDigits(const TextObject &font);
void drawArtwork(ScreenBuffer &buffer, int x, int y, const TextObject &board,
                 const std::vector<TextObject> &digits, int awayScore, int homeScore);

void draw(Composition::PageComposer &page, const Rect &area, const GameTimeDisplayState &display,
          const Artwork &artwork);
}
