#include "Screens/HeadCoachScreen.h"
#include "Screens/GameTimeFormatting.h"
#include "Styles/PFF_Theme.h"
#include "Rendering/Surface.h"
#include "Screens/ContentControlFrameView.h"
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

    Composition::PageComposer page(surface.buffer());
    const auto frame = ContentControlFrameView::drawFrame(
        page, m_banner, "Head Coach", ContentControlFrameView::TitleFallback::LeftAligned);
    const auto& content = frame.content;
    const auto& controls = frame.controls;
    const auto& footer = frame.footer;

    const auto& text = PFF_Colors::TextHighlightColor;
    const auto message = active ? "Choose a coaching option." : "This team is unavailable in the active league.";
    if (content.size.height > 1)
        page.writeLine(content, 1, TextClip::clipUtf8Text(message, content.size.width), text);

    const int spacing = controls.size.height >= 25 ? 5 : 3;
    for (std::size_t i = 0; i < m_controls.size(); ++i)
        m_controls[i]->setBounds(Rect{Point{controls.position.x + 1,
            controls.position.y + 1 + static_cast<int>(i) * spacing},
            Size{std::max(0, controls.size.width - 2), 3}});
    m_return->setBounds(PffLeagueFooter::returnBounds(footer));
    for (std::size_t i = 0; i < m_root.childCount(); ++i)
    {
        auto* button = static_cast<Button*>(m_root.childAt(i));
        button->setNormalStyle(PFF_Colors::ButtonNormalColor);
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        button->setHoverStyle(PFF_Colors::ButtonHoverColor);
        button->setPressedStyle(PFF_Colors::ButtonPressedColor);
        button->setDisabledStyle(PFF_Colors::ButtonNormalColor.withDim(true));
    }

    ContentControlFrameView::drawFooter(page, footer,
        world.isLoaded() ? world.worldName : "No league loaded",
        world.isLoaded() ? PffGameTime::formatGameTimeEastern(world.currentTimeUtc) : "");
    m_root.setBounds(frame.screen);
    m_root.draw(surface);
}
