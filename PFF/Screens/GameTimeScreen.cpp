#include "Screens/GameTimeScreen.h"
#include "Input/Event.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Surface.h"
#include "Styles/PFF_Theme.h"
#include "UI/Widgets/Label.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
std::vector<PffTest::State> replayStates(const std::vector<PffBroadcast::Play> &plays,
                                         const WorldTestMatchup &game)
{
    std::vector<PffTest::State> states;
    states.reserve(plays.size());
    PffTest::State state;
    const auto side = [&](const std::string &key) { return key == game.homeCoreKey ? 1 : 0; };
    for (const auto &play : plays)
    {
        const auto field = [&](std::string_view name) { return play.value("START_STATE", name); };
        state.quarter = std::clamp(PffBroadcast::number(field("QUARTER")), 1, 5);
        const auto clock = field("CLOCK");
        const auto colon = clock.find(':');
        state.remaining = colon == std::string::npos ? PffBroadcast::number(clock)
                         : 60 * PffBroadcast::number(clock.substr(0, colon)) +
                               PffBroadcast::number(clock.substr(colon + 1));
        const bool kickoff = play.value("PLAY_CONTEXT", "PlayKind") == "KICKOFF";
        state.offense = side(kickoff ? play.value("PLAY_CONTEXT", "KickingTeamKey")
                                    : field("POSSESSION_TEAM_KEY"));
        state.pendingKick = kickoff ? state.offense : -1;
        const int yard = PffBroadcast::number(field("BALL_AT_YARDLINE"));
        state.spot = side(field("BALL_AT_TEAM_KEY")) == state.offense ? yard : 100 - yard;
        state.down = PffBroadcast::number(field("DOWN"));
        state.lineToGain = state.spot + PffBroadcast::number(field("TOGO"));
        // The recorded score is authoritative at the replay cursor.
        for (int team = 0; team < 2; ++team)
        {
            const int score = PffBroadcast::number(field(team == 0 ? "AWAY_SCORE" : "HOME_SCORE"));
            state.teams[team].quarters[state.quarter - 1] += score - state.teams[team].score();
        }
        states.push_back(state);

        // Accumulate only recorded outcomes from plays already shown.
        std::string kind, detail;
        int team = state.offense, value = 0;
        for (const auto &tag : play.tags)
        {
            if (tag.name == "ACTION_BEGIN")
            {
                kind.clear();
                detail.clear();
                team = state.offense;
                value = 0;
            }
            else if (tag.name == "ActionKind")
                kind = tag.value;
            else if (tag.name == "TeamKey" && !kind.empty())
                team = side(tag.value);
            else if (tag.name == "ActionValue")
                value = PffBroadcast::number(tag.value);
            else if (tag.name == "ActionDetail")
                detail = tag.value;
            else if (tag.name == "ACTION_END")
            {
                auto &stats = state.teams[team];
                const int yards = std::clamp(value, -state.spot, 100 - state.spot);
                if (kind == "SCORE" || kind == "SAFETY" || kind == "EXTRA_POINT")
                    stats.quarters[state.quarter - 1] += value;
                else if (kind == "FIRST_DOWN")
                    ++stats.firstDowns;
                else if (kind == "RUSH")
                {
                    ++stats.rushes;
                    stats.rushYards += yards;
                }
                else if (kind == "COMPLETE_PASS")
                {
                    ++stats.attempts;
                    ++stats.completions;
                    stats.passYards += yards;
                }
                else if (kind == "INCOMPLETE_PASS")
                    ++stats.attempts;
                else if (kind == "SACK")
                    ++stats.sacks;
                else if (kind == "PUNT")
                {
                    ++stats.punts;
                    stats.puntYards += std::min(value, 100 - state.spot);
                }
                else if (kind == "POSSESSION" && detail == "interception")
                {
                    ++state.teams[1 - team].attempts;
                    ++state.teams[1 - team].interceptions;
                }
                else if (kind == "POSSESSION" && detail == "fumble recovery")
                    ++state.teams[1 - team].fumblesLost;
                kind.clear();
            }
        }
    }
    return states;
}
} // namespace

