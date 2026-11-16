// GameTime controller implementation. Coordinates play advancement, replay navigation, and save-before-return.

#include "Screens/GameTimeController.h"
#include "GCE/Database/WorldTeamData.h"
#include <algorithm>
#include <sstream>
#include <utility>

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
        if (field("QUARTER").empty())
        {
            states.push_back(state);
            continue;
        }
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
        if (play.value("PENALTY_ENFORCEMENT", "PlayResultStands") == "0")
        {
            if (play.value("PENALTY_ENFORCEMENT", "FirstDownAwarded") == "1")
                ++state.teams[state.offense].firstDowns;
            continue;
        }
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

GameTimeController::GameTimeController(std::unique_ptr<PffTest::GameTimeModel> game, WorldActor actor,
                                       std::function<void(const WorldEditCommit &)> committed)
    : m_game(std::move(game)), m_actor(std::move(actor)), m_committed(std::move(committed))
{
}
void GameTimeController::enter()
{
    const auto &matchup = m_game->matchup();
    const std::array teamIds{matchup.awayId, matchup.homeId};
    for (std::size_t team = 0; team < teamIds.size(); ++team)
    {
        const auto data = loadTeamData(m_actor, teamIds[team]);
        if (data.success)
            m_teamNames[team] = {data.value.city, data.value.nickname};
    }
    ++m_logRevision;
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
    if (!m_replay && m_plays.empty() && !m_game->interruption() && playCalls().empty())
        advance();
    syncDecisionClock();
}
void GameTimeController::advance(std::string_view call)
{
    if (m_replay)
    {
        if (m_playIndex + 1 < m_plays.size())
        {
            ++m_playIndex;
            ++m_logRevision;
        }
        return;
    }
    if (m_game->state().final)
        return;
    const bool wasRunning = m_running;
    const auto generation = m_game->state().generation;
    const auto ordinal = m_game->state().playOrdinal;
    auto result = m_game->next(m_actor, call, call.empty() ? 0 : elapsedDecisionSeconds());
    if (result.success && m_game->state().playOrdinal == ordinal && !m_game->state().final &&
        !m_game->interruption() && m_game->playCalls().empty())
    {
        if (m_committed && result.value.changed)
            m_committed(result.value);
        result = m_game->next(m_actor);
    }
    if (m_game->state().generation != generation)
        m_plays = PffBroadcast::latestPlay(m_game->taggedHistory());
    ++m_logRevision;
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
        m_game->setCpuTakeover(false);
    }
    const bool final = m_game->state().final;
    if (m_game->interruption())
        m_running = false;
    if (final)
    {
        m_running = false;
        m_returnPending = wasRunning;
    }
}
void GameTimeController::previousPlay()
{
    if (m_replay && m_playIndex > 0)
    {
        --m_playIndex;
        ++m_logRevision;
    }
}
void GameTimeController::selectView(PffBroadcast::View view)
{
    if (m_view != view)
    {
        m_view = view;
        ++m_logRevision;
    }
}
void GameTimeController::resumeGame()
{
    if (!display().canResume)
        return;
    m_running = false;
    m_game->setCpuTakeover(false);
    const auto result = m_game->resume(m_actor);
    ++m_logRevision;
    if (!result.success)
        m_error = "Unable to resume: " + result.message;
    else
    {
        if (m_committed)
            m_committed(result.value.commit);
        m_error.clear();
        m_plays = PffBroadcast::latestPlay(m_game->taggedHistory());
        m_decisionGeneration = -1;
        syncDecisionClock();
    }
}
void GameTimeController::saveAndReturn()
{
    if (m_returnPending)
        return;
    m_running = false;
    if (m_game->state().final)
    {
        m_returnPending = true;
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
}
GameTimeDisplayState GameTimeController::display() const
{
    const auto &live = m_game->state();
    const auto &interruption = m_game->interruption();
    const bool canAdvance = !live.final && !interruption && !m_returnPending && !m_running;
    return {m_replay ? (m_replayStates.empty() ? m_emptyReplay : m_replayStates[m_playIndex]) : live,
            m_game->matchup(), m_plays.empty() ? nullptr : &m_plays[m_playIndex],
            interruption ? &*interruption : nullptr, m_error, m_view, m_playIndex, m_plays.size(),
            m_logRevision, m_replay, m_running,
            m_replay ? m_playIndex + 1 < m_plays.size() : canAdvance,
            m_replay && m_playIndex > 0, canAdvance, !live.final && !m_returnPending && m_running,
            bool(interruption) && !m_returnPending, !m_returnPending, m_teamNames};
}

void GameTimeController::nextPlay()
{
    if (display().canNext && playCalls().empty())
        advance();
    syncDecisionClock();
}

void GameTimeController::tick()
{
    syncDecisionClock();
    if (!m_decisionPaused && (playClockRemaining() == 0 ||
        (m_decisionClockRunning && gameClockRemaining() == m_game->decisionClockBoundary())))
    {
        advance("DELAY_OF_GAME");
        syncDecisionClock();
    }
    else if (m_running && playCalls().empty())
        advance();
}

std::vector<PffTest::PlayCall> GameTimeController::playCalls() const
{
    return m_replay ? std::vector<PffTest::PlayCall>{} : m_game->playCalls();
}

void GameTimeController::makeCall(std::string_view key)
{
    syncDecisionClock();
    if (m_decisionPaused) return;
    advance(playClockRemaining() == 0 || (m_decisionClockRunning &&
        gameClockRemaining() == m_game->decisionClockBoundary())
        ? std::string_view("DELAY_OF_GAME") : key);
    syncDecisionClock();
}

void GameTimeController::syncDecisionClock()
{
    const auto& game = m_game->state();
    if (playCalls().empty())
    {
        m_deadline = {};
        m_decisionClockRunning = false;
        m_decisionGeneration = -1;
        return;
    }
    if (m_decisionGeneration == game.generation) return;
    m_decisionGeneration = game.generation;
    const int seconds = m_game->decisionPlayClockSeconds();
    m_decisionStart = Clock::now();
    m_decisionClockRunning = seconds > 0 && m_game->decisionGameClockRunning();
    m_deadline = seconds < 0 ? Clock::time_point{} : m_decisionStart + std::chrono::seconds(seconds);
}

int GameTimeController::elapsedDecisionSeconds() const
{
    if (!m_decisionClockRunning) return 0;
    const auto now = m_decisionPaused ? m_pausedAt : Clock::now();
    const auto cutoff = m_deadline == Clock::time_point{} ? now : std::min(now, m_deadline);
    return std::clamp(static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(cutoff - m_decisionStart).count()),
        0, m_game->state().remaining - m_game->decisionClockBoundary());
}

