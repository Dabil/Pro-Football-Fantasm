// GameTime view. Declares the widgets and rendering entry points for live games and replays.

#pragma once
#include "Screens/GameTimeController.h"
#include "Screens/ScoreboardView.h"
#include "Screens/Screen.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ScrollBox.h"
#include "UI/Widgets/ListBox.h"
#include <functional>
#include <string_view>

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
    std::string_view takeSoundCue();

  private:
    void updateControls();
    void returnToCallerIfReady();
    void rebuildLog(int width);
    void refreshCallWindow();
    void drawCallWindow(Surface&);
    bool handleCallWindowEvent(const Input::Event&);
    void makeSelectedCall();
    void selectCallList(ListBox& list);
    GameTimeController m_controller;
    ScoreboardView::Artwork m_scoreboard;
    std::function<void()> m_return;
    ContainerWidget m_root;
    ContainerWidget m_callRoot;
    std::array<ListBox*, 5> m_callLists{};
    std::array<std::vector<std::string>, 5> m_callKeys;
    ListBox* m_selectedList = nullptr;
    Button* m_makeCall = nullptr;
    Button* m_timeout = nullptr;
    Button* m_pauseCall = nullptr;
    std::int64_t m_callGeneration = -1;
    bool m_callActive = false;
    bool m_callFocused = false;
    bool m_callPointerCaptured = false;
    Rect m_callBounds{};
    std::optional<Point> m_callPosition;
    std::optional<Point> m_dragOffset;
    std::optional<Point> m_lastCallClick;
    std::chrono::steady_clock::time_point m_lastCallClickTime{};
    ScrollBox *m_log = nullptr;
    Button *m_next = nullptr, *m_run = nullptr, *m_pause = nullptr, *m_returnButton = nullptr;
    Button *m_resume = nullptr, *m_previous = nullptr;
    Button *m_broadcast = nullptr, *m_detail = nullptr, *m_pbp = nullptr;
    std::size_t m_drawnLogRevision = 0;
    std::size_t m_audioLogRevision = 0;
    std::string m_lastAudioRecord;
    int m_logWidth = 0;
};
