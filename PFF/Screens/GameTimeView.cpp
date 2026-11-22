// GameTime view implementation. Renders the controller's display state and forwards widget input as commands.

#include "Screens/GameTimeView.h"
#include "Screens/ScoreboardView.h"
#include "Screens/StatisticsView.h"
#include "Input/Event.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Surface.h"
#include "Styles/PFF_Theme.h"
#include "UI/Widgets/Label.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>
#include <string>

GameTimeView::GameTimeView(Assets::AssetLibrary &assets, std::unique_ptr<PffTest::GameTimeModel> game, WorldActor actor,
                           std::function<void(const WorldEditCommit &)> committed,
                           std::function<void()> back, bool showScoreboardArtwork)
    : m_controller(std::move(game), std::move(actor), std::move(committed)),
      m_scoreboard(showScoreboardArtwork ? ScoreboardView::loadArtwork(assets) : ScoreboardView::Artwork{}),
      m_return(std::move(back))
{
    auto log = std::make_unique<ScrollBox>();
    auto scrollStyle = log->styleSet();
    scrollStyle.focused = scrollStyle.focused.withUnderline(false);
    log->setStyleSet(scrollStyle);
    m_log = log.get();
    m_root.addChild(std::move(log));
    for (auto& list : m_callLists)
    {
        auto widget = std::make_unique<ListBox>();
        widget->setFocusedStyle(widget->normalStyle());
        widget->setVerticalScrollbarVisible(true);
        list = widget.get();
        m_callRoot.addChild(std::move(widget));
    }
    const auto callButton = [&](const char* label, auto callback) {
        auto button = std::make_unique<Button>(label);
        button->setNormalStyle(PFF_Colors::ButtonNormalColor);
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        button->setHoverStyle(PFF_Colors::ButtonHoverColor);
        button->setActivationCallback([callback](const ButtonActivationResult& result) {
            if (result.activated) callback();
        });
        auto* result = button.get();
        m_callRoot.addChild(std::move(button));
        return result;
    };
    m_makeCall = callButton("Make the Call", [this] { makeSelectedCall(); });
    m_timeout = callButton("Time-Out", [this] { m_controller.timeout(); });
    m_pauseCall = callButton("Pause", [this] { m_controller.toggleDecisionPause(); });
    for (const auto &command : {std::pair<const char *, const char *>{"Next Play", "next"},
                                {"Run to Final", "run"},
                                {"Previous Play", "previous"},
                                {"Broadcast", "broadcast"},
                                {"Detail", "detail"},
                                {"PBP", "pbp"},
                                {"Pause", "pause"},
                                {"Resume Game", "resume"},
                                {"Return", "return"}})
    {
        auto b = std::make_unique<Button>(Rect{}, command.first, command.second);
        b->setNormalStyle(PFF_Colors::ButtonNormalColor);
        b->setHoverStyle(PFF_Colors::ButtonHoverColor);
        b->setFocusedStyle(PFF_Colors::ButtonHoverColor);
        b->setPressedStyle(PFF_Colors::ButtonPressedColor);
        b->setDisabledStyle(PFF_Colors::ButtonNormalColor.withDim(true));
        b->setActivationCallback([this](const ButtonActivationResult &r) {
            if (!r.activated)
                return;
            if (r.commandId == "next")
                m_controller.nextPlay();
            else if (r.commandId == "previous")
                m_controller.previousPlay();
            else if (r.commandId == "broadcast")
                m_controller.selectView(PffBroadcast::View::Broadcast);
            else if (r.commandId == "detail")
                m_controller.selectView(PffBroadcast::View::Detail);
            else if (r.commandId == "pbp")
                m_controller.selectView(PffBroadcast::View::Pbp);
            else if (r.commandId == "run")
                m_controller.run();
            else if (r.commandId == "pause")
                m_controller.pause();
            else if (r.commandId == "return")
                m_controller.saveAndReturn();
            else if (r.commandId == "resume")
                m_controller.resumeGame();
            updateControls();
        });
        if (std::string(command.second) == "next")
            m_next = b.get();
        else if (std::string(command.second) == "previous")
            m_previous = b.get();
        else if (std::string(command.second) == "broadcast")
            m_broadcast = b.get();
        else if (std::string(command.second) == "detail")
            m_detail = b.get();
        else if (std::string(command.second) == "pbp")
            m_pbp = b.get();
        else if (std::string(command.second) == "run")
            m_run = b.get();
        else if (std::string(command.second) == "pause")
            m_pause = b.get();
        else if (std::string(command.second) == "resume")
            m_resume = b.get();
        else
            m_returnButton = b.get();
        m_root.addChild(std::move(b));
    }
    updateControls();
}
void GameTimeView::onEnter()
{
    m_root.resetFocus();
    m_controller.enter();
    updateControls();
}
void GameTimeView::updateControls()
{
    refreshCallWindow();
    const auto state = m_controller.display();
    m_next->setEnabled(state.canNext);
    m_previous->setEnabled(state.canPrevious);
    m_run->setEnabled(state.canRun);
    m_pause->setEnabled(state.canPause);
    m_returnButton->setEnabled(state.canReturn);
    m_resume->setEnabled(state.canResume);
    for (const auto &[button, view] : {std::pair{m_broadcast, PffBroadcast::View::Broadcast},
                                      std::pair{m_detail, PffBroadcast::View::Detail},
                                      std::pair{m_pbp, PffBroadcast::View::Pbp}})
    {
        const bool dim = state.view != view;
        button->setNormalStyle(PFF_Colors::ButtonNormalColor.withDim(dim));
        button->setHoverStyle(PFF_Colors::ButtonHoverColor.withDim(dim));
        button->setFocusedStyle(PFF_Colors::ButtonHoverColor.withDim(dim));
        button->setPressedStyle(PFF_Colors::ButtonPressedColor.withDim(dim));
    }
}
void GameTimeView::returnToCallerIfReady()
{
    if (!m_controller.takeReturnRequest())
        return;
    // The callback may pop and destroy this screen. Call it only after widget
    // event/update dispatch has unwound, and never access members afterwards.
    const auto back = std::move(m_return);
    if (back)
        back();
}
bool GameTimeView::handleEvent(const Input::Event &event)
{
    // Every user exit follows the same save-before-return path.
    if (const auto *command = event.asCommand())
    {
        if (command->command.code == Input::CommandCode::Cancel ||
            command->command.code == Input::CommandCode::Back)
        {
            m_controller.saveAndReturn();
            updateControls();
            returnToCallerIfReady();
            return true;
        }
    }
    bool routeToCall = m_callActive && m_callFocused;
    if (const auto* mouse = event.asMouse())
    {
        const auto point = mouse->position;
        const bool inside = m_callActive && m_callBounds.contains(point.x, point.y);
        if (mouse->isMove() && !inside)
            for (auto* list : m_callLists) list->clearPointerState();
        routeToCall = m_callPointerCaptured || inside;
        if (mouse->button == Input::MouseButton::Left && (mouse->isPress() || mouse->isClick()))
        {
            m_callFocused = inside;
            if (inside)
                m_root.clearChildFocus();
            else
                m_callRoot.clearChildFocus();
            m_callPointerCaptured = inside && mouse->isPress();
        }
        if (m_dragOffset)
        {
            if (mouse->isMove() || mouse->isDrag())
                m_callPosition = Point{point.x - m_dragOffset->x, point.y - m_dragOffset->y};
            if (mouse->isRelease())
            {
                m_dragOffset.reset();
                m_callPointerCaptured = false;
            }
            return true;
        }
        if (inside && mouse->button == Input::MouseButton::Left && mouse->isPress() &&
            point.y <= m_callBounds.position.y + 1 && !m_callRoot.hitTest(point))
        {
            m_dragOffset = Point{point.x - m_callBounds.position.x, point.y - m_callBounds.position.y};
            return true;
        }
        if (mouse->isRelease()) m_callPointerCaptured = false;
    }
    else if (m_callActive)
    {
        if (const auto* command = event.asCommand())
        {
            const auto code = command->command.code;
            if (code == Input::CommandCode::NextFocus || code == Input::CommandCode::PreviousFocus)
            {
                const bool forward = code == Input::CommandCode::NextFocus;
                auto& current = m_callFocused ? m_callRoot : m_root;
                const int index = current.focusedChild() ? current.indexOfChild(*current.focusedChild()) : -1;
                bool hasNext = index < 0;
                for (int i = index + (forward ? 1 : -1); !hasNext && i >= 0 &&
                     i < static_cast<int>(current.childCount()); i += forward ? 1 : -1)
                    hasNext = current.childAt(i)->canReceiveFocus();
                if (!hasNext)
                {
                    current.clearChildFocus();
                    m_callFocused = !m_callFocused;
                    auto& next = m_callFocused ? m_callRoot : m_root;
                    if (forward) next.focusFirstChild();
                    else next.focusLastChild();
                    if (auto* list = dynamic_cast<ListBox*>(next.focusedChild())) selectCallList(*list);
                    updateControls();
                    return true;
                }
            }
        }
    }
    const bool handled = routeToCall ? handleCallWindowEvent(event) : m_root.handleEvent(event);
    returnToCallerIfReady();
    return handled;
}

