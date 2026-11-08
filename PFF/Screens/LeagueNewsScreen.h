#pragma once

#include "App/LoadedWorldContext.h"
#include "Assets/AssetLibrary.h"
#include "Screens/Screen.h"
#include "UI/Widgets/ContainerWidget.h"

#include <functional>
#include <vector>

class Button;
class ScrollBox;

class LeagueNewsScreen final : public Screen
{
public:
    LeagueNewsScreen(Assets::AssetLibrary& assets, std::function<void()> returnToLeaguePlay,
        std::function<LoadedWorldContext()> context);

    void onEnter() override;
    bool handleEvent(const Input::Event& event) override;
    void draw(Surface& surface) override;

private:
    Assets::AssetLibrary& m_assets;
    std::function<void()> m_returnToLeaguePlay;
    void refresh();
    void rebuildDates(int width);
    void rebuildNews(int width);
    std::function<LoadedWorldContext()> m_contextProvider;
    LoadedWorldContext m_context;
    std::string m_message;
    std::vector<std::string> m_days;
    std::vector<std::vector<std::string>> m_entries;
    std::size_t m_selectedDay = 0;
    int m_datesWidth = -1, m_newsWidth = -1;
    ScrollBox* m_dates = nullptr;
    ScrollBox* m_news = nullptr;
    TextObject m_title;
    ContainerWidget m_root;
    Button* m_return = nullptr;
    bool m_returnPending = false;
};
