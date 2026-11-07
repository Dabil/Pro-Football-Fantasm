// PFF includes
#include "Screens/LeagueScheduleScreen.h"
#include "Screens/TeamLink.h"
#include "Styles/PFF_Theme.h"
#include "Screens/GameTimeFormatting.h"

// TUI includes
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Input/Event.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"

// C++ library includes
#include <algorithm>
#include <array>
#include <utility>

namespace
{
    using Composition::PageComposer;

    std::string displayResult(const WorldScheduleGameRecord& game)
    {
        if (game.hasVisibleScore || game.hasFinalScore)
        {
            const std::string away=(game.awayNickname.empty()?game.awayDisplayName:game.awayNickname)+
                ": "+std::to_string(game.awayScore);
            const std::string home=(game.homeNickname.empty()?game.homeDisplayName:game.homeNickname)+
                ": "+std::to_string(game.homeScore);
            return game.homeScore>game.awayScore ? home+", "+away : away+", "+home;
        }
        if (game.status=="IN_PROGRESS") return "In progress - score unavailable";
        if (game.status=="FINAL") return "Score unavailable";
        if (game.status=="CANCELLED" || game.status=="CANCELED") return "Cancelled";
        return "Pending";
    }
}

LeagueScheduleScreen::LeagueScheduleScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher,
    std::function<LoadedWorldContext()> loadedWorldContextProvider,
    std::function<void(const LoadedWorldContext&,std::int64_t)> openTeam,
    std::function<void(const WorldEditCommit&)> committed,
    std::function<void(std::unique_ptr<PffTest::GenericTestGame>,WorldActor)> openGame, std::int64_t teamSeasonId)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
{
    m_openTeam=std::move(openTeam);
    m_committed=std::move(committed);
    m_openGame=std::move(openGame);
    m_teamSeasonId = teamSeasonId;
    buildButtons();
}

LeagueScheduleScreen::~LeagueScheduleScreen()
{
}

void LeagueScheduleScreen::onEnter()
{
    m_root.resetFocus();
    // Read fixtures from the selected saved league, not the global Core template.
    loadSchedule();

    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_LeagueScheduleTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        m_teamSeasonId ? "TEAM SCHEDULE" : "LEAGUE SCHEDULE",
        PFF_Colors::ProFootballTitleColor);
}

void LeagueScheduleScreen::onExit()
{
    m_advancing=false;
    m_batchKickoff.clear();
    m_batchStopped=false;
    m_returningFromGame=false;
    m_scheduleCache.reset();
    m_LeagueScheduleTitleObject.clear();

    m_availableWeeks.clear();
    m_scheduleGames.clear();
    m_scheduleLoadMessage.clear();
    m_currentWeekIndex = 0;
    m_scheduleLoaded = false;
    m_gameTimeLabel.clear();
    m_scrollOffset = m_maxScrollOffset = 0;

    updateNavigationButtons();
}

bool LeagueScheduleScreen::handleEvent(const Input::Event& event)
{
    refreshLeagueContext();
    int scroll = 0;
    if (const auto* input=event.asCommand())
    {
        if (m_backgroundGame && (input->command.code == Input::CommandCode::Back ||
                                 input->command.code == Input::CommandCode::Cancel))
            return true;
        switch (input->command.code)
        {
        case Input::CommandCode::MoveDown: scroll=1; break;
        case Input::CommandCode::MoveUp: scroll=-1; break;
        case Input::CommandCode::PageDown: scroll=m_visibleRows; break;
        case Input::CommandCode::PageUp: scroll=-m_visibleRows; break;
        case Input::CommandCode::MoveHome: scroll=-m_maxScrollOffset; break;
        case Input::CommandCode::MoveEnd: scroll=m_maxScrollOffset; break;
        default: break;
        }
    }
    if (const auto* mouse=event.asMouse())
    {
        if (mouse->isWheel() && m_scheduleBounds.contains(mouse->position.x,mouse->position.y))
        {
            scroll=mouse->button==Input::MouseButton::WheelUp || mouse->wheelDelta>0 ? -3 : 3;
        }
    }
    if (scroll!=0)
    {
        const int offset=std::clamp(m_scrollOffset+scroll,0,m_maxScrollOffset);
        if (offset!=m_scrollOffset) m_scheduleCache.reset();
        m_scrollOffset=offset;
        return true;
    }
    return m_root.handleEvent(event);
}

