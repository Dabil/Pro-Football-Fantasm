// GameTime controller API. Accepts live and replay commands and exposes read-only display state.

#pragma once

#include "Database/GameTimeModel.h"
#include "Screens/PffBroadcast.h"
#include <functional>
#include <chrono>

// Read-only state for one render. Read it again after issuing a command.
struct GameTimeTeamName
{
    std::string city, nickname;
};

struct GameTimeDisplayState
{
    const PffTest::State &game;
    const WorldTestMatchup &matchup;
    const PffBroadcast::Play *play;
    const WorldTestQueuedEvent *interruption;
    const std::string &error;
    PffBroadcast::View view;
    std::size_t playIndex, playCount, logRevision;
    bool replay, running, canNext, canPrevious, canRun, canPause, canResume, canReturn;
    const std::array<GameTimeTeamName, 2> &teamNames;
};

// Translates screen commands into session operations or read-only replay navigation.
class GameTimeController
{
  public:
    GameTimeController(std::unique_ptr<PffTest::GameTimeModel> game, WorldActor actor,
                       std::function<void(const WorldEditCommit &)> committed);
    void enter();
    void tick();
    void nextPlay();
    std::vector<PffTest::PlayCall> playCalls() const;
    void makeCall(std::string_view key);
    void toggleDecisionPause();
    bool decisionPaused() const;
    bool canTimeout() const;
    void timeout();
    int playClockRemaining() const;
    int gameClockRemaining() const;
    void previousPlay();
    void selectView(PffBroadcast::View view);
    void run();
    void pause();
    void resumeGame();
    void saveAndReturn();
    bool takeReturnRequest();
    GameTimeDisplayState display() const;

  private:
    void advance(std::string_view call = {});
    void syncDecisionClock();
    int elapsedDecisionSeconds() const;
    std::unique_ptr<PffTest::GameTimeModel> m_game;
    WorldActor m_actor;
    std::function<void(const WorldEditCommit &)> m_committed;
    PffBroadcast::View m_view = PffBroadcast::View::Broadcast;
    std::vector<PffBroadcast::Play> m_plays;
    std::vector<PffTest::State> m_replayStates;
    PffTest::State m_emptyReplay{};
    std::array<GameTimeTeamName, 2> m_teamNames;
    std::size_t m_playIndex = 0, m_logRevision = 0;
    bool m_replay = false, m_running = false, m_returnPending = false;
    bool m_decisionPaused = false;
    std::int64_t m_decisionGeneration = -1;
    bool m_decisionClockRunning = false;
    using Clock = std::chrono::steady_clock;
    Clock::time_point m_deadline{}, m_pausedAt{}, m_decisionStart{};
    std::string m_error;
};
