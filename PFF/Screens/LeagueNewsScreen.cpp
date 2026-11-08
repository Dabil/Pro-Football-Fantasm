#include "Screens/LeagueNewsScreen.h"

#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Rendering/Surface.h"
#include "Styles/PFF_Theme.h"
#include "Screens/GameTimeFormatting.h"
#include "GCE/Database/WorldScheduler.h"
#include "UI/Widgets/Label.h"
#include "UI/Widgets/ScrollBox.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include "UI/Widgets/Button.h"

#include <algorithm>
#include <utility>

LeagueNewsScreen::LeagueNewsScreen(Assets::AssetLibrary& assets, std::function<void()> returnToLeaguePlay,
                                   std::function<LoadedWorldContext()> context)
    : m_assets(assets), m_returnToLeaguePlay(std::move(returnToLeaguePlay)),
      m_contextProvider(std::move(context))
{
    auto news = std::make_unique<ScrollBox>();
    auto scrollStyle = news->styleSet();
    scrollStyle.focused = PFF_Colors::ScrollBoxFocusedStyle;
    news->setStyleSet(scrollStyle);
    m_news = news.get();
    m_root.addChild(std::move(news));
    auto dates = std::make_unique<ScrollBox>();
    dates->setStyleSet(scrollStyle);
    m_dates = dates.get();
    m_root.addChild(std::move(dates));
    auto button = std::make_unique<Button>("Return");
    button->setNormalStyle(PFF_Colors::ButtonNormalColor);
    button->setHoverStyle(PFF_Colors::ButtonHoverColor);
    button->setPressedStyle(PFF_Colors::ButtonPressedColor);
    button->setFocusedStyle(PFF_Colors::ButtonHoverColor);
    button->setActivationCallback([this](const ButtonActivationResult& result) {
        m_returnPending = result.activated;
    });
    m_return = button.get();
    m_root.addChild(std::move(button));
}

void LeagueNewsScreen::onEnter()
{
    m_root.resetFocus();
    refresh();
    const auto font = m_assets.loadBannerFontAsset("banner.ansi_shadow");
    if (font.success && font.hasFont())
        m_title = BannerFactory::makeBanner(*font.asset.font, "LEAGUE NEWS", PFF_Colors::ProFootballTitleColor);
}

bool LeagueNewsScreen::handleEvent(const Input::Event& event)
{
    const bool handled = m_root.handleEvent(event);
    if (m_returnPending)
    {
        m_returnPending = false;
        const auto back = m_returnToLeaguePlay;
        if (back) back();
        return true;
    }
    return handled;
}

void LeagueNewsScreen::refresh()
{
    m_context = m_contextProvider();
    m_days.clear();
    m_entries.clear();
    m_selectedDay = 0;
    m_datesWidth = m_newsWidth = -1;
    m_dates->viewport().scrollTo(0, 0);
    m_news->viewport().scrollTo(0, 0);
    if (!m_context.hasActiveCompetitionSeason())
    {
        m_message = "Load a league to view its news.";
        return;
    }
    const auto actor = openLocalWorldActor(m_context.databasePath);
    if (!actor.success)
    {
        m_message = "Unable to open league news: " + actor.message;
        return;
    }
    const auto news = loadWorldLeagueNews(actor.value, m_context.activeCompetitionSeasonId);
    if (!news.success)
    {
        m_message = "Unable to load league news: " + news.message;
        return;
    }
    m_message = "No completed league news yet.";
    for (const auto& event : news.value)
    {
        const auto local = PffGameTime::easternTime(event.occurredUtc);
        if (!local) continue;
        const auto day = PffGameTime::dayAndDate(*local);
        if (m_days.empty() || m_days.back() != day)
        {
            m_days.push_back(day);
            m_entries.emplace_back();
        }
        m_entries.back().push_back(PffGameTime::clock(*local) + "  " + event.headline);
    }
}

