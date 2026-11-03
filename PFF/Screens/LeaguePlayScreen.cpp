// PFF includes
#include "Screens/LeaguePlayScreen.h"
#include "Styles/PFF_Theme.h"

// TUI includes
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"

// C++ library includes
#include <utility>

namespace
{
	using Composition::PageComposer;
}

LeaguePlayScreen::LeaguePlayScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
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
    const auto [footer, body] = page.splitBottom(afterHeader, 10);
    const auto [leftPane, rightPane] = page.splitLeft(body, body.size.width / 2);
    const auto [leftTopPane, leftBottomPane] = page.splitTop(leftPane, leftPane.size.height / 2);
    const auto [rightTopPane, rightBottomPane] = page.splitTop(rightPane, rightPane.size.height / 2);

    m_root.setBounds(body);
    positionButtons(leftTopPane, leftBottomPane, rightTopPane, rightBottomPane, footer);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

    page.createRegion("Footer", footer);
    page.createRegion("Body", body);

    page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor, ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("InnerPanel"), U' ', PFF_Colors::HeaderInnerColor);
    page.writeObjectInRegion(m_LeaguePlayTitleObject, "TitleArea", Composition::Align::center());

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
    m_root.addChild(std::move(leagueLeadersButton));
    m_root.addChild(std::move(tradeDeskButton));
}

void LeaguePlayScreen::positionButtons(
    const Rect& leftTopPane,
    const Rect& leftBottomPane,
    const Rect& rightTopPane,
    const Rect& rightBottomPane,
    const Rect& footer)
{
    // Keyboard-active buttons use the same visible highlight as mouse hover.
    const Style& ButtonFocusedStyle = PFF_Colors::ButtonHoverColor;

    if (m_mainButton)
    {
        const Size buttonSize{ 28, 3 };

        m_mainButton->setBounds(Rect{
            Point{
                footer.position.x + ((footer.size.width - buttonSize.width) - 10),
                footer.position.y + ((footer.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_mainButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_mainButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_mainButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_mainButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if (m_teamsAndStandingsButton)
    {
        const Size buttonSize{ 28, 3 };

        m_teamsAndStandingsButton->setBounds(Rect{
            Point{
                leftTopPane.position.x + ((leftTopPane.size.width - buttonSize.width) / 2),
                leftTopPane.position.y + ((leftTopPane.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_teamsAndStandingsButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_teamsAndStandingsButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_teamsAndStandingsButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_teamsAndStandingsButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if (m_leagueScheduleButton)
    {
        const Size buttonSize{ 28, 3 };

        m_leagueScheduleButton->setBounds(Rect{
            Point{
                leftBottomPane.position.x + ((leftBottomPane.size.width - buttonSize.width) / 2),
                leftBottomPane.position.y + ((leftBottomPane.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_leagueScheduleButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_leagueScheduleButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_leagueScheduleButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_leagueScheduleButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if (m_leagueLeadersButton)
    {
        const Size buttonSize{ 28, 3 };

        m_leagueLeadersButton->setBounds(Rect{
            Point{
                rightTopPane.position.x + ((rightTopPane.size.width - buttonSize.width) / 2),
                rightTopPane.position.y + ((rightTopPane.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_leagueLeadersButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_leagueLeadersButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_leagueLeadersButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_leagueLeadersButton->setFocusedStyle(ButtonFocusedStyle);
    }

    if (m_tradeDeskButton)
    {
        const Size buttonSize{ 28, 3 };

        m_tradeDeskButton->setBounds(Rect{
            Point{
                rightBottomPane.position.x + ((rightBottomPane.size.width - buttonSize.width) / 2),
                rightBottomPane.position.y + ((rightBottomPane.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_tradeDeskButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_tradeDeskButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_tradeDeskButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_tradeDeskButton->setFocusedStyle(ButtonFocusedStyle);
    }
}

void LeaguePlayScreen::handleButtonActivation(const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}