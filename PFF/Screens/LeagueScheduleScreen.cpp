// PFF includes
#include "Screens/LeagueScheduleScreen.h"
#include "Styles/PFF_Theme.h"

// TUI includes
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"

// C++ library includes
#include <algorithm>
#include <utility>

namespace
{
    using Composition::PageComposer;
}

LeagueScheduleScreen::LeagueScheduleScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher,
    std::function<LoadedWorldContext()> loadedWorldContextProvider)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
{
    buildButtons();
}

LeagueScheduleScreen::~LeagueScheduleScreen()
{
}

void LeagueScheduleScreen::onEnter()
{
    // Read fixtures from the selected saved league, not the global Core template.
    loadSchedule();

    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_LeagueScheduleTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "LEAGUE SCHEDULE",
        PFF_Colors::ProFootballTitleColor);
}

void LeagueScheduleScreen::onExit()
{
    m_LeagueScheduleTitleObject.clear();

    m_availableWeeks.clear();
    m_scheduleGames.clear();
    m_scheduleLoadMessage.clear();
    m_currentWeekIndex = 0;
    m_scheduleLoaded = false;

    updateNavigationButtons();
}

bool LeagueScheduleScreen::handleEvent(const Input::Event& event)
{
    refreshLeagueContext();
    return m_root.handleEvent(event);
}

void LeagueScheduleScreen::update(const Animation::TickEvent& event)
{
    refreshLeagueContext();
    m_root.update(event);
}

void LeagueScheduleScreen::draw(Surface& surface)
{
    refreshLeagueContext();
    ScreenBuffer& buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);

    PageComposer page(buffer);
    page.clearRegions();
    page.createFullScreenRegion("Screen");

    const Rect screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(afterHeader, 5);

    const int navigationWidth = std::min(34, std::max(28, body.size.width / 3));

    const auto [navigationPane, schedulePane] = page.splitRight(body, navigationWidth);

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
            m_scheduleLoadMessage,
            PFF_Colors::TextHighlightColor);
    }

    m_root.draw(surface);
}

void LeagueScheduleScreen::buildButtons()
{
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
    const Style& normal = PFF_Colors::ButtonNormalColor;
    const Style& hover = PFF_Colors::ButtonHoverColor;
    const Style& pressed = PFF_Colors::ButtonPressedColor;

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
        m_leaguePlayButton->setFocusedStyle(normal);
    }

    const Size navigationButtonSize{ 24, 3 };
    const int navigationX =
        navigationPane.position.x +
        ((navigationPane.size.width - navigationButtonSize.width) / 2);

    const int firstButtonY =
        navigationPane.position.y +
        std::max(2, (navigationPane.size.height / 2) - 4);

    if (m_lookAheadButton)
    {
        m_lookAheadButton->setBounds(Rect{
            Point{ navigationX, firstButtonY },
            navigationButtonSize
            });

        m_lookAheadButton->setNormalStyle(normal);
        m_lookAheadButton->setHoverStyle(hover);
        m_lookAheadButton->setPressedStyle(pressed);
        m_lookAheadButton->setFocusedStyle(normal);
    }

    if (m_lookBackButton)
    {
        m_lookBackButton->setBounds(Rect{
            Point{ navigationX, firstButtonY + 5 },
            navigationButtonSize
            });

        m_lookBackButton->setNormalStyle(normal);
        m_lookBackButton->setHoverStyle(hover);
        m_lookBackButton->setPressedStyle(pressed);
        m_lookBackButton->setFocusedStyle(normal);
    }
}

void LeagueScheduleScreen::handleButtonActivation(
    const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty())
    {
        return;
    }

    if (result.commandId == "schedule:look_ahead")
    {
        if (m_scheduleLoaded &&
            m_currentWeekIndex + 1 < m_availableWeeks.size())
        {
            ++m_currentWeekIndex;
            updateNavigationButtons();
        }

        return;
    }

    if (result.commandId == "schedule:look_back")
    {
        if (m_scheduleLoaded && m_currentWeekIndex > 0)
        {
            --m_currentWeekIndex;
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
        context.currentTimeUtc != m_dataContext.currentTimeUtc)
    {
        loadSchedule();
    }
}

void LeagueScheduleScreen::loadSchedule()
{
    const std::int32_t previousWeek = currentWeek();
    const LoadedWorldContext context = m_loadedWorldContextProvider
        ? m_loadedWorldContextProvider() : LoadedWorldContext{};
    const bool sameSeason = context.loaded && m_dataContext.loaded &&
        context.databasePath == m_dataContext.databasePath &&
        context.worldKey == m_dataContext.worldKey &&
        context.activeCompetitionSeasonId == m_dataContext.activeCompetitionSeasonId;
    m_dataContext = context;
    m_availableWeeks.clear();
    m_scheduleGames.clear();
    m_scheduleLoadMessage.clear();
    m_currentWeekIndex = 0;
    m_scheduleLoaded = false;

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
    m_scheduleLoaded = !m_availableWeeks.empty();

    if (!m_scheduleLoaded)
    {
        m_scheduleLoadMessage =
            "The loaded league contains no regular-season schedule weeks.";
    }

    if (sameSeason && previousWeek > 0)
    {
        const auto previous = std::find(m_availableWeeks.begin(), m_availableWeeks.end(), previousWeek);
        if (previous != m_availableWeeks.end())
        {
            m_currentWeekIndex = static_cast<std::size_t>(previous - m_availableWeeks.begin());
        }
    }

    updateNavigationButtons();
}

void LeagueScheduleScreen::updateNavigationButtons()
{
    const bool canLookBack =
        m_scheduleLoaded && m_currentWeekIndex > 0;

    const bool canLookAhead =
        m_scheduleLoaded &&
        m_currentWeekIndex + 1 < m_availableWeeks.size();

    if (m_lookBackButton)
    {
        m_lookBackButton->setVisible(canLookBack);
    }

    if (m_lookAheadButton)
    {
        m_lookAheadButton->setVisible(canLookAhead);
    }
}

void LeagueScheduleScreen::drawCurrentWeek(
    Composition::PageComposer& page,
    std::string_view panelRegionName,
    std::string_view contentRegionName) const
{
    const std::int32_t week = currentWeek();

    if (week <= 0)
    {
        return;
    }

    const std::string weekTitle =
        "WEEK " + std::to_string(week) + " - " + m_dataContext.worldName;

    page.writePanelTitle(
        panelRegionName,
        weekTitle,
        PFF_Colors::TextHighlightColor,
        3,
        1);

    int row = 3;

    for (const WorldScheduleGameRecord& game : m_scheduleGames)
    {
        if (game.stage != "REGULAR" || game.weekNumber != week)
        {
            continue;
        }

        page.writeLine(
            contentRegionName,
            row,
            game.matchupDisplayName(),
            PFF_Colors::TextHighlightColor);

        row += 2;
    }
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