void LeagueNewsScreen::rebuildDates(int width)
{
    if (width == m_datesWidth) return;
    const bool opening = m_datesWidth < 0;
    const int previousScroll = m_dates->viewport().scrollY();
    m_datesWidth = width;
    m_dates->clearChildren();
    int row = 0;
    for (std::size_t remaining = m_days.size(); remaining > 0; --remaining)
    {
        const auto day = remaining - 1;
        auto button = std::make_unique<Button>(Rect{Point{0, row}, Size{width, 3}}, m_days[day], "news:date");
        button->setNormalStyle(PFF_Colors::ButtonNormalColor);
        button->setHoverStyle(PFF_Colors::ButtonHoverColor);
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        button->setPressedStyle(PFF_Colors::ButtonPressedColor);
        button->setActivationCallback([this, day](const ButtonActivationResult& result) {
            if (!result.activated || m_selectedDay == day) return;
            m_selectedDay = day;
            m_newsWidth = -1;
            m_news->viewport().scrollTo(0, 0);
        });
        m_dates->addChild(std::move(button));
        row += 3;
    }
    m_dates->setContentSize(width, row);
    if (opening) m_dates->viewport().scrollToBottom();
    else m_dates->viewport().scrollTo(0, previousScroll);
}

void LeagueNewsScreen::rebuildNews(int width)
{
    if (width == m_newsWidth) return;
    m_newsWidth = width;
    m_news->clearChildren();
    int row = 0;
    const auto add = [&](const std::string& text) {
        for (const auto& line : wrapText(text, width))
        {
            auto label = std::make_unique<Label>(Rect{Point{0, row++}, Size{width, 1}}, line);
            label->setTextStyle(PFF_Colors::NormalTextColor);
            m_news->addChild(std::move(label));
        }
        ++row;
    };
    if (m_entries.empty()) add(m_message);
    else for (const auto& text : m_entries[m_selectedDay]) add(text);
    m_news->setContentSize(width, row);
}

void LeagueNewsScreen::draw(Surface& surface)
{
    auto& buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);
    Composition::PageComposer page(buffer);
    page.createFullScreenRegion("Screen");
    const auto screen = page.resolveRegion("Screen");
    const auto [header, belowHeader] = page.splitTop(screen, 12);
    const auto [footer, body] = page.splitBottom(belowHeader, 5);

    page.createRegion("Header", header);
    page.createInsetRegion("InnerPanel", "Header", 2, 1, 2, 1);
    page.drawPanel("Header", PFF_Colors::HeaderOuterColor, PFF_Colors::HeaderOuterColor,
        ObjectFactory::doubleLineBorder());
    const auto inner = page.resolveRegion("InnerPanel");
    buffer.fillRect(inner, U' ', PFF_Colors::HeaderInnerColor);
    page.writeObjectInRegion(m_title, "InnerPanel", Composition::Align::center());

    const auto [navigation, content] = page.splitRight(body, std::min(30, body.size.width / 3));
    page.createRegion("ContentPane", content);
    page.createRegion("NavigationPane", navigation);
    for (const auto* pane : {"ContentPane", "NavigationPane"})
        page.drawPanel(pane, PFF_Colors::MainBackgroundColor, PFF_Colors::MainBorderColor,
            ObjectFactory::singleLineBorder());
    page.writePanelTitle("ContentPane", m_days.empty() ? "News" : m_days[m_selectedDay],
        PFF_Colors::TextHighlightColor, 3, 1);
    page.writePanelTitle("NavigationPane", "Dates", PFF_Colors::TextHighlightColor, 3, 1);
    page.createInsetRegion("News", "ContentPane", 2, 2, 2, 1);
    page.createInsetRegion("Dates", "NavigationPane", 2, 2, 2, 1);
    const auto news = page.resolveRegion("News");
    const auto dates = page.resolveRegion("Dates");
    m_news->setBounds(news);
    m_dates->setBounds(dates);
    rebuildNews(std::max(1, news.size.width - 1));
    rebuildDates(std::max(1, dates.size.width - 1));

    const Rect footerText{Point{footer.position.x + 2, footer.position.y + 1},
        Size{std::max(0, footer.size.width - 42), 2}};
    page.writeLine(footerText, 0, TextClip::clipUtf8Text(
        m_context.isLoaded() ? m_context.worldName : "No league loaded", footerText.size.width),
        PFF_Colors::TextHighlightColor);
    if (m_context.isLoaded())
        page.writeLine(footerText, 1, TextClip::clipUtf8Text(
            PffGameTime::formatGameTimeEastern(m_context.currentTimeUtc), footerText.size.width),
            PFF_Colors::TextHighlightColor);

    const int width = std::min(28, footer.size.width);
    m_root.setBounds(screen);
    m_return->setBounds(Rect{Point{footer.position.x + std::max(0, footer.size.width - width - 2),
                                  footer.position.y + 1}, Size{width, std::min(3, footer.size.height)}});
    m_root.draw(surface);
}
