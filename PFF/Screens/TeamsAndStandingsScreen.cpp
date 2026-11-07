// PFF includes
#include "Screens/TeamsAndStandingsScreen.h"
#include "Screens/TeamLink.h"
#include "Styles/PFF_Theme.h"
#include "Screens/GameTimeFormatting.h"

// TUI includes
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Input/Event.h"
#include "Utilities/Text/TextClip.h"

// C++ library includes
#include <algorithm>
#include <array>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <sstream>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace
{
    using Composition::PageComposer;

    struct StandingColumn
    {
        const char* heading;
        const char* fullName;
        int width;
    };

    const std::array<StandingColumn, 9> StandingColumns{{
        { "Team", "Team Name", 0 },
        { "W", "Wins", 3 },
        { "L", "Losses", 3 },
        { "T", "Ties", 3 },
        { "W%", "Win Percentage", 5 },
        { "PF", "Points For", 4 },
        { "PA", "Points Against", 4 },
        { "NP", "Net Points", 5 },
        { "ST", "Streak", 4 }
    }};

    using PffGameTime::formatGameTimeEastern;

    std::string formatWinPercentage(double value)
    {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << std::fixed << std::setprecision(3) << value;
        std::string result = text.str();
        if (result.front() == '0') result.erase(0, 1);
        return result;
    }
}

TeamsAndStandingsScreen::TeamsAndStandingsScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher,
    std::function<LoadedWorldContext()> loadedWorldContextProvider,
    std::function<void(const LoadedWorldContext&,std::int64_t)> openTeam)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
{
    m_openTeam=std::move(openTeam);
    buildButtons();
}

TeamsAndStandingsScreen::~TeamsAndStandingsScreen()
{
}

void TeamsAndStandingsScreen::onEnter()
{
    m_root.resetFocus();
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
    m_headingTooltips.clear();
    m_mousePosition = Point{ -1, -1 };
    m_standingsCache.reset();
}

bool TeamsAndStandingsScreen::handleEvent(const Input::Event& event)
{
    if (const auto* mouse = event.asMouse())
    {
        m_mousePosition = mouse->position;
    }
    else if (event.asCommand() || event.type() == Input::EventType::Resize)
    {
        m_mousePosition = Point{ -1, -1 };
    }
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
    if (m_standingsCache &&
        m_standingsCache->buffer().getWidth() == buffer.getWidth() &&
        m_standingsCache->buffer().getHeight() == buffer.getHeight())
    {
        buffer = m_standingsCache->buffer();
        m_root.draw(surface);
        drawHeadingTooltip(surface);
        return;
    }
    m_headingTooltips.clear();
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

    if (screen.size.width < 112 || body.size.height < 30)
    {
        page.writeLine(body, 2, TextClip::clipUtf8Text(
            "Enlarge the window to at least 112 columns by 47 rows to view standings.",
            body.size.width), PFF_Colors::TextHighlightColor);
    }
    else if (m_teamsLoaded)
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

    drawFooter(page, footer);
    auto returnButton=m_root.removeChild(*m_leaguePlayButton);
    m_root.addChild(std::move(returnButton));
    // Cache before drawing interactive elements so hover, press and tooltip
    // changes cannot become baked into the next frame.
    m_standingsCache = std::make_unique<Surface>();
    m_standingsCache->buffer() = buffer;
    m_root.draw(surface);
    drawHeadingTooltip(surface);
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
    // Keyboard-active buttons use the same visible highlight as mouse hover.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor;

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
        m_leaguePlayButton->setFocusedStyle(ButtonFocusedStyle);
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
        context.worldName != m_dataContext.worldName ||
        context.activeCompetitionSeasonId != m_dataContext.activeCompetitionSeasonId ||
        context.currentTimeUtc != m_dataContext.currentTimeUtc ||
        context.dataRevision != m_dataContext.dataRevision)
    {
        loadTeams();
    }
}

void TeamsAndStandingsScreen::loadTeams()
{
    m_standingsCache.reset();
    m_teams.clear();
    m_teamLoadMessage.clear();
    m_teamsLoaded = false;
    m_headingTooltips.clear();
    m_gameTimeLabel.clear();

    m_dataContext = m_loadedWorldContextProvider
        ? m_loadedWorldContextProvider() : LoadedWorldContext{};
    if (m_dataContext.isLoaded())
    {
        m_gameTimeLabel = formatGameTimeEastern(m_dataContext.currentTimeUtc);
    }

    if (!m_dataContext.hasActiveCompetitionSeason())
    {
        m_teamLoadMessage = "Load a league from the Commissioner's Office to view teams.";
        return;
    }

    PffStandingsResult result = loadPffStandings(
        m_dataContext.databasePath, m_dataContext.activeCompetitionSeasonId);

    if (!result.success)
    {
        m_teamLoadMessage = "Unable to load standings from the loaded league";

        if (!result.errorMessage.empty())
        {
            m_teamLoadMessage += ": " + result.errorMessage;
        }

        return;
    }

    m_teams = std::move(result.rows);
    // Each division panel filters this display order to its own teams.
    std::stable_sort(m_teams.begin(), m_teams.end(),
        [](const PffStandingRow& left, const PffStandingRow& right)
        {
            const auto leftScore = 2 * left.wins + left.ties;
            const auto rightScore = 2 * right.wins + right.ties;
            if (leftScore != rightScore) return leftScore > rightScore;
            if (left.pointsFor != right.pointsFor) return left.pointsFor > right.pointsFor;
            return left.team.fallbackDisplayName() < right.team.fallbackDisplayName();
        });
    m_teamsLoaded = true;
}