GameTimeScreen::GameTimeScreen(std::unique_ptr<PffTest::PffGameSession> game, WorldActor actor,
                               std::function<void(const WorldEditCommit &)> committed,
                               std::function<void()> back)
    : m_game(std::move(game)), m_actor(std::move(actor)), m_committed(std::move(committed)),
      m_return(std::move(back))
{
    auto log = std::make_unique<ScrollBox>();
    auto scrollStyle = log->styleSet();
    scrollStyle.focused = PFF_Colors::ScrollBoxFocusedStyle;
    log->setStyleSet(scrollStyle);
    m_log = log.get();
    m_root.addChild(std::move(log));
    for (const auto &command : {std::pair<const char *, const char *>{"Next Play", "next"},
                                {"Run to Final", "run"},
                                {"Previous Play", "previous"},
                                {"Broadcast", "broadcast"},
                                {"Detail", "detail"},
                                {"PBP", "pbp"},
                                {"Pause", "pause"},
                                {"Resume Game", "resume"},
                                {"Return", "return"}})
    {
        auto b = std::make_unique<Button>(Rect{}, command.first, command.second);
        b->setNormalStyle(PFF_Colors::ButtonNormalColor);
        b->setHoverStyle(PFF_Colors::ButtonHoverColor);
        b->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        b->setPressedStyle(PFF_Colors::ButtonPressedColor);
        b->setDisabledStyle(PFF_Colors::ButtonNormalColor.withDim(true));
        b->setActivationCallback([this](const ButtonActivationResult &r) {
            if (!r.activated)
                return;
            if (r.commandId == "next")
            {
                m_running = false;
                nextPlay();
            }
            else if (r.commandId == "previous")
                previousPlay();
            else if (r.commandId == "broadcast")
                selectView(PffBroadcast::View::Broadcast);
            else if (r.commandId == "detail")
                selectView(PffBroadcast::View::Detail);
            else if (r.commandId == "pbp")
                selectView(PffBroadcast::View::Pbp);
            else if (r.commandId == "run")
            {
                m_error.clear();
                m_running = true;
            }
            else if (r.commandId == "pause")
                m_running = false;
            else if (r.commandId == "return")
                saveAndReturn();
            else if (r.commandId == "resume")
                resumeGame();
            updateControls();
        });
        if (std::string(command.second) == "next")
            m_next = b.get();
        else if (std::string(command.second) == "previous")
            m_previous = b.get();
        else if (std::string(command.second) == "broadcast")
            m_broadcast = b.get();
        else if (std::string(command.second) == "detail")
            m_detail = b.get();
        else if (std::string(command.second) == "pbp")
            m_pbp = b.get();
        else if (std::string(command.second) == "run")
            m_run = b.get();
        else if (std::string(command.second) == "pause")
            m_pause = b.get();
        else if (std::string(command.second) == "resume")
            m_resume = b.get();
        else
            m_returnButton = b.get();
        m_root.addChild(std::move(b));
    }
    updateControls();
}
void GameTimeScreen::onEnter()
{
    m_root.resetFocus();
    m_replay = m_game->state().final;
    if (m_replay)
    {
        std::istringstream input(std::string(m_game->taggedHistory()));
        m_plays = PffBroadcast::readPlays(input);
        m_replayStates = replayStates(m_plays, m_game->matchup());
    }
    else
        m_plays = PffBroadcast::latestPlay(m_game->taggedHistory());
    m_running = false;
    if (!m_replay && m_plays.empty() && !m_game->interruption())
        nextPlay();
    updateControls();
}
void GameTimeScreen::nextPlay()
{
    if (m_replay)
    {
        if (m_playIndex + 1 < m_plays.size())
        {
            ++m_playIndex;
            m_dirty = true;
        }
        return;
    }
    if (m_game->state().final)
        return;
    const bool wasRunning = m_running;
    const auto ordinal = m_game->state().playOrdinal;
    auto result = m_game->next(m_actor);
    if (result.success && m_game->state().playOrdinal == ordinal && !m_game->state().final && !m_game->interruption())
    {
        if (m_committed && result.value.changed)
            m_committed(result.value);
        result = m_game->next(m_actor);
    }
    if (m_game->state().playOrdinal != ordinal)
        m_plays = PffBroadcast::latestPlay(m_game->taggedHistory());
    m_dirty = true;
    if (result.success)
    {
        m_error.clear();
        if (m_committed)
            m_committed(result.value);
    }
    else
    {
        m_error = "Game stopped: " + result.message +
                  (m_game->interruption() ? "  Resume Game or Return retries the save."
                                          : "  Next Play retries without rerolling.");
        m_running = false;
    }
    const bool final = m_game->state().final;
    if (m_game->interruption())
        m_running = false;
    if (final)
    {
        m_running = false;
        m_returnPending = wasRunning;
    }
    updateControls();
}
void GameTimeScreen::previousPlay()
{
    if (m_replay && m_playIndex > 0)
    {
        --m_playIndex;
        m_dirty = true;
    }
}
void GameTimeScreen::selectView(PffBroadcast::View view)
{
    if (m_view != view)
    {
        m_view = view;
        m_dirty = true;
    }
}
void GameTimeScreen::updateControls()
{
    const bool final = m_game->state().final;
    const bool interrupted = bool(m_game->interruption());
    m_next->setEnabled(m_replay ? m_playIndex + 1 < m_plays.size()
                                : !final && !interrupted && !m_returnPending && !m_running);
    m_previous->setEnabled(m_replay && m_playIndex > 0);
    m_run->setEnabled(!final && !interrupted && !m_returnPending && !m_running);
    m_pause->setEnabled(!final && !m_returnPending && m_running);
    m_returnButton->setEnabled(!m_returnPending);
    m_resume->setEnabled(interrupted && !m_returnPending);
    for (const auto &[button, view] : {std::pair{m_broadcast, PffBroadcast::View::Broadcast},
                                      std::pair{m_detail, PffBroadcast::View::Detail},
                                      std::pair{m_pbp, PffBroadcast::View::Pbp}})
    {
        const bool dim = m_view != view;
        button->setNormalStyle(PFF_Colors::ButtonNormalColor.withDim(dim));
        button->setHoverStyle(PFF_Colors::ButtonHoverColor.withDim(dim));
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor.withDim(dim));
        button->setPressedStyle(PFF_Colors::ButtonPressedColor.withDim(dim));
    }
}
void GameTimeScreen::resumeGame()
{
    m_running = false;
    const auto result = m_game->resume(m_actor);
    m_dirty = true;
    if (!result.success)
        m_error = "Unable to resume: " + result.message;
    else
    {
        if (m_committed)
            m_committed(result.value.commit);
        m_error.clear();
        m_plays = PffBroadcast::latestPlay(m_game->taggedHistory());
    }
    updateControls();
}
void GameTimeScreen::saveAndReturn()
{
    if (m_returnPending)
        return;
    m_running = false;
    if (m_game->state().final)
    {
        m_returnPending = true;
        updateControls();
        return;
    }
    const auto result = m_game->save(m_actor);
    if (result.success)
    {
        if (m_committed)
            m_committed(result.value);
        m_error.clear();
        m_returnPending = true;
    }
    else
        m_error = "Unable to save: " + result.message + ". The game remains open.";
    updateControls();
}
void GameTimeScreen::returnToCallerIfReady()
{
    if (!m_returnPending)
        return;
    m_returnPending = false;
    // The callback may pop and destroy this screen. Call it only after widget
    // event/update dispatch has unwound, and never access members afterwards.
    const auto back = std::move(m_return);
    if (back)
        back();
}
bool GameTimeScreen::handleEvent(const Input::Event &event)
{
    // Every user exit follows the same save-before-return path.
    if (const auto *command = event.asCommand())
    {
        if (command->command.code == Input::CommandCode::Cancel ||
            command->command.code == Input::CommandCode::Back)
        {
            saveAndReturn();
            returnToCallerIfReady();
            return true;
        }
    }
    const bool handled = m_root.handleEvent(event);
    returnToCallerIfReady();
    return handled;
}
void GameTimeScreen::update(const Animation::TickEvent &event)
{
    if (m_running)
        nextPlay(); // One bounded play per UI tick; rendering/input remain live.
    m_root.update(event);
    returnToCallerIfReady();
}
void GameTimeScreen::rebuildLog(int width)
{
    if (!m_dirty && width == m_logWidth)
        return;
    m_log->clearChildren();
    int row = 0;
    const auto &game = m_game->matchup();
    const PffBroadcast::Teams teams{game.awayCoreKey, game.homeCoreKey, game.awayName, game.homeName};
    const auto lines = m_plays.empty() ? std::vector<std::string>{"No play selected."}
                                      : PffBroadcast::render(m_plays[m_playIndex], teams, m_view);
    for (const auto &text : lines)
    {
        const auto first = text.find_first_not_of(' ');
        const int indent = first == std::string::npos ? 0 : static_cast<int>(first);
        const auto rows = wrapText(std::string_view(text).substr(indent), std::max(1, width - 2 - indent));
        for (const auto &line : rows)
        {
            auto label = std::make_unique<Label>(
                Rect{Point{indent, row++}, Size{std::max(1, width - 2 - indent), 1}}, line);
            label->setTextStyle(text.rfind("===", 0) == 0   ? PFF_Colors::NormalTextHeadingColor
                                : text.rfind("---", 0) == 0 ? PFF_Colors::NormalTextColor.withBold()
                                                            : PFF_Colors::NormalTextColor);
            m_log->addChild(std::move(label));
        }
    }
    m_log->setContentSize(std::max(1, width - 2), row);
    m_log->viewport().scrollTo(0, 0);
    m_logWidth = width;
    m_dirty = false;
}
void GameTimeScreen::draw(Surface &surface)
{
    auto &buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);
    Composition::PageComposer page(buffer);
    page.createFullScreenRegion("Screen");
    const auto bounds = page.resolveRegion("Screen");
    m_root.setBounds(bounds);
    const bool small = bounds.size.width < 90 || bounds.size.height < 30;
    const int width = std::max(1, bounds.size.width - 4);
    auto write = [&](int row, const std::string &text) {
        page.writeLine(Rect{Point{2, row}, Size{width, 1}}, 0, TextClip::clipUtf8Text(text, width),
                       PFF_Colors::TextHighlightColor);
    };
    const PffTest::State emptyReplay{};
    const auto &state = m_replay ? (m_replayStates.empty() ? emptyReplay : m_replayStates[m_playIndex])
                                 : m_game->state();
    const auto &game = m_game->matchup();
    write(1, m_replay ? "GAME TIME - Replay" : "GAME TIME - Generic Level Sim");
    std::ostringstream clock;
    if (state.quarter == 5)
        clock << "Overtime";
    else
        clock << "Quarter " << state.quarter;
    clock << "  " << state.remaining / 60 << ':' << std::setw(2)
          << std::setfill('0') << state.remaining % 60;

    const auto currentSpot = PffBroadcast::yardLine(state.offense ? game.homeName : game.awayName,
                                                    state.offense ? game.awayName : game.homeName, state.spot);

    write(3, game.awayName + "  " + std::to_string(state.teams[0].score()) + "    at    " +
                 game.homeName + "  " + std::to_string(state.teams[1].score()));
    write(5, m_replay && m_plays.empty() ? "Replay has no plays"
                     : state.final ? "FINAL - Results and PBP saved"
                         : state.pendingKick >= 0
                               ? clock.str() + "  |  Kickoff pending: " +
                                     (state.pendingKick ? game.homeName : game.awayName) + "  |  " + currentSpot
                               : clock.str() + "  |  " + (state.offense ? game.homeName : game.awayName) +
                               "  |  Down " + std::to_string(state.down) + " to go " +
                               std::to_string(state.lineToGain - state.spot) + "  |  " + currentSpot);
    write(7, "Team                         Q1   Q2   Q3   Q4   OT   Total");
    for (int t = 0; t < 2; ++t)
    {
        std::ostringstream score;
        score << std::left << std::setw(28) << TextClip::clipUtf8Text(t ? game.homeName : game.awayName, 27)
              << std::right;
        for (int q : state.teams[t].quarters)
            score << std::setw(5) << q;
        score << std::setw(7) << state.teams[t].score();
        write(8 + t, score.str());
    }
    write(11, "Team statistics                   Away          Home");
    auto stat = [&](int row, const std::string &label, const std::string &away, const std::string &home) {
        std::ostringstream o;
        o << std::left << std::setw(32) << label << std::setw(14) << away << home;
        write(row, o.str());
    };
    const auto &a = state.teams[0];
    const auto &h = state.teams[1];
    stat(12, "First downs", std::to_string(a.firstDowns), std::to_string(h.firstDowns));
    stat(13, "Rushing attempts / yards", std::to_string(a.rushes) + " / " + std::to_string(a.rushYards),
         std::to_string(h.rushes) + " / " + std::to_string(h.rushYards));
    stat(14, "Passing complete / attempts",
         std::to_string(a.completions) + " / " + std::to_string(a.attempts),
         std::to_string(h.completions) + " / " + std::to_string(h.attempts));
    stat(15, "Passing yards / sacks taken", std::to_string(a.passYards) + " / " + std::to_string(a.sacks),
         std::to_string(h.passYards) + " / " + std::to_string(h.sacks));
    stat(16, "Interceptions / fumbles lost",
         std::to_string(a.interceptions) + " / " + std::to_string(a.fumblesLost),
         std::to_string(h.interceptions) + " / " + std::to_string(h.fumblesLost));
    stat(17, "Punts / yards", std::to_string(a.punts) + " / " + std::to_string(a.puntYards),
         std::to_string(h.punts) + " / " + std::to_string(h.puntYards));
    if (m_replay && !m_plays.empty())
        write(18, "Play " + std::to_string(m_playIndex + 1) + " of " + std::to_string(m_plays.size()));
    constexpr int tabsY = 19;
    m_broadcast->setBounds(Rect{Point{2, tabsY}, Size{14, 3}});
    m_detail->setBounds(Rect{Point{18, tabsY}, Size{14, 3}});
    m_pbp->setBounds(Rect{Point{34, tabsY}, Size{14, 3}});
    const int logY = tabsY + 3;
    const int logHeight = std::max(1, bounds.size.height - logY - 6);
    m_log->setBounds(Rect{Point{2, logY}, Size{width, logHeight}});
    rebuildLog(width);
    const int y = std::max(20, bounds.size.height - 5);
    m_next->setBounds(Rect{Point{2, y}, Size{20, 3}});
    m_run->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_previous->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_pause->setBounds(Rect{Point{46, y}, Size{20, 3}});
    m_resume->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_returnButton->setBounds(Rect{Point{std::max(68, bounds.size.width - 22), y}, Size{20, 3}});
    updateControls();
    m_log->setVisible(!small);
    m_next->setVisible(!small);
    m_run->setVisible(!small && !m_replay && !m_game->interruption());
    m_previous->setVisible(!small && m_replay);
    m_broadcast->setVisible(!small);
    m_detail->setVisible(!small);
    m_pbp->setVisible(!small);
    m_resume->setVisible(!small && bool(m_game->interruption()));
    m_pause->setVisible(!small && !m_replay);
    m_returnButton->setVisible(!small);
    if (small)
        write(19, "Enlarge the window to at least 90 x 30 to view game controls.");
    if (m_game->interruption())
        write(18, "EVENT: " + m_game->interruption()->blocker);
    write(bounds.size.height - 2,
          m_error.empty() ? (m_game->interruption() ? "Waiting for event. Handle it, then Resume Game. Return keeps saved progress."
                             : m_replay      ? "Next Play and Previous Play move through the replay."
                             : m_running     ? "Running to final... Pause to inspect; Return saves and exits."
                             : state.final ? "Final saved. Scroll the play-by-play or select Return."
                                           : "Next Play advances once. Return saves and exits.")
                          : m_error);
    m_root.draw(surface);
}
