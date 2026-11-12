#include "Screens/ScoreboardView.h"
#include "Screens/GameTimeController.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

void ScoreboardView::draw(Composition::PageComposer &page, const Rect &area,
                          const GameTimeDisplayState &display)
{
    if (area.size.width <= 0 || area.size.height <= 0)
        return;

    const auto write = [&](int row, std::string_view text) {
        page.writeLine(area, row, TextClip::clipUtf8Text(text, area.size.width), PFF_Colors::TextHighlightColor);
    };
    const auto &state = display.game;
    const auto &game = display.matchup;
    write(0, display.replay ? "GAME TIME - Replay" : "GAME TIME - Generic Level Sim");

    std::ostringstream clock;
    if (state.quarter == 5)
        clock << "Overtime";
    else
        clock << "Quarter " << state.quarter;
    clock << "  " << state.remaining / 60 << ':' << std::setw(2) << std::setfill('0') << state.remaining % 60;
    const auto spot = PffBroadcast::yardLine(state.offense ? game.homeName : game.awayName,
                                             state.offense ? game.awayName : game.homeName, state.spot);

    std::string situation;
    if (display.replay && !display.play)
        situation = "Replay has no plays";
    else if (state.final)
        situation = "FINAL - Results and PBP saved";
    else if (state.pendingKick >= 0)
        situation = clock.str() + "  |  Kickoff pending: " +
                    (state.pendingKick ? game.homeName : game.awayName) + "  |  " + spot;
    else
        situation = clock.str() + "  |  " + (state.offense ? game.homeName : game.awayName) +
                    "  |  Down " + std::to_string(state.down) + " to go " +
                    std::to_string(state.lineToGain - state.spot) + "  |  " + spot;

    int row = 2;
    const int tableRows = 3;
    const auto writeWrapped = [&](const std::string &text, int endRow) {
        for (const auto &line : wrapText(text, area.size.width))
        {
            if (row >= endRow)
                break;
            write(row++, line);
        }
    };
    writeWrapped(game.awayName + "  " + std::to_string(state.teams[0].score()) + "  at  " +
                     game.homeName + "  " + std::to_string(state.teams[1].score()),
                 std::min(row + 3, area.size.height - tableRows));
    ++row;
    writeWrapped(situation, area.size.height - tableRows);

    const int nameWidth = std::max(1, area.size.width - 25);
    const auto nameColumn = [&](std::string_view name) {
        auto clipped = TextClip::clipUtf8Text(name, nameWidth - 1);
        const int cells = TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(clipped));
        return clipped + std::string(nameWidth - cells, ' ');
    };
    write(row++, nameColumn("Team") + "  Q1  Q2  Q3  Q4  OT Total");
    for (int team = 0; team < 2; ++team)
    {
        std::ostringstream score;
        score << nameColumn(team == 0 ? game.awayName : game.homeName);
        for (const int points : state.teams[team].quarters)
            score << std::setw(4) << points;
        score << std::setw(5) << state.teams[team].score();
        write(row++, score.str());
    }
}
