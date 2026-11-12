// GameTime view. Declares the widgets and rendering entry points for live games and replays.

#pragma once
#include "Screens/GameTimeController.h"
#include "Screens/ScoreboardView.h"
#include "Screens/Screen.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ScrollBox.h"
#include <functional>

class GameTimeView : public Screen
{
  public:
    static constexpr bool DefaultShowScoreboardArtwork = true;

    GameTimeView(Assets::AssetLibrary &, std::unique_ptr<PffTest::GameTimeModel>, WorldActor,
                 std::function<void(const WorldEditCommit &)>, std::function<void()> returnToCaller,
                 bool showScoreboardArtwork = DefaultShowScoreboardArtwork);
    void onEnter() override;
    bool handleEvent(const Input::Event &) override;
    void update(const Animation::TickEvent &) override;
    void draw(Surface &) override;

  private:
    void updateControls();
    void returnToCallerIfReady();
    void rebuildLog(int width);
    GameTimeController m_controller;
    ScoreboardView::Artwork m_scoreboard;
    std::function<void()> m_return;
    ContainerWidget m_root;
    ScrollBox *m_log = nullptr;
    Button *m_next = nullptr, *m_run = nullptr, *m_pause = nullptr, *m_returnButton = nullptr;
    Button *m_resume = nullptr, *m_previous = nullptr;
    Button *m_broadcast = nullptr, *m_detail = nullptr, *m_pbp = nullptr;
    std::size_t m_drawnLogRevision = 0;
    int m_logWidth = 0;
};