bool GameTimeView::handleCallWindowEvent(const Input::Event& event)
{
    if (const auto* mouse = event.asMouse(); mouse && (mouse->isMove() || mouse->isWheel()))
    {
        for (auto* list : m_callLists)
        {
            if (list->isVisible() && list->bounds().contains(mouse->position.x, mouse->position.y))
                list->handleEvent(event);
            else
                list->clearPointerState();
        }
        return true;
    }
    if (const auto* command = event.asCommand())
    {
        if (command->command.code == Input::CommandCode::MoveRight)
        {
            m_callRoot.handleEvent(Input::Event::command(Input::Command{Input::CommandCode::NextFocus}));
            if (auto* list = dynamic_cast<ListBox*>(m_callRoot.focusedChild()))
                selectCallList(*list);
            updateControls();
            return true;
        }
        if (command->command.code == Input::CommandCode::Confirm &&
            dynamic_cast<ListBox*>(m_callRoot.focusedChild()))
        {
            selectCallList(*static_cast<ListBox*>(m_callRoot.focusedChild()));
            makeSelectedCall();
            return true;
        }
    }
    const bool handled = m_callRoot.handleEvent(event);
    if (auto* list = dynamic_cast<ListBox*>(m_callRoot.focusedChild()))
    {
        if (event.asCommand())
            selectCallList(*list);
        if (const auto* mouse = event.asMouse())
            if (handled && mouse->button == Input::MouseButton::Left && (mouse->isPress() || mouse->isClick()) &&
                list->bounds().contains(mouse->position.x, mouse->position.y))
            {
                selectCallList(*list);
                const auto now = std::chrono::steady_clock::now();
                const auto point = mouse->position;
                const bool repeated = m_lastCallClick && m_lastCallClick->x == point.x &&
                    m_lastCallClick->y == point.y && now - m_lastCallClickTime <= std::chrono::milliseconds(500);
                m_lastCallClick = point;
                m_lastCallClickTime = now;
                if (mouse->clickCount == 2 || repeated) makeSelectedCall();
            }
    }
    updateControls();
    return true;
}
void GameTimeView::update(const Animation::TickEvent &event)
{
    m_controller.tick();
    updateControls();
    m_root.update(event);
    if (m_callActive) m_callRoot.update(event);
    returnToCallerIfReady();
}
void GameTimeView::rebuildLog(int width)
{
    const auto state = m_controller.display();
    if (state.logRevision == m_drawnLogRevision && width == m_logWidth)
        return;
    m_log->clearChildren();
    int row = 0;
    const auto &game = state.matchup;
    const PffBroadcast::Teams teams{game.awayCoreKey, game.homeCoreKey, game.awayName, game.homeName};
    const auto lines = !state.play ? std::vector<std::string>{"No play selected."}
                                      : PffBroadcast::render(*state.play, teams, state.view);
    for (const auto &text : lines)
    {
        const auto first = text.find_first_not_of(' ');
        const int indent = first == std::string::npos ? 0 : static_cast<int>(first);
        const auto rows = wrapText(std::string_view(text).substr(indent), std::max(1, width - 2 - indent));
        for (const auto &line : rows)
        {
            auto label = std::make_unique<Label>(
                Rect{Point{indent, row++}, Size{std::max(1, width - 2 - indent), 1}}, line);
            label->setTextStyle(text.rfind("===", 0) == 0   ? PFF_Colors::NormalTextHeadingColor
                                : text.rfind("---", 0) == 0 ? PFF_Colors::NormalTextColor.withBold()
                                                            : PFF_Colors::NormalTextColor);
            m_log->addChild(std::move(label));
        }
    }
    m_log->setContentSize(std::max(1, width - 2), row);
    m_log->viewport().scrollTo(0, 0);
    m_logWidth = width;
    m_drawnLogRevision = state.logRevision;
}
void GameTimeView::draw(Surface &surface)
{
    auto &buffer = surface.buffer();
    buffer.clear(PFF_Colors::MainBackgroundColor);
    Composition::PageComposer page(buffer);
    page.createFullScreenRegion("Screen");
    const auto bounds = page.resolveRegion("Screen");
    m_root.setBounds(bounds);
    const bool small = bounds.size.width < 90 || bounds.size.height < 30;
    const int width = std::max(1, bounds.size.width - 4);
    auto write = [&](int row, const std::string &text) {
        page.writeLine(Rect{Point{2, row}, Size{width, 1}}, 0, TextClip::clipUtf8Text(text, width),
                       PFF_Colors::TextHighlightColor);
    };
    const auto display = m_controller.display();
    const auto &state = display.game;
    const int y = std::max(20, bounds.size.height - 5);
    const auto gameArea = page.splitTop(bounds, y).first;
    constexpr int SummaryHorizontalInset = 3;
    constexpr int SummaryTopInset = 1;
    constexpr int ViewerGap = 1;
    constexpr int MinimumViewerHeight = 6;
    int summaryHeight = gameArea.size.height / 2;
    int scoreboardWidth = gameArea.size.width / 2;
    if (!small)
        scoreboardWidth = std::max(scoreboardWidth, ScoreboardView::boxScoreWidth(state.quarter) + SummaryHorizontalInset);
    const auto artworkSize = ScoreboardView::artworkSummarySize(m_scoreboard, display);
    const int artworkSummaryHeight = artworkSize.height + SummaryTopInset + ViewerGap;
    if (!small && artworkSize.height > 0 &&
        artworkSummaryHeight + MinimumViewerHeight <= gameArea.size.height)
    {
        summaryHeight = artworkSummaryHeight;
        scoreboardWidth = std::max(scoreboardWidth, artworkSize.width + SummaryHorizontalInset);
    }
    const auto [summary, viewer] = page.splitTop(gameArea, summaryHeight);
    const auto [scoreboard, statistics] = page.splitLeft(summary, scoreboardWidth);
    const auto inset = [](const Rect &pane) {
        return Rect{Point{pane.position.x + 2, pane.position.y + 1},
                    Size{std::max(0, pane.size.width - SummaryHorizontalInset), std::max(0, pane.size.height - SummaryTopInset)}};
    };
    ScoreboardView::draw(page, inset(scoreboard), display, m_scoreboard);
    StatisticsView::draw(page, inset(statistics), state);

    const int tabsY = viewer.position.y + (display.interruption ? 1 : 0);
    m_broadcast->setBounds(Rect{Point{2, tabsY}, Size{14, 3}});
    m_detail->setBounds(Rect{Point{18, tabsY}, Size{14, 3}});
    m_pbp->setBounds(Rect{Point{34, tabsY}, Size{14, 3}});
    if (!small && display.replay && display.play)
    {
        const Rect counter{Point{50, tabsY + 1}, Size{bounds.size.width - 52, 1}};
        page.writeLine(counter, 0, "Play " + std::to_string(display.playIndex + 1) + " of " +
            std::to_string(display.playCount), PFF_Colors::TextHighlightColor);
    }
    const int logY = tabsY + 3;
    const int logHeight = std::max(1, viewer.position.y + viewer.size.height - logY - 1);
    m_log->setBounds(Rect{Point{2, logY}, Size{width, logHeight}});
    if (!small)
        rebuildLog(width);
    m_next->setBounds(Rect{Point{2, y}, Size{20, 3}});
    m_run->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_previous->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_pause->setBounds(Rect{Point{46, y}, Size{20, 3}});
    m_resume->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_returnButton->setBounds(Rect{Point{std::max(68, bounds.size.width - 22), y}, Size{20, 3}});
    updateControls();
    m_log->setVisible(!small);
    m_next->setVisible(!small);
    m_run->setVisible(!small && !display.replay && !display.interruption);
    m_previous->setVisible(!small && display.replay);
    m_broadcast->setVisible(!small);
    m_detail->setVisible(!small);
    m_pbp->setVisible(!small);
    m_resume->setVisible(!small && bool(display.interruption));
    m_pause->setVisible(!small && !display.replay);
    m_returnButton->setVisible(!small);
    if (small)
        write(19, "Enlarge the window to at least 90 x 30 to view game controls.");
    if (!small && display.interruption)
        write(viewer.position.y, "EVENT: " + display.interruption->blocker);
    write(bounds.size.height - 2,
          display.error.empty() ? (display.interruption ? "Waiting for event. Handle it, then Resume Game. Return keeps saved progress."
                             : display.replay      ? "Next Play and Previous Play move through the replay."
                             : display.running     ? "Running to final... Pause to inspect; Return saves and exits."
                             : state.final ? "Final saved. Scroll the play-by-play or select Return."
                                           : "Next Play advances once. Return saves and exits.")
                          : display.error);
    m_root.draw(surface);
    if (m_callActive) drawCallWindow(surface);
}

