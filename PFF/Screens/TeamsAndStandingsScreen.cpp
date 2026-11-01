// PFF includes
#include "Screens/TeamsAndStandingsScreen.h"
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

TeamsAndStandingsScreen::TeamsAndStandingsScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher,
    std::function<LoadedWorldContext()> loadedWorldContextProvider)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
{
    buildButtons();
}

TeamsAndStandingsScreen::~TeamsAndStandingsScreen()
{
}

void TeamsAndStandingsScreen::onEnter()
{
    // Read mutable team identity and membership from the selected saved league.
    loadTeams();

    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_TeamsAndStandingsTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "TEAMS & STANDINGS",
        PFF_Colors::ProFootballTitleColor);
}

void TeamsAndStandingsScreen::onExit()
{
    m_TeamsAndStandingsTitleObject.clear();
    m_teams.clear();
    m_teamLoadMessage.clear();
    m_teamsLoaded = false;
}

bool TeamsAndStandingsScreen::handleEvent(const Input::Event& event)
{
    refreshLeagueContext();
    return m_root.handleEvent(event);
}

void TeamsAndStandingsScreen::update(const Animation::TickEvent& event)
{
    refreshLeagueContext();
    m_root.update(event);
}

void TeamsAndStandingsScreen::draw(Surface& surface)
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
    const auto [afcPane, nfcPane] = page.splitLeft(body, body.size.width / 2);

    const int divisionHeight = afcPane.size.height / 3;

    const Rect afcEast = Rect
    {
        Point{ afcPane.position.x, afcPane.position.y },
        Size{ afcPane.size.width, divisionHeight }
    };

    const Rect afcCent = Rect
    {
        Point{ afcPane.position.x, afcEast.position.y + afcEast.size.height },
        Size{ afcPane.size.width, divisionHeight }
    };

    const Rect afcWest = Rect
    {
        Point{ afcPane.position.x, afcCent.position.y + afcCent.size.height },
        Size{ afcPane.size.width, divisionHeight }
    };

    const Rect nfcEast = Rect
    {
        Point{ nfcPane.position.x, nfcPane.position.y },
        Size{ nfcPane.size.width, divisionHeight }
    };

    const Rect nfcCent = Rect
    {
        Point{ nfcPane.position.x, nfcEast.position.y + nfcEast.size.height },
        Size{ nfcPane.size.width, divisionHeight }
    };

    const Rect nfcWest = Rect
    {
        Point{ nfcPane.position.x, nfcCent.position.y + nfcCent.size.height },
        Size{ nfcPane.size.width, divisionHeight }
    };

    m_root.setBounds(body);
    positionButtons(footer);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

    page.createRegion("Footer", footer);
    page.createRegion("Body", body);
    page.createRegion("AFCEast", afcEast);
    page.createRegion("AFCCent", afcCent);
    page.createRegion("AFCWest", afcWest);
    page.createRegion("NFCEast", nfcEast);
    page.createRegion("NFCCent", nfcCent);
    page.createRegion("NFCWest", nfcWest);

    page.createInsetRegion("AFCEastContent", "AFCEast", 1, 1, 1, 1);
    page.createInsetRegion("AFCCentContent", "AFCCent", 1, 1, 1, 1);
    page.createInsetRegion("AFCWestContent", "AFCWest", 1, 1, 1, 1);
    page.createInsetRegion("NFCEastContent", "NFCEast", 1, 1, 1, 1);
    page.createInsetRegion("NFCCentContent", "NFCCent", 1, 1, 1, 1);
    page.createInsetRegion("NFCWestContent", "NFCWest", 1, 1, 1, 1);


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
        m_TeamsAndStandingsTitleObject,
        "TitleArea",
        Composition::Align::center());

    if (m_teamsLoaded)
    {
        page.drawPanel("AFCEast", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
        page.drawPanel("AFCCent", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
        page.drawPanel("AFCWest", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
        page.drawPanel("NFCEast", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
        page.drawPanel("NFCCent", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
        page.drawPanel("NFCWest", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());

        drawDivisionTeams(page, "AFCEast", "AFCEastContent", "AFC", "East");
        drawDivisionTeams(page, "AFCCent", "AFCCentContent", "AFC", "Central");
        drawDivisionTeams(page, "AFCWest", "AFCWestContent", "AFC", "West");
        drawDivisionTeams(page, "NFCEast", "NFCEastContent", "NFC", "East");
        drawDivisionTeams(page, "NFCCent", "NFCCentContent", "NFC", "Central");
        drawDivisionTeams(page, "NFCWest", "NFCWestContent", "NFC", "West");
    }
    else
    {
        page.writeLine(
            body,
            2,
            "  " + m_teamLoadMessage,
            PFF_Colors::TextHighlightColor);
    }

    m_root.draw(surface);
}

void TeamsAndStandingsScreen::buildButtons()
{
    auto leaguePlayButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Return",
        "nav:back");

    leaguePlayButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_leaguePlayButton = leaguePlayButton.get();
    m_root.addChild(std::move(leaguePlayButton));
}

void TeamsAndStandingsScreen::positionButtons(const Rect& footer)
{
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

        m_leaguePlayButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_leaguePlayButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_leaguePlayButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_leaguePlayButton->setFocusedStyle(PFF_Colors::ButtonNormalColor);
    }
}

void TeamsAndStandingsScreen::handleButtonActivation(
    const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}

void TeamsAndStandingsScreen::refreshLeagueContext()
{
    const LoadedWorldContext context = m_loadedWorldContextProvider
        ? m_loadedWorldContextProvider() : LoadedWorldContext{};
    if (context.loaded != m_dataContext.loaded ||
        context.databasePath != m_dataContext.databasePath ||
        context.worldKey != m_dataContext.worldKey ||
        context.activeCompetitionSeasonId != m_dataContext.activeCompetitionSeasonId ||
        context.currentTimeUtc != m_dataContext.currentTimeUtc)
    {
        loadTeams();
    }
}

void TeamsAndStandingsScreen::loadTeams()
{
    m_teams.clear();
    m_teamLoadMessage.clear();
    m_teamsLoaded = false;

    m_dataContext = m_loadedWorldContextProvider
        ? m_loadedWorldContextProvider() : LoadedWorldContext{};

    if (!m_dataContext.hasActiveCompetitionSeason())
    {
        m_teamLoadMessage = "Load a league from the Commissioner's Office to view teams.";
        return;
    }

    WorldTeamsLoadResult result = loadWorldTeams(
        m_dataContext.databasePath, m_dataContext.activeCompetitionSeasonId);

    if (!result.succeeded())
    {
        m_teamLoadMessage = "Unable to load teams from the loaded league";

        if (!result.errorMessage.empty())
        {
            m_teamLoadMessage += ": " + result.errorMessage;
        }

        return;
    }

    m_teams = std::move(result.teams);
    m_teamsLoaded = true;
}

void TeamsAndStandingsScreen::drawDivisionTeams(
    Composition::PageComposer& page,
    std::string_view regionName,
    std::string_view regionContent,
    std::string_view conference,
    std::string_view division) const
{
    const std::string title =
        std::string(conference) + " " + std::string(division);

    page.writePanelTitle(
        regionName,
        title,
        PFF_Colors::TextHighlightColor,
        2,
        1);

    int row = 3;

    for (const WorldTeamRecord& team : m_teams)
    {
        if (team.conference != conference || team.division != division)
        {
            continue;
        }

        page.writeLine(
            regionContent,
            row,
            "  " + team.fallbackDisplayName(),
            PFF_Colors::TextHighlightColor);

        ++row;
    }
}

