// PFF includes
#include "Screens/CommissionersOfficeScreen.h"
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

CommissionersOfficeScreen::CommissionersOfficeScreen(
    Assets::AssetLibrary& assetLibrary,
    std::function<bool(std::string_view)> commandDispatcher)
    : m_assetLibrary(assetLibrary)
    , m_commandDispatcher(std::move(commandDispatcher))
{
    buildButtons();
}

CommissionersOfficeScreen::~CommissionersOfficeScreen()
{

}

void CommissionersOfficeScreen::onEnter()
{
    m_fontResult = m_assetLibrary.loadBannerFontAsset(m_TitleBannerFontKey);

    if (!m_fontResult.success || !m_fontResult.hasFont())
    {
        return;
    }

    m_CommissionersOfficeTitleObject = BannerFactory::makeBanner(
        *m_fontResult.asset.font,
        "COMMISSIONER'S OFFICE",
        PFF_Colors::ProFootballTitleColor);
}

void CommissionersOfficeScreen::onExit()
{
    m_CommissionersOfficeTitleObject.clear();
}

bool CommissionersOfficeScreen::handleEvent(const Input::Event& event)
{
    return m_root.handleEvent(event);
}

void CommissionersOfficeScreen::update(const Animation::TickEvent& event)
{
    m_root.update(event);
}

void CommissionersOfficeScreen::draw(Surface& surface)
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
    page.writeObjectInRegion(m_CommissionersOfficeTitleObject, "TitleArea", Composition::Align::center());

    m_root.draw(surface);
}

void CommissionersOfficeScreen::buildButtons()
{
    auto mainButton = std::make_unique<Button>(
        Rect{ Point{ 0, 0 }, Size{ 34, 3 } },
        "Return",
        "route:pff:main");

    mainButton->setActivationCallback(
        [this](const ButtonActivationResult& result)
        {
            handleButtonActivation(result);
        });

    m_mainButton = mainButton.get();
    m_root.addChild(std::move(mainButton));
}

void CommissionersOfficeScreen::positionButtons(const Rect& footer)
{
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
        m_mainButton->setFocusedStyle(PFF_Colors::ButtonNormalColor);
    }
}

void CommissionersOfficeScreen::handleButtonActivation(const ButtonActivationResult& result)
{
    if (!result.activated || result.commandId.empty() || !m_commandDispatcher)
    {
        return;
    }

    m_commandDispatcher(result.commandId);
}