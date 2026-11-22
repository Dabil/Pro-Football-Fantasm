#pragma once
#include "Screens/Screen.h"
#include "App/LoadedWorldContext.h"
#include "Assets/AssetLibrary.h"
#include "GCE/Database/WorldRoster.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/ScrollBox.h"
#include "UI/Widgets/Button.h"
#include <functional>

class RosterTable;
class RosterScreen final : public Screen
{
public:
    RosterScreen(Assets::AssetLibrary&, std::function<bool(std::string_view)> dispatch,
        std::function<LoadedWorldContext()> context, LoadedWorldContext selectedWorld,
        std::int64_t teamSeasonId);
    void onEnter() override;
    void update(const Animation::TickEvent&) override;
    bool handleEvent(const Input::Event&) override;
    void draw(Surface&) override;
private:
    void refresh();
    void rebuildRows();
    void choose(const std::string&);
    void cycleFocus(int direction);
    void movePlayer(int direction);
    void revealPlayer();
    std::function<bool(std::string_view)> m_dispatch;
    std::function<LoadedWorldContext()> m_context;
    LoadedWorldContext m_selected, m_display;
    std::int64_t m_teamSeasonId;
    WorldRosterLoadResult m_roster;
    std::string m_filter="All", m_primary="All", m_message;
    bool m_checked=false, m_returnPending=false;
    Point m_mouse{-1,-1};
    TextObject m_banner;
    ContainerWidget m_root;
    ScrollBox *m_list=nullptr, *m_controls=nullptr;
    RosterTable* m_table=nullptr;
    Button* m_return=nullptr;
    std::vector<std::pair<std::string,Button*>> m_filters;
};
