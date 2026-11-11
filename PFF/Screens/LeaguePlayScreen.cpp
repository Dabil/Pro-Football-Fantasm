// PFF includes
#include "Screens/LeaguePlayScreen.h"
#include "Styles/PFF_Theme.h"
#include "Screens/GameTimeFormatting.h"
#include "Utilities/Text/TextClip.h"

// TUI includes
#include "Rendering/Surface.h"
#include "Screens/LeagueFooter.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"

// C++ library includes
#include <algorithm>
#include <utility>

namespace
{
	using Composition::PageComposer;
}

LeaguePlayScreen::LeaguePlayScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher,
    std::function<LoadedWorldContext()> loadedWorldContextProvider)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
    , m_loadedWorldContextProvider(std::move(loadedWorldContextProvider))
{
    buildButtons();
}

LeaguePlayScreen::~LeaguePlayScreen()
{

}

void LeaguePlayScreen::onEnter()
{
    m_root.resetFocus();
    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_LeaguePlayTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "LEAGUE PLAY",
        PFF_Colors::ProFootballTitleColor);
}

void LeaguePlayScreen::onExit()
{
	m_LeaguePlayTitleObject.clear();
}

bool LeaguePlayScreen::handleEvent(const Input::Event& event)
{
	return m_root.handleEvent(event);
}

void LeaguePlayScreen::update(const Animation::TickEvent& event)
{
	m_root.update(event);
}

void LeaguePlayScreen::draw(Surface& surface)
{
	ScreenBuffer& buffer = surface.buffer();
	buffer.clear(PFF_Colors::MainBackgroundColor);

	PageComposer page(buffer);
	page.clearRegions();
	page.createFullScreenRegion("Screen");

	const Rect screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 20);
    const auto [footer, body] = page.splitBottom(afterHeader, PffLeagueFooter::Height);
    const auto [leftPane, remainingPanes] = page.splitLeft(body, body.size.width / 3);
    const auto [middlePane, rightPane] = page.splitLeft(remainingPanes, remainingPanes.size.width / 2);
    const auto middleTopPane = page.splitTop(middlePane, middlePane.size.height / 2).first;
    const auto [leftTopPane, leftBottomPane] = page.splitTop(leftPane, leftPane.size.height / 2);
    const auto [rightTopPane, rightBottomPane] = page.splitTop(rightPane, rightPane.size.height / 2);

    m_root.setBounds(body);
    positionButtons(leftTopPane, leftBottomPane, middleTopPane, rightTopPane, rightBottomPane, footer);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

    page.createRegion("Footer", footer);
    page.createRegion("Body", body);

    page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor, ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("InnerPanel"), U' ', PFF_Colors::HeaderInnerColor);
    page.writeObjectInRegion(m_LeaguePlayTitleObject, "TitleArea", Composition::Align::center());

    const auto world = m_loadedWorldContextProvider ? m_loadedWorldContextProvider() : LoadedWorldContext{};
    const auto footerText = PffLeagueFooter::textBounds(footer);
    page.writeLine(footerText, 0, TextClip::clipUtf8Text(
        world.isLoaded() ? world.worldName : "No league loaded", footerText.size.width),
        PFF_Colors::TextHighlightColor);
    if (world.isLoaded())
        page.writeLine(footerText, 1, TextClip::clipUtf8Text(
            PffGameTime::formatGameTimeEastern(world.currentTimeUtc), footerText.size.width),
            PFF_Colors::TextHighlightColor);

    m_root.draw(surface);
}

void LeaguePlayScreen::buildButtons()
{
    auto mainButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Return",
        "nav:back");

    auto teamsAndStandingsButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Teams & Standings",
        "route:pff:teams_and_standings");

    auto leagueScheduleButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "League Schedule",
        "route:pff:league_schedule");

    auto leagueNewsButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "League News",
        "route:pff:league_news");
    leagueNewsButton->setActivationCallback(
        [this](const ButtonActivationResult& result) { handleButtonActivation(result); });
    m_leagueNewsButton = leagueNewsButton.get();

    auto leagueLeadersButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "League Leaders",
        "route:pff:league_leaders");

    auto tradeDeskButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Trade Desk",
        "route:pff:trade_desk");

    mainButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    teamsAndStandingsButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    leagueScheduleButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    leagueLeadersButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    tradeDeskButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_mainButton = mainButton.get();
    m_teamsAndStandingsButton = teamsAndStandingsButton.get();
    m_leagueScheduleButton = leagueScheduleButton.get();
    m_leagueLeadersButton = leagueLeadersButton.get();
    m_tradeDeskButton = tradeDeskButton.get();

    m_root.addChild(std::move(mainButton));
    m_root.addChild(std::move(teamsAndStandingsButton));
    m_root.addChild(std::move(leagueScheduleButton));
    m_root.addChild(std::move(leagueNewsButton));
    m_root.addChild(std::move(leagueLeadersButton));
    m_root.addChild(std::move(tradeDeskButton));
}

void LeaguePlayScreen::positionButtons(
    const Rect& leftTopPane,
    const Rect& leftBottomPane,
    const Rect& middleTopPane,
    const Rect& rightTopPane,
    const Rect& rightBottomPane,
    const Rect& footer)
{
    // Keyboard-active buttons use the same visible highlight as mouse hover.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor;

    if (m_mainButton)
    {
        m_mainButton->setBounds(PffLeagueFooter::returnBounds(footer));

        m_mainButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_mainButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_mainButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_mainButton->setFocusedStyle(ButtonFocusedStyle);
    }

    const auto centerButton = [&](Button& button, const Rect& pane)
    {
        const Size size{std::min(28, pane.size.width), std::min(3, pane.size.height)};
        button.setBounds(Rect{
            Point{pane.position.x + (pane.size.width - size.width) / 2,
                  pane.position.y + (pane.size.height - size.height) / 2},
            size});
        button.setNormalStyle(PFF_Colors::ButtonNormalColor);
        button.setHoverStyle(PFF_Colors::ButtonHoverColor);
        button.setPressedStyle(PFF_Colors::ButtonPressedColor);
        button.setFocusedStyle(ButtonFocusedStyle);
    };

    centerButton(*m_teamsAndStandingsButton, leftTopPane);
    centerButton(*m_leagueScheduleButton, leftBottomPane);
    centerButton(*m_leagueNewsButton, middleTopPane);
    centerButton(*m_leagueLeadersButton, rightTopPane);
    centerButton(*m_tradeDeskButton, rightBottomPane);
}

void LeaguePlayScreen::handleButtonActivation(const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}