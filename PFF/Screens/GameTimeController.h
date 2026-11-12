// GameTime controller API. Accepts live and replay commands and exposes read-only display state.

#pragma once

#include "Database/PffGameSession.h"
#include "Screens/PffBroadcast.h"
#include <functional>

// Read-only state for one render. Read it again after issuing a command.
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
};

// Translates screen commands into session operations or read-only replay navigation.
class GameTimeController
{
  public:
    GameTimeController(std::unique_ptr<PffTest::PffGameSession> game, WorldActor actor,
                       std::function<void(const WorldEditCommit &)> committed);
    void enter();
    void tick();
    void nextPlay();
    void previousPlay();
    void selectView(PffBroadcast::View view);
    void run();
    void pause();
    void resumeGame();
    void saveAndReturn();
    bool takeReturnRequest();
    GameTimeDisplayState display() const;

  private:
    void advance();
    std::unique_ptr<PffTest::PffGameSession> m_game;
    WorldActor m_actor;
    std::function<void(const WorldEditCommit &)> m_committed;
    PffBroadcast::View m_view = PffBroadcast::View::Broadcast;
    std::vector<PffBroadcast::Play> m_plays;
    std::vector<PffTest::State> m_replayStates;
    PffTest::State m_emptyReplay{};
    std::size_t m_playIndex = 0, m_logRevision = 0;
    bool m_replay = false, m_running = false, m_returnPending = false;
    std::string m_error;
};
