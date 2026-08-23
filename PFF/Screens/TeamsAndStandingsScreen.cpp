// PFF includes
#include "Screens/TeamsAndStandingsScreen.h"
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

TeamsAndStandingsScreen::TeamsAndStandingsScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
{
    buildButtons();
}

TeamsAndStandingsScreen::~TeamsAndStandingsScreen()
{

}

void TeamsAndStandingsScreen::onEnter()
{
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
}

bool TeamsAndStandingsScreen::handleEvent(const Input::Event& event)
{
	return m_root.handleEvent(event);
}

void TeamsAndStandingsScreen::update(const Animation::TickEvent& event)
{
    m_root.update(event);
}

void TeamsAndStandingsScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);

    PageComposer page(buffer);
    page.clearRegions();
    page.createFullScreenRegion("Screen");

    const Rect screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(afterHeader, 5);
    const auto [afcPane, nfcPane] = page.splitLeft(body, body.size.width / 2);

    int const divisionHeight = afcPane.size.height / 3;

    const Rect afcEast = Rect{ Point{afcPane.position.x, afcPane.position.y},
                         Size{ afcPane.size.width, divisionHeight} };
    const Rect afcCent = Rect{ Point{afcPane.position.x, afcEast.position.y + afcEast.size.height},
                         Size{ afcPane.size.width, divisionHeight} };
    const Rect afcWest = Rect{ Point{afcPane.position.x, afcCent.position.y + afcCent.size.height},
                         Size{ afcPane.size.width, divisionHeight} };

    const Rect nfcEast = Rect{ Point{nfcPane.position.x, nfcPane.position.y},
                         Size{ nfcPane.size.width, divisionHeight} };
    const Rect nfcCent = Rect{ Point{nfcPane.position.x, nfcEast.position.y + nfcEast.size.height},
                         Size{ nfcPane.size.width, divisionHeight} };
    const Rect nfcWest = Rect{ Point{nfcPane.position.x, nfcCent.position.y + nfcCent.size.height},
                         Size{ nfcPane.size.width, divisionHeight} };

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

    page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor, ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("InnerPanel"), U' ', PFF_Colors::HeaderInnerColor);
    page.writeObjectInRegion(m_TeamsAndStandingsTitleObject, "TitleArea", Composition::Align::center());

    page.drawPanel("AFCEast", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
    page.drawPanel("AFCCent", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
    page.drawPanel("AFCWest", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
    page.drawPanel("NFCEast", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
    page.drawPanel("NFCCent", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());
    page.drawPanel("NFCWest", PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor, ObjectFactory::roundedBorder());

    m_root.draw(surface);
}

void TeamsAndStandingsScreen::buildButtons()
{
    auto leaguePlayButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Return",
        "route:pff:league_play");

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

void TeamsAndStandingsScreen::handleButtonActivation(const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}