#pragma once
#include "Database/GenericTestGame.h"
#include "Screens/Screen.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ScrollBox.h"
#include <functional>

class GameTimeScreen : public Screen
{
  public:
    GameTimeScreen(std::unique_ptr<PffTest::GenericTestGame>, WorldActor,
                   std::function<void(const WorldEditCommit &)>, std::function<void()> returnToCaller);
    void onEnter() override;
    bool handleEvent(const Input::Event &) override;
    void update(const Animation::TickEvent &) override;
    void draw(Surface &) override;

  private:
    void nextPlay();
    void saveAndReturn();
    void resumeGame();
    void updateControls();
    void returnToCallerIfReady();
    void rebuildLog(int width);
    std::unique_ptr<PffTest::GenericTestGame> m_game;
    WorldActor m_actor;
    std::function<void(const WorldEditCommit &)> m_committed;
    std::function<void()> m_return;
    ContainerWidget m_root;
    ScrollBox *m_log = nullptr;
    Button *m_next = nullptr, *m_run = nullptr, *m_pause = nullptr, *m_returnButton = nullptr;
    Button *m_resume = nullptr;
    bool m_running = false, m_returnPending = false, m_dirty = true, m_follow = true;
    int m_logWidth = 0;
    std::string m_error;
};
