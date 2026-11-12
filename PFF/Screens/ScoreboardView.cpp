#include "Screens/ScoreboardView.h"
#include "Screens/GameTimeController.h"
#include "Assets/AssetLibrary.h"
#include "Rendering/Objects/TextObjectBuilder.h"
#include "Styles/PFF_Theme.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
constexpr int DigitCount = 10;
constexpr int DigitWidth = 8;
constexpr int DigitHeight = 7;
constexpr int DigitStride = DigitWidth + 1;
constexpr int AwayOffset = 2;
constexpr int HomeOffset = 27;
constexpr int ScoreOffsetY = 2;
constexpr int ArtworkRow = 1;
constexpr int TextMatchupRow = 2;
constexpr int TableRows = 3;
constexpr int TeamColumnWidth = 25;
constexpr int PeriodColumnWidth = 4;

std::string quarterLabel(int quarter)
{
    return quarter == 5 ? "Overtime" : "Quarter " + std::to_string(quarter);
}

std::string clockText(int remaining)
{
    std::ostringstream text;
    text << remaining / 60 << ':' << std::setw(2) << std::setfill('0') << remaining % 60;
    return text.str();
}

TextObject crop(const TextObject &source, int left, int top, int width, int height)
{
    if (source.getWidth() < left + width || source.getHeight() < top + height)
        return {};
    TextObjectBuilder builder(width, height);
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
            const auto *cell = source.tryGetCell(left + x, top + y);
            if (!cell)
                return {};
            builder.setCell(x, y, cell->glyph, cell->kind, cell->width, cell->style);
        }
    return builder.build();
}

void drawScore(ScreenBuffer &buffer, int x, int y, const std::vector<TextObject> &digits, int score)
{
    score = std::clamp(score, 0, 99);
    digits[score / 10].draw(buffer, x, y);
    digits[score % 10].draw(buffer, x + DigitStride, y);
}

void drawBoardText(ScreenBuffer &buffer, Point origin, const GameTimeDisplayState &display)
{
    constexpr int BlockWidth = 21;
    constexpr int TeamRow = 11;
    constexpr int ClockRow = 14;
    constexpr int SituationRow = 15;
    const std::array blockStarts{1, 24};
    const auto &state = display.game;
    const bool scrimmage = !state.final && state.pendingKick < 0 && state.down > 0 &&
        (!display.replay || (display.play && !display.play->value("START_STATE", "POSSESSION_TEAM_KEY").empty()));
    const auto centered = [&](int left, int row, std::string_view text, const Style &style) {
        const auto clipped = UnicodeConversion::utf8ToU32(TextClip::clipUtf8Text(text, BlockWidth));
        const int offset = (BlockWidth - TextClip::measureDisplayWidth(clipped)) / 2;
        buffer.writeText(origin.x + left + offset, origin.y + row, clipped, style);
    };
    for (std::size_t team = 0; team < blockStarts.size(); ++team)
    {
        const int left = blockStarts[team];
        const auto &name = display.teamNames[team];
        const auto &style = scrimmage && state.offense == static_cast<int>(team)
            ? PFF_Colors::ScoreboardPossessionText : PFF_Colors::ScoreboardTeamText;
        buffer.fillRect(Rect{Point{origin.x + left, origin.y + TeamRow},
                             Size{BlockWidth, SituationRow - TeamRow + 1}},
                        U' ', PFF_Colors::ScoreboardTeamText);
        centered(left, TeamRow, name.city, style);
        centered(left, TeamRow + 1, name.nickname.empty()
            ? (team == 0 ? display.matchup.awayName : display.matchup.homeName) : name.nickname, style);
        buffer.fillRect(Rect{Point{origin.x + left, origin.y + ClockRow}, Size{BlockWidth, 2}},
                        U' ', PFF_Colors::ScoreboardSituationText);
    }
    centered(blockStarts[0], ClockRow, quarterLabel(state.quarter), PFF_Colors::ScoreboardSituationText);
    // The last two cells of the home clock row are reserved for the future play clock.
    centered(blockStarts[1], ClockRow, clockText(state.remaining), PFF_Colors::ScoreboardSituationText);
    if (scrimmage)
    {
        const std::string down = "Down " + std::to_string(state.down) + " to go " +
            std::to_string(state.lineToGain - state.spot);
        const std::string line = state.spot == 50 ? "At the 50-yard line"
            : state.spot < 50 ? "Own " + std::to_string(state.spot) + "-yard line"
                              : "Opp " + std::to_string(100 - state.spot) + "-yard line";
        centered(blockStarts[0], SituationRow, down, PFF_Colors::ScoreboardSituationText);
        centered(blockStarts[1], SituationRow, line, PFF_Colors::ScoreboardSituationText);
    }
}
}

std::vector<TextObject> ScoreboardView::buildDigits(const TextObject &font)
{
    if (font.getWidth() != DigitCount * DigitStride - 1 || font.getHeight() != DigitHeight)
        return {};

    std::vector<TextObject> digits;
    digits.reserve(DigitCount);
    for (int digit = 0; digit < DigitCount; ++digit)
    {
        auto glyph = crop(font, digit * DigitStride, 0, DigitWidth, DigitHeight);
        if (glyph.isEmpty())
            return {};
        digits.push_back(std::move(glyph));
    }
    return digits;
}

