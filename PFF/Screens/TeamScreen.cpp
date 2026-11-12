#include "Screens/TeamScreen.h"
#include "Screens/GameTimeFormatting.h"
#include "Styles/PFF_Theme.h"
#include "Rendering/Surface.h"
#include "Screens/ContentControlFrameView.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Utilities/Text/TextClip.h"
#include <algorithm>

TeamScreen::TeamScreen(Assets::AssetLibrary& assets,std::function<bool(std::string_view)> dispatch,
    std::function<LoadedWorldContext()> context,LoadedWorldContext selectedWorld,std::int64_t teamSeasonId,
    std::function<void(const LoadedWorldContext&,std::int64_t)> openTeamData,
    std::function<void(const LoadedWorldContext&,std::int64_t)> openSchedule,
    std::function<void(const LoadedWorldContext&,std::int64_t)> openHeadCoach)
    : m_assets(assets),m_dispatch(std::move(dispatch)),m_context(std::move(context)),
      m_selectedWorld(std::move(selectedWorld)),m_teamSeasonId(teamSeasonId),m_openTeamData(std::move(openTeamData))
{
    m_openSchedule=std::move(openSchedule);
    m_openHeadCoach=std::move(openHeadCoach);
    for(const auto* label: {"Team Data","Head Coach","Roster","Statistics","Schedule","Team News","Return"})
    {
        auto button=std::make_unique<Button>(label);
        if(std::string_view(label)=="Return")
        {
            m_return=button.get();
            button->setActivationCallback([this](const ButtonActivationResult& result) {
                if(result.activated)m_returnPending=true;
            });
        }
        else {button->setEnabled(false);m_options.push_back(button.get());
            if(std::string_view(label)=="Team Data"){m_teamData=button.get();button->setActivationCallback([this](const ButtonActivationResult& r){if(r.activated)m_openDataPending=true;});}}
        if(std::string_view(label)=="Schedule")
        {
            m_schedule=button.get();
            button->setActivationCallback([this](const ButtonActivationResult& r){if(r.activated)m_schedulePending=true;});
        }
        if (std::string_view(label) == "Head Coach")
        {
            m_headCoach = button.get();
            button->setActivationCallback([this](const ButtonActivationResult& result) {
                if (result.activated) m_headCoachPending = true;
            });
        }
        m_root.addChild(std::move(button));
    }
}

void TeamScreen::onEnter() {m_root.resetFocus();refreshTeam(true);}

void TeamScreen::refreshTeam(bool force)
{
    const auto context=m_context?m_context():LoadedWorldContext{};
    if(!force && m_checked && context.loaded==m_displayContext.loaded &&
        context.databasePath==m_displayContext.databasePath && context.worldKey==m_displayContext.worldKey &&
        context.activeCompetitionSeasonId==m_displayContext.activeCompetitionSeasonId &&
        context.currentTimeUtc==m_displayContext.currentTimeUtc && context.dataRevision==m_displayContext.dataRevision && context.worldName==m_displayContext.worldName) return;
    m_displayContext=context;m_checked=true;m_available=false;m_banner.clear();m_teamData->setEnabled(false);
    m_schedule->setEnabled(false);
    m_headCoach->setEnabled(false);
    m_title="Team unavailable";m_message="This team is not available in the active league and season.";
    if(!context.hasActiveCompetitionSeason() || context.databasePath!=m_selectedWorld.databasePath ||
        context.worldKey!=m_selectedWorld.worldKey ||
        context.activeCompetitionSeasonId!=m_selectedWorld.activeCompetitionSeasonId) return;
    const auto teams=loadWorldTeams(context.databasePath,context.activeCompetitionSeasonId);
    if(!teams.success) {m_message="Unable to load team information: "+teams.errorMessage;return;}
    const auto team=std::find_if(teams.teams.begin(),teams.teams.end(),[&](const auto& value) {return value.teamSeasonId==m_teamSeasonId;});
    if(team==teams.teams.end()) return;
    m_schedule->setEnabled(bool(m_openSchedule));
    m_headCoach->setEnabled(bool(m_openHeadCoach));
    m_team=*team;m_available=true;m_message.clear();
    if(m_openTeamData){const auto actor=openLocalWorldActor(context.databasePath);if(actor.success){const auto details=loadTeamData(actor.value,m_teamSeasonId);m_teamData->setEnabled(details.success);if(!details.success)m_message="Team Data unavailable: "+details.message;}else m_message="Team Data unavailable: "+actor.message;}
    m_title=m_team.nickname.empty()?m_team.fallbackDisplayName():m_team.nickname;
    const auto font=m_assets.loadBannerFontAsset("banner.ansi_shadow");
    if(font.success && font.hasFont())
        m_banner=BannerFactory::makeBanner(*font.asset.font,m_title,PFF_Colors::ProFootballTitleColor);
}