void TeamsAndStandingsScreen::drawDivisionTeams(
    Composition::PageComposer& page,
    std::string_view regionName,
    std::string_view regionContent,
    std::string_view conference,
    std::string_view division)
{
    const std::string title =
        std::string(conference) + " " + std::string(division);

    page.writePanelTitle(
        regionName,
        title,
        PFF_Colors::TextHighlightColor,
        2,
        1);

    const Rect content = page.resolveRegion(regionContent);
    // Eight numeric widths plus eight single-cell gaps consume 39 cells.
    const int teamWidth = std::min(30, std::max(1, content.size.width - 39));
    int x = content.position.x;
    for (std::size_t column = 0; column < StandingColumns.size(); ++column)
    {
        const auto& definition = StandingColumns[column];
        const int width = column == 0 ? teamWidth : definition.width;
        const std::string heading = definition.heading;
        const int headingX = column == 0 ? x : x + width - static_cast<int>(heading.size());
        page.writeLine(Rect{ Point{ headingX, content.position.y + 2 }, Size{ width, 1 } },
            0, heading, PFF_Colors::TextHighlightColor);
        m_headingTooltips.push_back({ Rect{ Point{ headingX, content.position.y + 2 },
            Size{ static_cast<int>(heading.size()), 1 } }, definition.fullName });
        x += width + 1;
    }

    int row = 3;
    for (const PffStandingRow& standing : m_teams)
    {
        const auto& team = standing.team;
        if (team.conference != conference || team.division != division)
        {
            continue;
        }

        if (row >= content.size.height) break;
        const std::array<std::string, 9> values{{
            team.fallbackDisplayName(),
            std::to_string(standing.wins), std::to_string(standing.losses),
            std::to_string(standing.ties), formatWinPercentage(standing.winPercentage()),
            std::to_string(standing.pointsFor), std::to_string(standing.pointsAgainst),
            std::to_string(standing.pointsFor - standing.pointsAgainst),
            standing.streakLength == 0 ? "\xE2\x80\x94" :
                std::string(1, standing.streakResult) + std::to_string(standing.streakLength)
        }};
        x = content.position.x;
        for (std::size_t column = 0; column < values.size(); ++column)
        {
            const int width = column == 0 ? teamWidth : StandingColumns[column].width;
            std::string value = TextClip::clipUtf8Text(values[column], width);
            if (column != 0)
            {
                const int displayWidth = TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(value));
                value.insert(0, static_cast<std::size_t>(std::max(0, width - displayWidth)), ' ');
            }
            if(column==0 && !value.empty())
            {
                const auto id=team.teamSeasonId;const auto world=m_dataContext;const auto open=m_openTeam;
                const int length=TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(value));
                auto link=std::make_unique<TeamLink>(std::vector<TeamLink::Span>{{Rect{Point{x,content.position.y+row},Size{length,1}},value}},
                    [this,open,world,id] {m_root.clearPointerState();m_root.clearChildFocus();if(open) open(world,id);});
                m_teamLinks.push_back(link.get());m_root.addChild(std::move(link));
            }
            page.writeLine(Rect{ Point{ x, content.position.y + row }, Size{ width, 1 } },
                0, value, PFF_Colors::TextHighlightColor);
            x += width + 1;
        }

        ++row;
    }
}

void TeamsAndStandingsScreen::drawFooter(PageComposer& page, const Rect& footer) const
{
    const Rect textArea{ Point{ footer.position.x + 2, footer.position.y + 1 },
        Size{ std::max(0, footer.size.width - 42), 2 } };
    page.writeLine(textArea, 0, TextClip::clipUtf8Text(
        m_dataContext.isLoaded() ? m_dataContext.worldName : "No league loaded",
        textArea.size.width), PFF_Colors::TextHighlightColor);
    page.writeLine(textArea, 1, TextClip::clipUtf8Text(m_gameTimeLabel,
        textArea.size.width), PFF_Colors::TextHighlightColor);
}

void TeamsAndStandingsScreen::drawHeadingTooltip(Surface& surface) const
{
    auto& buffer = surface.buffer();
    for (const auto& tooltip : m_headingTooltips)
    {
        if (!tooltip.bounds.contains(m_mousePosition.x, m_mousePosition.y)) continue;
        const int width = static_cast<int>(tooltip.label.size()) + 4;
        const int x = std::clamp(m_mousePosition.x, 0, std::max(0, buffer.getWidth() - width));
        const int y = std::clamp(m_mousePosition.y + 1, 0, std::max(0, buffer.getHeight() - 3));
        const Rect panel{ Point{ x, y }, Size{ width, 3 } };
        buffer.fillRect(panel, U' ', PFF_Colors::HeaderInnerColor);
        buffer.drawFrame(panel, PFF_Colors::TextHighlightColor,
            U'\u250c', U'\u2510', U'\u2514', U'\u2518', U'\u2500', U'\u2502');
        buffer.writeString(x + 2, y + 1, tooltip.label, PFF_Colors::TextHighlightColor);
        break;
    }
}