ScoreboardView::Artwork ScoreboardView::loadArtwork(Assets::AssetLibrary &assets)
{
    const auto board = assets.loadTextAsset("pff.scoreboard");
    const auto font = assets.loadTextAsset("pff.scoreboard_font");
    if (!board.success || !board.asset.object || !font.success || !font.asset.object)
        return {};
    return {*board.asset.object, buildDigits(*font.asset.object)};
}

Size ScoreboardView::artworkSummarySize(const Artwork &artwork, const GameTimeDisplayState &display)
{
    if (artwork.board.isEmpty() || artwork.digits.size() != DigitCount ||
        display.game.teams[0].score() > 99 || display.game.teams[1].score() > 99)
        return {};
    return {artwork.board.getWidth(), ArtworkRow + artwork.board.getHeight() + 1 + TableRows};
}

int ScoreboardView::boxScoreWidth(int quarter)
{
    return TeamColumnWidth + PeriodColumnWidth * (quarter == 5 ? 5 : 4);
}

void ScoreboardView::drawArtwork(ScreenBuffer &buffer, int x, int y, const TextObject &board,
                                 const std::vector<TextObject> &digits, int awayScore, int homeScore)
{
    board.draw(buffer, x, y);
    if (digits.size() != DigitCount)
        return;
    drawScore(buffer, x + AwayOffset, y + ScoreOffsetY, digits, awayScore);
    drawScore(buffer, x + HomeOffset, y + ScoreOffsetY, digits, homeScore);
}

void ScoreboardView::draw(Composition::PageComposer &page, const Rect &area,
                          const GameTimeDisplayState &display, const Artwork &artwork)
{
    if (area.size.width <= 0 || area.size.height <= 0)
        return;

    const auto write = [&](int row, std::string_view text) {
        page.writeLine(area, row, TextClip::clipUtf8Text(text, area.size.width), PFF_Colors::TextHighlightColor);
    };
    const auto &state = display.game;
    const auto &game = display.matchup;
    write(0, display.replay ? "GAME TIME - Replay" : "GAME TIME - Generic Level Sim");

    const auto clock = quarterLabel(state.quarter) + " " + clockText(state.remaining);
    const auto spot = PffBroadcast::yardLine(state.offense ? game.homeName : game.awayName,
                                             state.offense ? game.awayName : game.homeName, state.spot);

    std::string situation;
    if (display.replay && !display.play)
        situation = "Replay has no plays";
    else if (state.final)
        situation = "FINAL - Results and PBP saved";
    else if (state.pendingKick >= 0)
        situation = clock + "  |  Kickoff pending: " +
                    (state.pendingKick ? game.homeName : game.awayName) + "  |  " + spot;
    else
        situation = clock + "  |  " + (state.offense ? game.homeName : game.awayName) +
                    "  |  Down " + std::to_string(state.down) + " to go " +
                    std::to_string(state.lineToGain - state.spot) + "  |  " + spot;

    int row = TextMatchupRow;
    const auto writeWrapped = [&](const std::string &text, int endRow) {
        for (const auto &line : wrapText(text, area.size.width))
        {
            if (row >= endRow)
                break;
            write(row++, line);
        }
    };
    const int awayScore = state.teams[0].score();
    const int homeScore = state.teams[1].score();
    const auto &board = artwork.board;
    const auto artworkSize = artworkSummarySize(artwork, display);
    if (artworkSize.height > 0 && artworkSize.width <= area.size.width && artworkSize.height <= area.size.height)
    {
        const int x = area.position.x + (area.size.width - board.getWidth()) / 2;
        const int y = area.position.y + ArtworkRow;
        drawArtwork(page.getTarget(), x, y, board, artwork.digits, awayScore, homeScore);
        drawBoardText(page.getTarget(), Point{x, y}, display);
        row = ArtworkRow + board.getHeight() + 1;
    }
    else
    {
        writeWrapped(game.awayName + "  " + std::to_string(awayScore) + "  at  " +
                         game.homeName + "  " + std::to_string(homeScore),
                     std::min(row + 3, area.size.height - TableRows));
        ++row;
        writeWrapped(situation, area.size.height - TableRows);
    }
    const int periodCount = state.quarter == 5 ? 5 : 4;
    const int tableLeft = std::max(0, (area.size.width - boxScoreWidth(state.quarter)) / 2);
    const auto cell = [&](int cellRow, int offset, int width, std::string_view value, bool rightAligned) {
        const int left = tableLeft + offset;
        const int available = std::min(width, area.size.width - left);
        if (cellRow >= area.size.height || available <= 0)
            return;
        const auto text = TextClip::clipUtf8Text(value, available);
        const int padding = rightAligned
            ? available - TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(text)) : 0;
        const Rect bounds{Point{area.position.x + left + padding, area.position.y + cellRow},
                          Size{available - padding, 1}};
        page.writeLine(bounds, 0, text, PFF_Colors::TextHighlightColor);
    };
    constexpr std::array<std::string_view, 5> headings{"Q1", "Q2", "Q3", "Q4", "OT"};
    cell(row, 0, TeamColumnWidth, "Team", false);
    for (int period = 0; period < periodCount; ++period)
        cell(row, TeamColumnWidth + period * PeriodColumnWidth, PeriodColumnWidth, headings[period], true);
    for (int team = 0; team < 2; ++team)
    {
        ++row;
        cell(row, 0, TeamColumnWidth, team == 0 ? game.awayName : game.homeName, false);
        for (int period = 0; period < periodCount; ++period)
            cell(row, TeamColumnWidth + period * PeriodColumnWidth, PeriodColumnWidth,
                 std::to_string(state.teams[team].quarters[period]), true);
    }
}