bool TeamScreen::handleEvent(const Input::Event& event)
{
    const bool handled=m_root.handleEvent(event);
    if(m_returnPending){m_returnPending=false;const auto dispatch=m_dispatch;if(dispatch)dispatch("nav:back");return handled;}
    if(m_schedulePending)
    {
        m_schedulePending=false;
        const auto open=m_openSchedule;
        const auto world=m_displayContext;
        const auto id=m_teamSeasonId;
        m_root.resetFocus();
        if(open)open(world,id);
        return handled;
    }
    if (m_headCoachPending)
    {
        m_headCoachPending = false;
        const auto open = m_openHeadCoach;
        const auto world = m_displayContext;
        const auto id = m_teamSeasonId;
        m_root.resetFocus();
        if (open) open(world, id);
        return true;
    }
    if(m_openDataPending){m_openDataPending=false;const auto open=m_openTeamData;const auto world=m_displayContext;const auto id=m_teamSeasonId;m_root.resetFocus();if(open)open(world,id);}
    return handled;
}
void TeamScreen::update(const Animation::TickEvent& event) {refreshTeam();m_root.update(event);}

void TeamScreen::draw(Surface& surface)
{
    const Style& textStyle=PFF_Colors::TextHighlightColor;
    const Style& normal=PFF_Colors::ButtonNormalColor;
    const Style& active=PFF_Colors::ButtonHoverColor;
    const Style& pressed=PFF_Colors::ButtonPressedColor;
    const Style disabled=normal.withDim(true);
    refreshTeam();
    Composition::PageComposer page(surface.buffer());
    const auto frame = ContentControlFrameView::drawFrame(
        page, m_banner, m_title);
    const auto &screen = frame.screen;
    const auto &content = frame.content;
    const auto &controls = frame.controls;
    const auto &footer = frame.footer;
    if(!m_message.empty()) page.writeLine(content,1,TextClip::clipUtf8Text(m_message,content.size.width),textStyle);
    const int optionSpacing = controls.size.height >= 30 ? 5 : 3;
    for(std::size_t i=0;i<m_options.size();++i)
        m_options[i]->setBounds(Rect{Point{controls.position.x+1,controls.position.y+2+static_cast<int>(i)*optionSpacing},Size{std::max(0,controls.size.width-2),3}});
    m_return->setBounds(PffLeagueFooter::returnBounds(footer));
    for(std::size_t i=0;i<m_root.childCount();++i)
    {
        auto* button=static_cast<Button*>(m_root.childAt(i));
        button->setNormalStyle(normal);button->setFocusedStyle(active);button->setHoverStyle(active);
        button->setPressedStyle(pressed);button->setDisabledStyle(disabled);
    }
    ContentControlFrameView::drawFooter(page, footer,
        m_displayContext.isLoaded() ? m_displayContext.worldName : "No league loaded",
        m_displayContext.isLoaded() ? PffGameTime::formatGameTimeEastern(m_displayContext.currentTimeUtc) : "");
    m_root.setBounds(screen);m_root.draw(surface);
}
