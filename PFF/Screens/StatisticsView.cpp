#include "Screens/StatisticsView.h"
#include "GCE/Database/WorldGameSessionState.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

void StatisticsView::draw(Composition::PageComposer &page, const Rect &area, const WorldTestRuntimeState &state)
{
    const int valueWidth = area.size.width >= 52 ? 12 : 9;
    const int labelWidth = std::max(1, area.size.width - 2 * valueWidth);
    const auto stat = [&](int row, std::string_view label, const std::string &away, const std::string &home) {
        std::ostringstream text;
        text << std::left << std::setw(labelWidth) << TextClip::clipUtf8Text(label, labelWidth)
             << std::right << std::setw(valueWidth) << away << std::setw(valueWidth) << home;
        page.writeLine(area, row, TextClip::clipUtf8Text(text.str(), area.size.width), PFF_Colors::TextHighlightColor);
    };
    const auto pair = [](int first, int second) {
        return std::to_string(first) + " / " + std::to_string(second);
    };
    const auto &away = state.teams[0];
    const auto &home = state.teams[1];
    const bool compact = labelWidth < 28;
    stat(0, "Team Statistics", "Away", "Home");
    stat(2, "First downs", std::to_string(away.firstDowns), std::to_string(home.firstDowns));
    stat(3, compact ? "Rush attempts / yards" : "Rushing attempts / yards",
         pair(away.rushes, away.rushYards), pair(home.rushes, home.rushYards));
    stat(4, compact ? "Pass comp / attempts" : "Passing complete / attempts",
         pair(away.completions, away.attempts), pair(home.completions, home.attempts));
    stat(5, compact ? "Pass yards / sacks" : "Passing yards / sacks taken",
         pair(away.passYards, away.sacks), pair(home.passYards, home.sacks));
    stat(6, compact ? "INT / fumbles lost" : "Interceptions / fumbles lost",
         pair(away.interceptions, away.fumblesLost), pair(home.interceptions, home.fumblesLost));
    stat(7, "Punts / yards", pair(away.punts, away.puntYards), pair(home.punts, home.puntYards));
}