void GameTimeView::refreshCallWindow()
{
    const auto state = m_controller.display();
    m_pauseCall->setLabel(m_controller.decisionPaused() ? "Resume" : "Pause");
    m_makeCall->setEnabled(m_selectedList && !m_controller.decisionPaused());
    m_timeout->setEnabled(m_controller.canTimeout());
    if (m_callGeneration == state.game.generation && m_callActive && !state.running && !state.interruption && !state.game.final)
        return;
    m_callGeneration = state.game.generation;
    const auto calls = m_controller.playCalls();
    if (calls.empty())
    {
        m_callActive = m_callFocused = m_callPointerCaptured = false;
        m_dragOffset.reset();
        m_callBounds = {};
        return;
    }
    if (!m_callActive)
    {
        m_callFocused = true;
        m_root.clearChildFocus();
    }
    m_callActive = true;
    m_callRoot.resetFocus();
    m_lastCallClick.reset();
    m_selectedList = nullptr;
    for (std::size_t i = 0; i < m_callLists.size(); ++i)
    {
        auto& keys = m_callKeys[i];
        auto* list = m_callLists[i];
        keys.clear();
        std::vector<ListBoxItem> items;
        for (const auto& call : calls)
            if (static_cast<std::size_t>(call.group) == i)
            {
                keys.push_back(call.key);
                items.push_back({call.name});
            }
        list->setItems(std::move(items));
        list->clearSelection();
        list->setVisible(!keys.empty());
        list->setEnabled(!keys.empty());
    }
    m_makeCall->setEnabled(false);
}

