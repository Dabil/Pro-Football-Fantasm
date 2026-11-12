// GameTime view implementation. Renders the controller's display state and forwards widget input as commands.

#include "Screens/GameTimeView.h"
#include "Input/Event.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Surface.h"
#include "Styles/PFF_Theme.h"
#include "UI/Widgets/Label.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

GameTimeView::GameTimeView(std::unique_ptr<PffTest::GameTimeModel> game, WorldActor actor,
                               std::function<void(const WorldEditCommit &)> committed,
                               std::function<void()> back)
    : m_controller(std::move(game), std::move(actor), std::move(committed)), m_return(std::move(back))
{
    auto log = std::make_unique<ScrollBox>();
    auto scrollStyle = log->styleSet();
    scrollStyle.focused = PFF_Colors::ScrollBoxFocusedStyle;
    log->setStyleSet(scrollStyle);
    m_log = log.get();
    m_root.addChild(std::move(log));
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
    const bool handled = m_root.handleEvent(event);
    returnToCallerIfReady();
    return handled;
}
void GameTimeView::update(const Animation::TickEvent &event)
{
    m_controller.tick();
    updateControls();
    m_root.update(event);
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
    const auto &game = display.matchup;
    write(1, display.replay ? "GAME TIME - Replay" : "GAME TIME - Generic Level Sim");
    std::ostringstream clock;
    if (state.quarter == 5)
        clock << "Overtime";
    else
        clock << "Quarter " << state.quarter;
    clock << "  " << state.remaining / 60 << ':' << std::setw(2)
          << std::setfill('0') << state.remaining % 60;

    const auto currentSpot = PffBroadcast::yardLine(state.offense ? game.homeName : game.awayName,
                                                    state.offense ? game.awayName : game.homeName, state.spot);

    write(3, game.awayName + "  " + std::to_string(state.teams[0].score()) + "    at    " +
                 game.homeName + "  " + std::to_string(state.teams[1].score()));
    write(5, display.replay && !display.play ? "Replay has no plays"
                     : state.final ? "FINAL - Results and PBP saved"
                         : state.pendingKick >= 0
                               ? clock.str() + "  |  Kickoff pending: " +
                                     (state.pendingKick ? game.homeName : game.awayName) + "  |  " + currentSpot
                               : clock.str() + "  |  " + (state.offense ? game.homeName : game.awayName) +
                               "  |  Down " + std::to_string(state.down) + " to go " +
                               std::to_string(state.lineToGain - state.spot) + "  |  " + currentSpot);
    write(7, "Team                         Q1   Q2   Q3   Q4   OT   Total");
    for (int t = 0; t < 2; ++t)
    {
        std::ostringstream score;
        score << std::left << std::setw(28) << TextClip::clipUtf8Text(t ? game.homeName : game.awayName, 27)
              << std::right;
        for (int q : state.teams[t].quarters)
            score << std::setw(5) << q;
        score << std::setw(7) << state.teams[t].score();
        write(8 + t, score.str());
    }
    write(11, "Team statistics                   Away          Home");
    auto stat = [&](int row, const std::string &label, const std::string &away, const std::string &home) {
        std::ostringstream o;
        o << std::left << std::setw(32) << label << std::setw(14) << away << home;
        write(row, o.str());
    };
    const auto &a = state.teams[0];
    const auto &h = state.teams[1];
    stat(12, "First downs", std::to_string(a.firstDowns), std::to_string(h.firstDowns));
    stat(13, "Rushing attempts / yards", std::to_string(a.rushes) + " / " + std::to_string(a.rushYards),
         std::to_string(h.rushes) + " / " + std::to_string(h.rushYards));
    stat(14, "Passing complete / attempts",
         std::to_string(a.completions) + " / " + std::to_string(a.attempts),
         std::to_string(h.completions) + " / " + std::to_string(h.attempts));
    stat(15, "Passing yards / sacks taken", std::to_string(a.passYards) + " / " + std::to_string(a.sacks),
         std::to_string(h.passYards) + " / " + std::to_string(h.sacks));
    stat(16, "Interceptions / fumbles lost",
         std::to_string(a.interceptions) + " / " + std::to_string(a.fumblesLost),
         std::to_string(h.interceptions) + " / " + std::to_string(h.fumblesLost));
    stat(17, "Punts / yards", std::to_string(a.punts) + " / " + std::to_string(a.puntYards),
         std::to_string(h.punts) + " / " + std::to_string(h.puntYards));
    if (display.replay && display.play)
        write(18, "Play " + std::to_string(display.playIndex + 1) + " of " + std::to_string(display.playCount));
    constexpr int tabsY = 19;
    m_broadcast->setBounds(Rect{Point{2, tabsY}, Size{14, 3}});
    m_detail->setBounds(Rect{Point{18, tabsY}, Size{14, 3}});
    m_pbp->setBounds(Rect{Point{34, tabsY}, Size{14, 3}});
    const int logY = tabsY + 3;
    const int logHeight = std::max(1, bounds.size.height - logY - 6);
    m_log->setBounds(Rect{Point{2, logY}, Size{width, logHeight}});
    rebuildLog(width);
    const int y = std::max(20, bounds.size.height - 5);
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
    if (display.interruption)
        write(18, "EVENT: " + display.interruption->blocker);
    write(bounds.size.height - 2,
          display.error.empty() ? (display.interruption ? "Waiting for event. Handle it, then Resume Game. Return keeps saved progress."
                             : display.replay      ? "Next Play and Previous Play move through the replay."
                             : display.running     ? "Running to final... Pause to inspect; Return saves and exits."
                             : state.final ? "Final saved. Scroll the play-by-play or select Return."
                                           : "Next Play advances once. Return saves and exits.")
                          : display.error);
    m_root.draw(surface);
}