int GameTimeController::gameClockRemaining() const
{
    return m_game->state().remaining - elapsedDecisionSeconds();
}

int GameTimeController::playClockRemaining() const
{
    if (m_deadline == Clock::time_point{}) return -1;
    const auto now = m_decisionPaused ? m_pausedAt : Clock::now();
    return std::max(0, static_cast<int>(std::chrono::ceil<std::chrono::seconds>(m_deadline - now).count()));
}

bool GameTimeController::decisionPaused() const
{
    return m_decisionPaused;
}

void GameTimeController::toggleDecisionPause()
{
    syncDecisionClock();
    const auto now = Clock::now();
    if (m_decisionPaused && m_deadline != Clock::time_point{})
    {
        m_deadline += now - m_pausedAt;
        m_decisionStart += now - m_pausedAt;
    }
    m_pausedAt = now;
    m_decisionPaused = !m_decisionPaused;
}

bool GameTimeController::canTimeout() const
{
    return !m_decisionPaused && !m_replay && m_game->canCallTimeout();
}

void GameTimeController::timeout()
{
    syncDecisionClock();
    if (!canTimeout()) return;
    if (playClockRemaining() == 0 || (m_decisionClockRunning &&
        gameClockRemaining() == m_game->decisionClockBoundary()))
    {
        advance("DELAY_OF_GAME");
        syncDecisionClock();
        return;
    }
    advance("TIMEOUT");
    syncDecisionClock();
}

void GameTimeController::run()
{
    if (display().canRun)
    {
        m_error.clear();
        m_game->setCpuTakeover(true);
        m_decisionPaused = false;
        m_running = true;
        syncDecisionClock();
    }
}

void GameTimeController::pause()
{
    m_running = false;
    m_game->setCpuTakeover(false);
    syncDecisionClock();
}

bool GameTimeController::takeReturnRequest()
{
    return std::exchange(m_returnPending, false);
}
