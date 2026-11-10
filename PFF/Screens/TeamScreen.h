#pragma once

#include "Screens/Screen.h"
#include "Assets/AssetLibrary.h"
#include "App/LoadedWorldContext.h"
#include "GCE/Database/WorldDatabase.h"
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/Button.h"
#include <functional>

class TeamScreen final : public Screen
{
public:
    TeamScreen(Assets::AssetLibrary& assets,std::function<bool(std::string_view)> dispatch,
        std::function<LoadedWorldContext()> context,LoadedWorldContext selectedWorld,std::int64_t teamSeasonId,
        std::function<void(const LoadedWorldContext&,std::int64_t)> openTeamData = {},
        std::function<void(const LoadedWorldContext&,std::int64_t)> openSchedule = {},
        std::function<void(const LoadedWorldContext&,std::int64_t)> openHeadCoach = {});
    void onEnter() override;
    bool handleEvent(const Input::Event& event) override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;
private:
    void refreshTeam(bool force=false);
    Assets::AssetLibrary& m_assets;
    std::function<bool(std::string_view)> m_dispatch;
    std::function<LoadedWorldContext()> m_context;
    LoadedWorldContext m_selectedWorld,m_displayContext;
    std::int64_t m_teamSeasonId;
    WorldTeamRecord m_team;
    bool m_available=false,m_checked=false;
    std::string m_title,m_message;
    TextObject m_banner;
    ContainerWidget m_root;
    std::vector<Button*> m_options;
    Button* m_return=nullptr;
    Button* m_teamData=nullptr;
    Button* m_schedule=nullptr;
    bool m_schedulePending=false;
    Button* m_headCoach=nullptr;
    bool m_headCoachPending=false;
    std::function<void(const LoadedWorldContext&,std::int64_t)> m_openHeadCoach;
    std::function<void(const LoadedWorldContext&,std::int64_t)> m_openSchedule;
    bool m_openDataPending=false,m_returnPending=false;
    std::function<void(const LoadedWorldContext&,std::int64_t)> m_openTeamData;
};
