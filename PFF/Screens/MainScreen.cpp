// PFF includes
#include "Screens/MainScreen.h"
#include "Styles/PFF_Theme.h"

// TUI includes
#include "Core/Rect.h"
#include "Assets/Loaders/AsciiBannerFontLoader.h"
#include "Assets/Models/AsciiBannerFont.h"
#include "Rendering/Surface.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Composition/Alignment.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Objects/AsciiBanner.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/TextObjectFactory.h"
#include "UI/Widgets/Button.h"

namespace
{
	using Composition::PageComposer;
}

MainScreen::MainScreen(
	Assets::AssetLibrary& assetLibrary,
	std::function<bool(std::string_view)> commandDispatcher,
	std::function<bool()> hasLoadedLeague)
	: m_assetLibrary(assetLibrary)
	, m_commandDispatcher(std::move(commandDispatcher))
	, m_hasLoadedLeague(std::move(hasLoadedLeague))
{
	buildButtons();
	updateLeaguePlayAvailability();
}

MainScreen::~MainScreen()
{

}

void MainScreen::onEnter()
{
	updateLeaguePlayAvailability();
	m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

	if (!m_fontResult.success || !m_fontResult.hasFont())
	{
		return;
	}

	m_ProFootballTitleObject = BannerFactory::makeBanner(
		*m_fontResult.asset.font,
		"PRO FOOTBALL",
		PFF_Colors::ProFootballTitleColor);

	m_FantasmTitleObject = BannerFactory::makeBanner(
		*m_fontResult.asset.font,
		"Fantasm",
		PFF_Colors::FantasmTitleColor);
}

void MainScreen::onExit()
{
	m_ProFootballTitleObject.clear();
	m_FantasmTitleObject.clear();
}

bool MainScreen::handleEvent(const Input::Event& event)
{
    updateLeaguePlayAvailability();
    return m_root.handleEvent(event);
}

void MainScreen::update(const Animation::TickEvent& event)
{
    updateLeaguePlayAvailability();
    m_root.update(event);
}

void MainScreen::draw(Surface& surface)
{
	updateLeaguePlayAvailability();
	ScreenBuffer& buffer = surface.buffer();
	buffer.clear(PFF_Colors::MainBackgroundColor);

	PageComposer page(buffer);
	page.clearRegions();
	page.createFullScreenRegion("Screen");
	
	const Rect screen = page.resolveRegion("Screen");
	const auto [header, body] = page.splitTop(screen, 20);
	const auto [leftPane, rightPane] = page.splitLeft(body, body.size.width / 2);

    m_root.setBounds(body);
    positionButtons(leftPane, rightPane);

	page.createRegion("Header", header);
	page.createRegion("Body", body);

	page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
	page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

	page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor, ObjectFactory::doubleLineBorder());
	buffer.fillRect(page.resolveRegion("InnerPanel"), U' ', PFF_Colors::HeaderInnerColor);
	page.writeObjectInRegion(m_ProFootballTitleObject, "TitleArea", Composition::Align::topCenter());
	page.writeObjectInRegion(m_FantasmTitleObject, "TitleArea", Composition::Align::bottomCenter());

    m_root.draw(surface);
}

void MainScreen::buildButtons()
{
    auto leaguePlayButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "League Play",
        "route:pff:league_play");

    leaguePlayButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_leaguePlayButton = leaguePlayButton.get();
    m_root.addChild(std::move(leaguePlayButton));

    auto commissionersOfficeButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Commissioner's Office",
        "route:pff:commissioners_office");

    commissionersOfficeButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_commissionersOfficeButton = commissionersOfficeButton.get();
    m_root.addChild(std::move(commissionersOfficeButton));
}

void MainScreen::updateLeaguePlayAvailability()
{
    const bool available = m_hasLoadedLeague && m_hasLoadedLeague();
    m_leaguePlayButton->setVisible(available);
    m_leaguePlayButton->setEnabled(available);

    if (!available && m_root.focusedChild() == m_leaguePlayButton)
    {
        m_root.setFocusedChild(*m_commissionersOfficeButton);
    }
}

void MainScreen::positionButtons(const Rect& leftPane, const Rect& rightPane)
{
    if (m_leaguePlayButton)
    {
        const Size buttonSize{ 34, 3 };

        m_leaguePlayButton->setBounds(Rect{
            Point{
                leftPane.position.x + ((leftPane.size.width - buttonSize.width) / 2),
                leftPane.position.y + ((leftPane.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_leaguePlayButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_leaguePlayButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_leaguePlayButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_leaguePlayButton->setFocusedStyle(PFF_Colors::ButtonNormalColor);
    }

    if (m_commissionersOfficeButton)
    {
        const Size buttonSize{ 34, 3 };

        m_commissionersOfficeButton->setBounds(Rect{
            Point{
                rightPane.position.x + ((rightPane.size.width - buttonSize.width) / 2),
                rightPane.position.y + ((rightPane.size.height - buttonSize.height) / 2)
            },
            buttonSize
            });

        m_commissionersOfficeButton->setNormalStyle(PFF_Colors::ButtonNormalColor);
        m_commissionersOfficeButton->setHoverStyle(PFF_Colors::ButtonHoverColor);
        m_commissionersOfficeButton->setPressedStyle(PFF_Colors::ButtonPressedColor);
        m_commissionersOfficeButton->setFocusedStyle(PFF_Colors::ButtonNormalColor);
    }
}

void MainScreen::handleButtonActivation(const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}
