// PFF includes
#include "Screens/TradeDeskScreen.h"
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

TradeDeskScreen::TradeDeskScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
{
    buildButtons();
}

TradeDeskScreen::~TradeDeskScreen()
{

}

void TradeDeskScreen::onEnter()
{
    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_TradeDeskTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "TRADE DESK",
        PFF_Colors::ProFootballTitleColor);
}

void TradeDeskScreen::onExit()
{
    m_TradeDeskTitleObject.clear();
}

bool TradeDeskScreen::handleEvent(const Input::Event& event)
{
    return m_root.handleEvent(event);
}

void TradeDeskScreen::update(const Animation::TickEvent& event)
{
    m_root.update(event);
}

void TradeDeskScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);

    PageComposer page(buffer);
    page.clearRegions();
    page.createFullScreenRegion("Screen");

    const Rect screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(afterHeader, 5);

    m_root.setBounds(body);
    positionButtons(footer);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);

    page.createRegion("Footer", footer);
    page.createRegion("Body", body);

    page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor, ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("InnerPanel"), U' ', PFF_Colors::HeaderInnerColor);
    page.writeObjectInRegion(m_TradeDeskTitleObject, "TitleArea", Composition::Align::center());

    m_root.draw(surface);
}

void TradeDeskScreen::buildButtons()
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

void TradeDeskScreen::positionButtons(const Rect& footer)
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

void TradeDeskScreen::handleButtonActivation(const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}