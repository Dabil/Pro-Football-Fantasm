// PFF includes
#include "Screens/LeagueScheduleScreen.h"
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
        "LEAGUE SCHEDULE",
        PFF_Colors::ProFootballTitleColor);
}

void LeagueScheduleScreen::onExit()
{
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

    drawFooter(page,footer);
    // Save the static page before drawing interactive button states.
    m_scheduleCache = std::make_unique<Surface>();
    m_scheduleCache->buffer() = buffer;
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
    // Keyboard-active buttons use the same visible highlight as mouse hover.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor;

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
        m_leaguePlayButton->setFocusedStyle(ButtonFocusedStyle);
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
        m_lookAheadButton->setFocusedStyle(ButtonFocusedStyle);
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
        m_lookBackButton->setFocusedStyle(ButtonFocusedStyle);
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
        context.currentTimeUtc != m_dataContext.currentTimeUtc)
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
    m_scheduleLoaded = !m_availableWeeks.empty();

    if (!m_scheduleLoaded)
    {
        m_scheduleLoadMessage =
            "The loaded league contains no regular-season schedule weeks.";
    }

    if (!sameSeason && m_scheduleLoaded)
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
    std::string_view contentRegionName)
{
    const std::int32_t week = currentWeek();

    if (week <= 0)
    {
        return;
    }

    const std::string weekTitle =
        "WEEK " + std::to_string(week);

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
    const int extra=content.size.width-102;
    const std::array<int,5> widths{{24+extra/3,15,11,18+extra/3,26+extra-2*(extra/3)}};
    const std::array<std::string,5> headings{{"MatchUp","Day and Date","Time","Location","Result"}};
    std::vector<std::string> rows;
    auto appendRow=[&](const std::array<std::string,5>& values)
    {
        std::array<std::vector<std::string>,5> wrapped;
        std::size_t height=1;
        for (std::size_t column=0;column<5;++column)
        {
            wrapped[column]=wrapText(values[column],widths[column]);
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
        }
    };
    std::string previousGroup;
    bool first=true;

    for (const WorldScheduleGameRecord& game : m_scheduleGames)
    {
        if (game.stage != "REGULAR" || game.weekNumber != week)
        {
            continue;
        }

        const std::string group=game.scheduledKickoffUtc.empty()
            ? "unknown:"+game.gameDate+" "+game.timeLabel : game.scheduledKickoffUtc;
        if (first || group!=previousGroup)
        {
            if (!first) rows.emplace_back();
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
            game.venueName.empty()?"Venue unavailable":game.venueName,displayResult(game)}});
    }

    m_visibleRows=std::max(1,content.size.height-4);
    m_maxScrollOffset=std::max(0,static_cast<int>(rows.size())-m_visibleRows);
    m_scrollOffset=std::clamp(m_scrollOffset,0,m_maxScrollOffset);
    for (int row=0;row<m_visibleRows && m_scrollOffset+row<static_cast<int>(rows.size());++row)
        page.writeLine(content,2+row,rows[static_cast<std::size_t>(m_scrollOffset+row)],PFF_Colors::TextHighlightColor);
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