void LeagueScheduleScreen::update(const Animation::TickEvent& event)
{
    refreshLeagueContext();
    if(!m_batchKickoff.empty() && !m_batchStopped) {runBatch();return;}
    if(m_advancing) continueGameWindow();
    m_root.update(event);
}

void LeagueScheduleScreen::draw(Surface& surface)
{
    refreshLeagueContext();
    ScreenBuffer& buffer = surface.buffer();
    if (m_scheduleCache &&
        m_scheduleCache->buffer().getWidth() == buffer.getWidth() &&
        m_scheduleCache->buffer().getHeight() == buffer.getHeight())
    {
        buffer = m_scheduleCache->buffer();
        m_root.draw(surface);
        return;
    }
    m_root.resetFocus();
    for(auto* link:m_teamLinks) m_root.removeChild(*link);
    m_teamLinks.clear();
    buffer.clear(PFF_Colors::MainBackgroundColor);

    PageComposer page(buffer);
    page.clearRegions();
    page.createFullScreenRegion("Screen");

    const Rect screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(afterHeader, 5);

    const int navigationWidth = std::min(34, std::max(28, body.size.width / 3));

    const auto [navigationPane, schedulePane] = page.splitRight(body, navigationWidth);
    m_scheduleBounds = schedulePane;

    m_root.setBounds(body);
    positionButtons(navigationPane, footer);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

    page.createRegion("Footer", footer);
    page.createRegion("Body", body);
    page.createRegion("SchedulePanel", schedulePane);
    page.createInsetRegion("ScheduleContent", "SchedulePanel", 2, 1, 2, 1);
    page.createRegion("NavigationPane", navigationPane);

    page.drawPanel(
        "Header",
        PFF_Colors::HeaderOuterColor,
        PFF_Colors::HeaderOuterColor,
        ObjectFactory::doubleLineBorder());

    buffer.fillRect(
        page.resolveRegion("InnerPanel"),
        U' ',
        PFF_Colors::HeaderInnerColor);

    page.writeObjectInRegion(
        m_LeagueScheduleTitleObject,
        "TitleArea",
        Composition::Align::center());

    page.drawPanel(
        "SchedulePanel",
        PFF_Colors::MainBackgroundColor,
        PFF_Colors::MainBorderColor,
        ObjectFactory::singleLineBorder());

    if (m_scheduleLoaded)
    {
        drawCurrentWeek(page, "SchedulePanel", "ScheduleContent");
    }
    else if (!m_scheduleLoadMessage.empty())
    {
        page.writeLine(
            "ScheduleContent",
            2,
            TextClip::clipUtf8Text(m_scheduleLoadMessage,page.resolveRegion("ScheduleContent").size.width),
            PFF_Colors::TextHighlightColor);
    }

    drawGameWindowStatus(page,navigationPane);
    drawFooter(page,footer);
    for(auto* button:{m_nextGameButton,m_lookAheadButton,m_lookBackButton,m_leaguePlayButton})
    {auto child=m_root.removeChild(*button);m_root.addChild(std::move(child));}
    // Save the static page before drawing interactive button states.
    m_scheduleCache = std::make_unique<Surface>();
    m_scheduleCache->buffer() = buffer;
    m_root.draw(surface);
}

void LeagueScheduleScreen::buildButtons()
{
    auto nextGameButton=std::make_unique<Button>(Rect{Point{0,0},Size{24,3}},m_teamSeasonId ? "Next Game" : "Next Game Window","schedule:next_game");
    nextGameButton->setActivationCallback([this](const ButtonActivationResult& result){handleButtonActivation(result);});
    m_nextGameButton=nextGameButton.get();
    m_root.addChild(std::move(nextGameButton));
    auto leaguePlayButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Return",
        "nav:back");

    auto lookAheadButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 28, 3 } },
        "Look Ahead",
        "schedule:look_ahead");

    auto lookBackButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 28, 3 } },
        "Look Back",
        "schedule:look_back");

    leaguePlayButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    lookAheadButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    lookBackButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_leaguePlayButton = leaguePlayButton.get();
    m_lookAheadButton = lookAheadButton.get();
    m_lookBackButton = lookBackButton.get();

    m_root.addChild(std::move(leaguePlayButton));
    m_root.addChild(std::move(lookAheadButton));
    m_root.addChild(std::move(lookBackButton));

    updateNavigationButtons();
}