void GameTimeView::selectCallList(ListBox& list)
{
    for (auto* other : m_callLists)
        if (other != &list) other->clearSelection();
    if (!list.selectedIndex()) list.moveToFirst();
    m_selectedList = &list;
}

void GameTimeView::makeSelectedCall()
{
    if (!m_selectedList || m_controller.decisionPaused()) return;
    const auto selected = m_selectedList->selectedIndex();
    if (!selected) return;
    const auto list = std::find(m_callLists.begin(), m_callLists.end(), m_selectedList) - m_callLists.begin();
    const auto key = m_callKeys[list][*selected];
    m_controller.makeCall(key);
    updateControls();
}

void GameTimeView::drawCallWindow(Surface& surface)
{
    auto& buffer = surface.buffer();
    constexpr int width = 88, height = 19;
    const int maxX = std::max(0, buffer.getWidth() - width);
    const int maxY = std::max(0, buffer.getHeight() - 5 - height);
    const auto requested = m_callPosition.value_or(Point{maxX / 2, maxY / 2});
    const int x = std::clamp(requested.x, 0, maxX), y = std::clamp(requested.y, 0, maxY);
    const Rect panel{Point{x, y}, Size{width, height}};
    m_callBounds = panel;
    buffer.fillRect(panel, U' ', PFF_Colors::FormPanelColor);
    buffer.writeString(x + 2, y + 1, "Call a Play", PFF_Colors::FormPanelHeadingColor);
    const auto clock = m_controller.playClockRemaining();
    if (clock >= 0)
        buffer.writeString(x + 19, y + 1, "Play clock " + std::to_string(clock) + "s", PFF_Colors::FormPanelHeadingColor);
    if (clock >= 0 && width >= 65)
    {
        const int seconds = m_controller.gameClockRemaining();
        buffer.writeString(x + 39, y + 1, "Game clock " + std::to_string(seconds / 60) + ":" +
            (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60), PFF_Colors::FormPanelHeadingColor);
    }
    m_callRoot.setBounds(panel);
    const std::array<const char*, 5> headings{"Running", "Passing", "Special Teams", "Kickoff", "Defense"};
    const auto count = std::count_if(m_callLists.begin(), m_callLists.end(), [](auto* list) { return list->isVisible(); });
    const int column = (width - 4) / std::max(1, static_cast<int>(count));
    int left = x + 2;
    for (std::size_t i = 0; i < m_callLists.size(); ++i)
        if (m_callLists[i]->isVisible())
        {
            buffer.writeString(left, y + 3, headings[i], PFF_Colors::FormPanelHeadingColor);
            m_callLists[i]->setBounds({Point{left, y + 4}, Size{column - 1, height - 8}});
            left += column;
        }
    m_pauseCall->setBounds({Point{x + width - 9, y + 1}, Size{8, 1}});
    m_makeCall->setBounds({Point{x + width / 2 - 15, y + height - 3}, Size{16, 2}});
    m_timeout->setBounds({Point{x + width / 2 + 3, y + height - 3}, Size{12, 2}});
    const auto& error = m_controller.display().error;
    if (!error.empty())
        buffer.writeString(x + 2, y + height - 4, TextClip::clipUtf8Text(error, width - 4),
            PFF_Colors::FormPanelHeadingColor);
    m_callRoot.draw(surface);
}
