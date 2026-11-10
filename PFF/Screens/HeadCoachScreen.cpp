#include "Screens/HeadCoachScreen.h"
#include "Screens/GameTimeFormatting.h"
#include "Styles/PFF_Theme.h"
#include "Rendering/Surface.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Utilities/Text/TextClip.h"
#include <algorithm>
#include <utility>

HeadCoachScreen::HeadCoachScreen(Assets::AssetLibrary& assets,
    std::function<bool(std::string_view)> dispatch,
    std::function<LoadedWorldContext()> context,
    LoadedWorldContext selectedWorld, std::int64_t teamSeasonId,
    std::function<void(std::int64_t)> openFormationEditor)
    : m_dispatch(std::move(dispatch)), m_context(std::move(context)),
      m_selectedWorld(std::move(selectedWorld)), m_teamSeasonId(teamSeasonId),
      m_openFormationEditor(std::move(openFormationEditor))
{
    const auto font = assets.loadBannerFontAsset("banner.ansi_shadow");
    if (font.success && font.hasFont())
        m_banner = BannerFactory::makeBanner(*font.asset.font, "Head Coach", PFF_Colors::ProFootballTitleColor);

    constexpr const char* labels[] = {
        "Coaching Schedule", "Formation Editor", "Personnel Packages", "Play Editor", "Game Plan"
    };
    for (std::size_t i = 0; i < m_controls.size(); ++i)
    {
        auto button = std::make_unique<Button>(labels[i]);
        m_controls[i] = button.get();
        button->setEnabled(i == 1 && bool(m_openFormationEditor));
        m_root.addChild(std::move(button));
    }
    m_controls[1]->setActivationCallback([this](const ButtonActivationResult& result) {
        if (result.activated) m_formationPending = true;
    });
    auto back = std::make_unique<Button>("Return");
    m_return = back.get();
    back->setActivationCallback([this](const ButtonActivationResult& result) {
        if (result.activated) m_returnPending = true;
    });
    m_root.addChild(std::move(back));
}

LoadedWorldContext HeadCoachScreen::currentWorld() const
{
    return m_context ? m_context() : m_selectedWorld;
}

bool HeadCoachScreen::matchesSelection(const LoadedWorldContext& world) const
{
    return world.hasActiveCompetitionSeason() &&
        world.databasePath == m_selectedWorld.databasePath &&
        world.worldKey == m_selectedWorld.worldKey &&
        world.activeCompetitionSeasonId == m_selectedWorld.activeCompetitionSeasonId;
}

void HeadCoachScreen::onEnter()
{
    m_root.resetFocus();
}

bool HeadCoachScreen::handleEvent(const Input::Event& event)
{
    const bool handled = m_root.handleEvent(event);
    if (m_returnPending)
    {
        m_returnPending = false;
        const auto dispatch = m_dispatch;
        if (dispatch) dispatch("nav:back");
        return true;
    }
    if (m_formationPending)
    {
        m_formationPending = false;
        const auto open = m_openFormationEditor;
        if (open && matchesSelection(currentWorld()))
        {
            m_root.resetFocus();
            open(m_teamSeasonId);
        }
        return true;
    }
    return handled;
}

void HeadCoachScreen::update(const Animation::TickEvent& event)
{
    m_controls[1]->setEnabled(bool(m_openFormationEditor) && matchesSelection(currentWorld()));
    m_root.update(event);
}

void HeadCoachScreen::draw(Surface& surface)
{
    const auto world = currentWorld();
    const bool active = matchesSelection(world);
    m_controls[1]->setEnabled(bool(m_openFormationEditor) && active);

    auto& buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);
    Composition::PageComposer page(buffer);
    page.createFullScreenRegion("Screen");
    const auto screen = page.resolveRegion("Screen");
    const auto [header, afterHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(afterHeader, 5);
    const auto [controls, content] = page.splitRight(body, std::min(34, std::max(28, body.size.width / 3)));
    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.createInsetRegion("TitleArea", "InnerPanel", 4, 2, 2, 1);
    page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor,
        ObjectFactory::doubleLineBorder());
    buffer.fillRect(page.resolveRegion("InnerPanel"), U' ', PFF_Colors::HeaderInnerColor);
    const auto titleArea = page.resolveRegion("TitleArea");
    if (!m_banner.isEmpty() && m_banner.getWidth() <= titleArea.size.width &&
        m_banner.getHeight() <= titleArea.size.height)
        page.writeObjectInRegion(m_banner, "TitleArea", Composition::Align::center());
    else
        page.writeLine(titleArea, 0, "Head Coach", PFF_Colors::ProFootballTitleColor);

    const auto& text = PFF_Colors::TextHighlightColor;
    const auto message = active ? "Choose a coaching option." : "This team is unavailable in the active league.";
    if (content.size.height > 1)
        page.writeLine(content, 1, TextClip::clipUtf8Text(message, content.size.width), text);

    const int spacing = controls.size.height >= 25 ? 5 : 3;
    for (std::size_t i = 0; i < m_controls.size(); ++i)
        m_controls[i]->setBounds(Rect{Point{controls.position.x + 1,
            controls.position.y + 1 + static_cast<int>(i) * spacing},
            Size{std::max(0, controls.size.width - 2), 3}});
    const int returnWidth = std::min(28, std::max(0, footer.size.width / 3));
    m_return->setBounds(Rect{Point{footer.position.x + footer.size.width - returnWidth,
        footer.position.y + 1}, Size{returnWidth, 3}});
    for (std::size_t i = 0; i < m_root.childCount(); ++i)
    {
        auto* button = static_cast<Button*>(m_root.childAt(i));
        button->setNormalStyle(PFF_Colors::ButtonNormalColor);
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        button->setHoverStyle(PFF_Colors::ButtonHoverColor);
        button->setPressedStyle(PFF_Colors::ButtonPressedColor);
        button->setDisabledStyle(PFF_Colors::ButtonNormalColor.withDim(true));
    }

    const Rect footerText{Point{footer.position.x + 2, footer.position.y + 1},
        Size{std::max(0, footer.size.width - returnWidth - 4), 2}};
    page.writeLine(footerText, 0, TextClip::clipUtf8Text(
        world.isLoaded() ? world.worldName : "No league loaded", footerText.size.width), text);
    if (world.isLoaded())
        page.writeLine(footerText, 1, TextClip::clipUtf8Text(
            PffGameTime::formatGameTimeEastern(world.currentTimeUtc), footerText.size.width), text);
    m_root.setBounds(screen);
    m_root.draw(surface);
}