void LeagueScheduleScreen::positionButtons(
    const Rect& navigationPane,
    const Rect& footer)
{
    // Keyboard-active buttons use the same visible highlight as mouse hover.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor;

    const Style& normal = PFF_Colors::ButtonNormalColor;
    const Style& hover = PFF_Colors::ButtonHoverColor;
    const Style& pressed = PFF_Colors::ButtonPressedColor;
    const Style disabled=normal.withDim(true); // Shared disabled button appearance.

    if (m_leaguePlayButton)
    {
        const Size buttonSize{ 28, 3 };

        m_leaguePlayButton->setBounds(Rect{
            Point{
                footer.position.x + ((footer.size.width - buttonSize.width) - 10),
                footer.position.y + ((footer.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_leaguePlayButton->setNormalStyle(normal);
        m_leaguePlayButton->setHoverStyle(hover);
        m_leaguePlayButton->setPressedStyle(pressed);
        m_leaguePlayButton->setFocusedStyle(ButtonFocusedStyle);
    }

    std::vector<Button*> buttons;
    for(auto* button:{m_nextGameButton,m_lookAheadButton,m_lookBackButton})
        if(button && button->isVisible()) buttons.push_back(button);
    const int height=navigationPane.size.height>=18?3:1;
    const int gap=navigationPane.size.height<12?0:1;
    const int total=static_cast<int>(buttons.size())*(height+gap)-gap;
    int y=navigationPane.position.y+std::max(0,(navigationPane.size.height-total-5)/2);
    const int statusRows=std::max(0,std::min(5,navigationPane.size.height-total-1));
    m_windowStatusBounds=Rect{Point{navigationPane.position.x+2,navigationPane.position.y+navigationPane.size.height-statusRows},Size{std::max(0,navigationPane.size.width-4),statusRows}};
    for(auto* button:buttons)
    {
        button->setBounds(Rect{Point{navigationPane.position.x+(navigationPane.size.width-24)/2,y},Size{24,height}});
        button->setNormalStyle(normal);button->setHoverStyle(hover);button->setPressedStyle(pressed);
        button->setFocusedStyle(ButtonFocusedStyle);button->setDisabledStyle(disabled);
        y+=height+gap;
    }
}

void LeagueScheduleScreen::handleButtonActivation(
    const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty())
    {
        return;
    }

    if(result.commandId=="schedule:next_game") {beginGameWindow();return;}
    if(result.commandId=="nav:back" && m_backgroundGame) return;

    if (result.commandId == "schedule:look_ahead")
    {
        if (m_scheduleLoaded &&
            m_currentWeekIndex + 1 < m_availableWeeks.size())
        {
            ++m_currentWeekIndex;
            m_scrollOffset = 0;
            m_scheduleCache.reset();
            updateNavigationButtons();
        }

        return;
    }

    if (result.commandId == "schedule:look_back")
    {
        if (m_scheduleLoaded && m_currentWeekIndex > 0)
        {
            --m_currentWeekIndex;
            m_scrollOffset = 0;
            m_scheduleCache.reset();
            updateNavigationButtons();
        }

        return;
    }

    if (m_commandDispatcher)
    {
        m_commandDispatcher(result.commandId);
    }
}

void LeagueScheduleScreen::refreshLeagueContext()
{
    const LoadedWorldContext context = m_loadedWorldContextProvider
        ? m_loadedWorldContextProvider() : LoadedWorldContext{};
    if (context.loaded != m_dataContext.loaded ||
        context.databasePath != m_dataContext.databasePath ||
        context.worldKey != m_dataContext.worldKey ||
        context.worldName != m_dataContext.worldName ||
        context.activeCompetitionSeasonId != m_dataContext.activeCompetitionSeasonId ||
        context.currentTimeUtc != m_dataContext.currentTimeUtc ||
        context.dataRevision != m_dataContext.dataRevision)
    {
        loadSchedule();
    }
}

void LeagueScheduleScreen::loadSchedule()
{
    m_scheduleCache.reset();
    const std::int32_t previousWeek = currentWeek();
    const LoadedWorldContext context = m_loadedWorldContextProvider
        ? m_loadedWorldContextProvider() : LoadedWorldContext{};
    const bool sameSeason = context.loaded && m_dataContext.loaded &&
        context.databasePath == m_dataContext.databasePath &&
        context.worldKey == m_dataContext.worldKey &&
        context.activeCompetitionSeasonId == m_dataContext.activeCompetitionSeasonId;
    m_dataContext = context;
    m_gameTimeLabel = context.isLoaded() ? PffGameTime::formatGameTimeEastern(context.currentTimeUtc) : "";
    if (!sameSeason) m_scrollOffset = 0;
    m_maxScrollOffset = 0;
    m_availableWeeks.clear();
    m_scheduleGames.clear();
    m_scheduleLoadMessage.clear();
    m_currentWeekIndex = 0;
    m_scheduleLoaded = false;
    m_gameWindowReady=false;
    m_advancing=false;
    m_controlsSelectedTeam=false;
    m_windowMessage.clear();
    m_gameWindow=WorldGameWindow{};

    if (!m_dataContext.hasActiveCompetitionSeason())
    {
        m_scheduleLoadMessage = "Load a league from the Commissioner's Office to view its schedule.";
        updateNavigationButtons();
        return;
    }

    WorldScheduleLoadResult result = loadWorldCompetitionSchedule(
        m_dataContext.databasePath, m_dataContext.activeCompetitionSeasonId);

    if (!result.succeeded())
    {
        m_scheduleLoadMessage = "Unable to load schedule from the loaded league";

        if (!result.errorMessage.empty())
        {
            m_scheduleLoadMessage += ": " + result.errorMessage;
        }

        updateNavigationButtons();
        return;
    }

    m_availableWeeks = std::move(result.regularSeasonWeeks);
    m_scheduleGames = std::move(result.games);
    std::stable_sort(m_scheduleGames.begin(),m_scheduleGames.end(),
        [](const WorldScheduleGameRecord& left,const WorldScheduleGameRecord& right)
        {
            if (left.scheduledKickoffUtc.empty()!=right.scheduledKickoffUtc.empty())
                return !left.scheduledKickoffUtc.empty();
            if (left.scheduledKickoffUtc!=right.scheduledKickoffUtc)
                return left.scheduledKickoffUtc<right.scheduledKickoffUtc;
            return left.scheduledGameId<right.scheduledGameId;
        });
    m_scheduleLoaded = m_teamSeasonId || !m_availableWeeks.empty();

    if (!m_scheduleLoaded)
    {
        m_scheduleLoadMessage =
            "The loaded league contains no regular-season schedule weeks.";
    }

    if (!m_teamSeasonId && !sameSeason && m_scheduleLoaded)
    {
        // A new view starts at the earliest unfinished regular-season week.
        auto firstUnfinished=m_availableWeeks.end();
        for (auto week=m_availableWeeks.begin();week!=m_availableWeeks.end();++week)
        {
            if (std::any_of(m_scheduleGames.begin(),m_scheduleGames.end(),[&](const auto& game)
                {return game.stage=="REGULAR" && game.weekNumber==*week && game.status!="FINAL" &&
                    game.status!="CANCELLED" && game.status!="CANCELED" && game.status!="VOID";}))
            {
                firstUnfinished=week;
                break;
            }
        }
        m_currentWeekIndex=firstUnfinished==m_availableWeeks.end() ? m_availableWeeks.size()-1 :
            static_cast<std::size_t>(firstUnfinished-m_availableWeeks.begin());
    }
    else if (sameSeason && previousWeek > 0)
    {
        const auto previous = std::find(m_availableWeeks.begin(), m_availableWeeks.end(), previousWeek);
        if (previous != m_availableWeeks.end())
        {
            m_currentWeekIndex = static_cast<std::size_t>(previous - m_availableWeeks.begin());
        }
    }

    loadGameWindow();
    updateNavigationButtons();
}

void LeagueScheduleScreen::updateNavigationButtons()
{
    if(m_nextGameButton) m_nextGameButton->setVisible(!m_teamSeasonId || m_controlsSelectedTeam);
    const bool targetAvailable = !m_teamSeasonId || !nextTeamKickoff().empty();
    const bool idle = m_batchKickoff.empty() || m_batchStopped;
    if(m_nextGameButton) m_nextGameButton->setEnabled(idle && bool(m_committed) && bool(m_openGame) &&
        (m_backgroundGame || (m_gameWindowReady && m_gameWindow.canStart && !m_advancing && targetAvailable)));
    if(m_leaguePlayButton) m_leaguePlayButton->setEnabled(!m_backgroundGame);
    const bool canLookBack =
        m_scheduleLoaded && m_currentWeekIndex > 0;

    const bool canLookAhead =
        m_scheduleLoaded &&
        m_currentWeekIndex + 1 < m_availableWeeks.size();

    if (m_lookBackButton)
    {
        m_lookBackButton->setVisible(!m_teamSeasonId && canLookBack);
    }

    if (m_lookAheadButton)
    {
        m_lookAheadButton->setVisible(!m_teamSeasonId && canLookAhead);
    }
}

void LeagueScheduleScreen::loadGameWindow()
{
    m_gameWindowReady=false;m_advancing=false;
    auto actor=openLocalWorldActor(m_dataContext.databasePath);
    if(!actor.success) {m_windowMessage="Unable to open game advancement: "+actor.message;return;}
    m_actor=actor.value;
    m_controlsSelectedTeam = m_teamSeasonId && controlsTeam(m_teamSeasonId);
    auto window=loadNextGameWindow(m_actor,m_dataContext.activeCompetitionSeasonId);
    if(!window.success) {m_windowMessage="Unable to load next game window: "+window.message;return;}
    m_gameWindow=std::move(window.value);
    if(m_gameWindow.worldId!=m_dataContext.worldId || m_gameWindow.worldKey!=m_dataContext.worldKey)
    {m_windowMessage="The loaded league changed. Reload the league before advancing.";return;}
    m_gameWindowReady=true;
    m_advancing=m_gameWindow.canResume;
    m_windowMessage=m_gameWindow.reason;
}

void LeagueScheduleScreen::publishAdvance(const WorldAdvanceOutcome& outcome)
{
    WorldEditCommit edit;
    edit.worldId=m_gameWindow.worldId;edit.worldKey=m_gameWindow.worldKey;
    edit.revision=outcome.revision;edit.currentTimeUtc=outcome.currentTimeUtc;
    edit.changed=outcome.revision!=m_dataContext.dataRevision;
    if(m_committed) m_committed(edit);
    loadSchedule();
}

bool LeagueScheduleScreen::controlsTeam(std::int64_t teamId) const
{
    const auto team = loadTeamData(m_actor, teamId);
    return team.success && std::any_of(team.value.staff.begin(), team.value.staff.end(),
        [](const WorldStaffPosition& role) { return role.userControlled; });
}

void LeagueScheduleScreen::openResult(std::int64_t gameId)
{
    try
    {
        auto game = std::make_unique<PffTest::GenericTestGame>(
            m_dataContext.databasePath, m_dataContext.activeCompetitionSeasonId);
        const auto result = game->viewFinal(m_actor, gameId);
        if (result.success)
        {
            const auto open = m_openGame;
            const auto actor = m_actor;
            m_root.clearPointerState();
            m_root.clearChildFocus();
            open(std::move(game), actor);
            return;
        }
        m_windowMessage = result.message;
    }
    catch (const std::exception& error)
    {
        m_windowMessage = error.what();
    }
    m_scheduleCache.reset();
}

std::string LeagueScheduleScreen::nextTeamKickoff() const
{
    if (m_gameWindow.kickoffUtc.empty()) return {};
    for (const auto& game : m_scheduleGames)
        if ((game.awayTeamSeasonId == m_teamSeasonId || game.homeTeamSeasonId == m_teamSeasonId) &&
            game.scheduledKickoffUtc >= m_gameWindow.kickoffUtc &&
            game.status != "FINAL" && game.status != "CANCELLED" &&
            game.status != "CANCELED" && game.status != "VOID")
            return game.scheduledKickoffUtc;
    return {};
}

void LeagueScheduleScreen::beginGameWindow()
{
    if (!m_committed || !m_openGame || (m_teamSeasonId && !m_controlsSelectedTeam)) return;
    if (m_backgroundGame)
    {
        m_batchStopped = false;
    }
    else
    {
        if (!m_gameWindowReady || !m_gameWindow.canStart || m_advancing) return;
        m_batchKickoff = m_teamSeasonId ? nextTeamKickoff() : m_gameWindow.kickoffUtc;
        m_batchStopped = false;
    }
    m_scheduleCache.reset();
    updateNavigationButtons();
}

void LeagueScheduleScreen::runBatch()
{
    const auto stop = [this](std::string message)
    {
        m_batchStopped = bool(m_backgroundGame);
        if (!m_batchStopped) m_batchKickoff.clear();
        if (!message.empty()) m_windowMessage = std::move(message);
        m_scheduleCache.reset();
        updateNavigationButtons();
    };
    try
    {
        if (m_returningFromGame)
        {
            m_returningFromGame = false;
            if (m_gameWindow.hasSelectedGame)
            {
                stop("Matchup saved. Select Next Game to resume.");
                return;
            }
        }
        if (!m_backgroundGame)
        {
            if (m_advancing && !m_gameWindow.canStart)
            {
                continueGameWindow();
                return;
            }
            if (m_gameWindow.kickoffUtc.empty() || m_gameWindow.kickoffUtc > m_batchKickoff ||
                !m_gameWindowReady || !m_gameWindow.canStart ||
                (m_teamSeasonId && !m_controlsSelectedTeam))
            {
                stop({});
                return;
            }
            auto game = std::make_unique<PffTest::GenericTestGame>(
                m_dataContext.databasePath, m_dataContext.activeCompetitionSeasonId);
            const auto previousRevision = m_gameWindow.revision;
            const auto started = game->start(m_actor, previousRevision);
            if (!started.success)
            {
                loadGameWindow();
                if (m_gameWindowReady && m_gameWindow.revision != previousRevision)
                {
                    WorldAdvanceOutcome outcome;
                    outcome.revision = m_gameWindow.revision;
                    outcome.currentTimeUtc = m_gameWindow.currentTimeUtc;
                    publishAdvance(outcome);
                }
                if (m_gameWindowReady && m_advancing) return;
                stop("Unable to start game: " + started.message);
                return;
            }
            m_committed(started.value.commit);
            const bool show = started.value.kickoffUtc == m_batchKickoff &&
                (controlsTeam(started.value.awayId) || controlsTeam(started.value.homeId));
            if (show)
            {
                m_returningFromGame = true;
                const auto open = m_openGame;
                const auto actor = m_actor;
                m_root.clearPointerState();
                m_root.clearChildFocus();
                open(std::move(game), actor);
                return;
            }
            m_backgroundGame = std::move(game);
            updateNavigationButtons();
        }
        // Keep the screen responsive while the shared executor runs a background matchup.
        for (int step = 0; step < 32 && !m_backgroundGame->state().final; ++step)
        {
            if (m_backgroundGame->interruption())
            {
                const auto reason = m_backgroundGame->interruption()->reason;
                m_backgroundGame.reset();
                loadSchedule();
                stop(reason);
                return;
            }
            const auto advanced = m_backgroundGame->next(m_actor);
            if (!advanced.success)
            {
                stop("Unable to simulate game: " + advanced.message);
                return;
            }
            if (advanced.value.changed) m_committed(advanced.value);
        }
        if (m_backgroundGame->state().final)
        {
            m_backgroundGame.reset();
            loadSchedule();
        }
    }
    catch (const std::exception& error)
    {
        stop("Unable to simulate game: " + std::string(error.what()));
    }
}

void LeagueScheduleScreen::continueGameWindow()
{
    if(!m_gameWindow.advance) {m_advancing=false;return;}
    auto result=continueWorldAdvance(m_actor,m_gameWindow.advance->requestId,m_gameWindow.revision,1);
    if(result.success) publishAdvance(result.value);
    else {m_advancing=false;m_windowMessage="Advancement stopped: "+result.message;m_scheduleCache.reset();updateNavigationButtons();}
}

void LeagueScheduleScreen::drawGameWindowStatus(Composition::PageComposer& page,const Rect& pane)
{
    const Style& statusStyle=PFF_Colors::TextHighlightColor; // Window timing and stop explanation.
    std::string message;
    if(!m_batchKickoff.empty() && !m_batchStopped) message="Simulating games...";
    else if(m_advancing) message="Advancing to game window...";
    else if(m_windowMessage=="UNSUPPORTED_PROVIDER") message="Game executor not installed. Games cannot start yet.";
    else if(m_windowMessage=="UNSUPPORTED_CODEC") message="Current games need a supported game executor before continuing.";
    else if(m_windowMessage=="REQUIRED_DECISION") message="Game participation needs your attention before continuing.";
    else if(m_windowMessage=="WAITING_FOR_WORLD_WORK") message="An earlier event needs attention before continuing.";
    else if(m_windowMessage=="CURRENT_WINDOW_UNFINISHED" || m_windowMessage=="EARLIER_GAME_UNRESOLVED") message="Finish all games in the current window before advancing.";
    else if(m_windowMessage=="NO_MORE_GAMES") message="No more scheduled game windows.";
    else if(m_windowMessage=="NOT_AUTHORIZED") message="You do not have permission to advance this league.";
    else if(m_windowMessage=="ADVANCE_ALREADY_ACTIVE") message="Another advancement is active. Resolve it before starting a game window.";
    else if(m_windowMessage=="DISPATCH_FAILED") message="An earlier event could not complete. Resolve the problem before continuing.";
    else if(m_windowMessage=="STALE_CHOICE") message="Game participation changed. Review team control before continuing.";
    else if(m_windowMessage=="CLOCK_BARRIER") message="Unresolved earlier game or event prevents advancement.";
    else message=m_windowMessage;
    const Rect area=m_windowStatusBounds;
    if(area.size.width<=0 || area.size.height<=0) return;
    if(message.empty() && !m_gameWindow.kickoffUtc.empty())
        message="Next window: "+PffGameTime::formatGameTimeEastern(m_gameWindow.kickoffUtc);
    const auto lines=wrapText(message,area.size.width);
    for(int row=0;row<std::min(area.size.height,static_cast<int>(lines.size()));++row)
        page.writeLine(area,row,lines[static_cast<std::size_t>(row)],statusStyle);
}

void LeagueScheduleScreen::drawCurrentWeek(
    Composition::PageComposer& page,
    std::string_view panelRegionName,
    std::string_view contentRegionName)
{
    const std::int32_t week = currentWeek();

    if (!m_teamSeasonId && week <= 0)
    {
        return;
    }

    const std::string weekTitle =
        m_teamSeasonId ? "SCHEDULE" : "WEEK " + std::to_string(week);

    page.writePanelTitle(
        panelRegionName,
        weekTitle,
        PFF_Colors::TextHighlightColor,
        3,
        1);

    const Rect content=page.resolveRegion(contentRegionName);
    if (content.size.width<102 || content.size.height<7)
    {
        page.writeLine(content,2,TextClip::clipUtf8Text(
            "Enlarge the window to show all five schedule columns.",content.size.width),
            PFF_Colors::TextHighlightColor);
        m_maxScrollOffset=0;
        return;
    }
    const auto displayed = [&](const WorldScheduleGameRecord& game)
    {
        return m_teamSeasonId ? game.awayTeamSeasonId == m_teamSeasonId || game.homeTeamSeasonId == m_teamSeasonId
                              : game.stage == "REGULAR" && game.weekNumber == week;
    };
    int matchupWidth=24;
    for(const auto& game:m_scheduleGames)
        if(displayed(game))
            matchupWidth=std::max(matchupWidth,TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(game.matchupDisplayName())));
    // Keep the full matchup on one line. Other columns may wrap within their
    // own cells; reserve enough space for both complete, independently linked names.
    const int requiredWidth=matchupWidth+64;
    if(content.size.width<requiredWidth)
    {
        page.writeLine(content,2,TextClip::clipUtf8Text("Enlarge the window to show all five schedule columns.",content.size.width),PFF_Colors::TextHighlightColor);
        m_maxScrollOffset=0;
        return;
    }
    const int extra=content.size.width-requiredWidth;
    const std::array<int,5> widths{{matchupWidth,15,11,12+extra/2,18+extra-extra/2}};
    const std::array<std::string,5> headings{{"Matchup","Day and Date","Time","Location","Result"}};
    std::vector<std::string> rows;
    std::vector<const WorldScheduleGameRecord*> linkRows;
    auto appendRow=[&](const std::array<std::string,5>& values,const WorldScheduleGameRecord* game=nullptr)
    {
        std::array<std::vector<std::string>,5> wrapped;
        std::size_t height=1;
        for (std::size_t column=0;column<5;++column)
        {
            wrapped[column]=column==0 ? std::vector<std::string>{values[column]} : wrapText(values[column],widths[column]);
            height=std::max(height,wrapped[column].size());
        }
        for (std::size_t line=0;line<height;++line)
        {
            std::string text;
            for (std::size_t column=0;column<5;++column)
            {
                const std::string cell=line<wrapped[column].size()?wrapped[column][line]:"";
                text+=cell;
                const int used=TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(cell));
                text.append(static_cast<std::size_t>(std::max(0,widths[column]-used)),' ');
                if (column<4) text+="  ";
            }
            rows.push_back(std::move(text));
            linkRows.push_back(line==0?game:nullptr);
        }
    };
    std::string previousGroup;
    bool first=true;

    for (const WorldScheduleGameRecord& game : m_scheduleGames)
    {
        if (!displayed(game))
        {
            continue;
        }

        const std::string group=game.scheduledKickoffUtc.empty()
            ? "unknown:"+game.gameDate+" "+game.timeLabel : game.scheduledKickoffUtc;
        if (first || (!m_teamSeasonId && group!=previousGroup))
        {
            if (!first) {rows.emplace_back();linkRows.push_back(nullptr);}
            appendRow(headings);
            previousGroup=group;
            first=false;
        }
        const auto kickoff=PffGameTime::easternTime(game.scheduledKickoffUtc);
        const std::string date=kickoff ? PffGameTime::dayAndDate(*kickoff) :
            (game.gameDate.empty()?"Date unavailable":game.dayLabel+" "+game.gameDate);
        const std::string time=kickoff ? PffGameTime::clock(*kickoff) :
            (game.timeLabel.empty()?"Unavailable":game.timeLabel);
        appendRow({{game.matchupDisplayName(),date,time,
            game.venueName.empty()?"Venue unavailable":game.venueName,displayResult(game)}},&game);
    }

    m_visibleRows=std::max(1,content.size.height-4);
    m_maxScrollOffset=std::max(0,static_cast<int>(rows.size())-m_visibleRows);
    m_scrollOffset=std::clamp(m_scrollOffset,0,m_maxScrollOffset);
    for (int row=0;row<m_visibleRows && m_scrollOffset+row<static_cast<int>(rows.size());++row)
        page.writeLine(content,2+row,rows[static_cast<std::size_t>(m_scrollOffset+row)],PFF_Colors::TextHighlightColor);
    for(int row=0;row<m_visibleRows && m_scrollOffset+row<static_cast<int>(linkRows.size());++row)
    {
        const auto* game=linkRows[static_cast<std::size_t>(m_scrollOffset+row)];
        if(!game) continue;
        int x=content.position.x;
        for(bool home:{false,true})
        {
            const auto& name=home?game->homeDisplayName:game->awayDisplayName;
            const auto id=home?game->homeTeamSeasonId:game->awayTeamSeasonId;
            const int width=TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(name));
            if(width>0 && id!=m_teamSeasonId)
            {
                const auto open=m_openTeam;const auto world=m_dataContext;
                auto link=std::make_unique<TeamLink>(std::vector<TeamLink::Span>{{Rect{Point{x,content.position.y+2+row},Size{width,1}},name}},
                    [this,open,world,id] {m_root.clearPointerState();m_root.clearChildFocus();if(open) open(world,id);});
                m_teamLinks.push_back(link.get());m_root.addChild(std::move(link));
            }
            x+=width+4; // The unlinked separator is exactly " at ".
        }
        if (m_openGame && game->status == "FINAL" && game->gameId)
        {
            const auto label = wrapText(displayResult(*game), widths[4]).front();
            const int resultX = content.position.x + widths[0] + widths[1] + widths[2] + widths[3] + 8;
            const int width = TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(label));
            auto link = std::make_unique<TeamLink>(
                std::vector<TeamLink::Span>{{Rect{Point{resultX,content.position.y+2+row},Size{width,1}},label}},
                [this,id=game->gameId] { openResult(id); });
            m_teamLinks.push_back(link.get());
            m_root.addChild(std::move(link));
        }
    }
    if (m_maxScrollOffset>0)
        page.writeLine(content,content.size.height-1,TextClip::clipUtf8Text(
            "Scroll: mouse wheel / arrows / Page Up / Page Down",content.size.width),PFF_Colors::TextHighlightColor);
}

void LeagueScheduleScreen::drawFooter(PageComposer& page,const Rect& footer) const
{
    const Rect textArea{Point{footer.position.x+2,footer.position.y+1},
        Size{std::max(0,footer.size.width-42),2}};
    page.writeLine(textArea,0,TextClip::clipUtf8Text(
        m_dataContext.isLoaded()?m_dataContext.worldName:"No league loaded",textArea.size.width),
        PFF_Colors::TextHighlightColor);
    page.writeLine(textArea,1,TextClip::clipUtf8Text(m_gameTimeLabel,textArea.size.width),PFF_Colors::TextHighlightColor);
}

std::int32_t LeagueScheduleScreen::currentWeek() const noexcept
{
    if (!m_scheduleLoaded ||
        m_currentWeekIndex >= m_availableWeeks.size())
    {
        return 0;
    }

    return m_availableWeeks[m_currentWeekIndex];
}

