#include "Screens/GameTimeScreen.h"
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

GameTimeScreen::GameTimeScreen(std::unique_ptr<PffTest::GenericTestGame> game, WorldActor actor,
                               std::function<void(const WorldEditCommit &)> committed,
                               std::function<void()> back)
    : m_game(std::move(game)), m_actor(std::move(actor)), m_committed(std::move(committed)),
      m_return(std::move(back))
{
    auto log = std::make_unique<ScrollBox>();
    m_log = log.get();
    m_root.addChild(std::move(log));
    for (const auto &command : {std::pair<const char *, const char *>{"Next Play", "next"},
                                {"Run to Final", "run"},
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
            {
                m_running = false;
                nextPlay();
            }
            else if (r.commandId == "run")
            {
                m_error.clear();
                m_running = true;
            }
            else if (r.commandId == "pause")
                m_running = false;
            else if (r.commandId == "return")
                saveAndReturn();
            else if (r.commandId == "resume")
                resumeGame();
            updateControls();
        });
        if (std::string(command.second) == "next")
            m_next = b.get();
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
void GameTimeScreen::onEnter()
{
    m_root.resetFocus();
    m_running = !m_game->state().final && !m_game->interruption();
    if (m_running)
        nextPlay();
    updateControls();
}
void GameTimeScreen::nextPlay()
{
    if (m_game->state().final)
        return;
    const auto result = m_game->next(m_actor);
    m_dirty = true;
    if (result.success)
    {
        m_error.clear();
        if (m_committed)
            m_committed(result.value);
    }
    else
    {
        m_error = "Game stopped: " + result.message +
                  (m_game->interruption() ? "  Resume Game or Return retries the save."
                                          : "  Next Play retries without rerolling.");
        m_running = false;
    }
    const bool final = m_game->state().final;
    if (m_game->interruption())
        m_running = false;
    if (final)
    {
        m_running = false;
        m_returnPending = true;
    }
    updateControls();
}
void GameTimeScreen::updateControls()
{
    const bool final = m_game->state().final;
    const bool interrupted = bool(m_game->interruption());
    m_next->setEnabled(!final && !interrupted && !m_returnPending && !m_running);
    m_run->setEnabled(!final && !interrupted && !m_returnPending && !m_running);
    m_pause->setEnabled(!final && !m_returnPending && m_running);
    m_returnButton->setEnabled(!m_returnPending);
    m_resume->setEnabled(interrupted && !m_returnPending);
}
void GameTimeScreen::resumeGame()
{
    m_running = false;
    const auto result = m_game->resume(m_actor);
    m_dirty = true;
    if (!result.success)
        m_error = "Unable to resume: " + result.message;
    else
    {
        if (m_committed)
            m_committed(result.value.commit);
        m_error.clear();
        m_running = !m_game->interruption();
    }
    updateControls();
}
void GameTimeScreen::saveAndReturn()
{
    if (m_returnPending)
        return;
    m_running = false;
    if (m_game->state().final)
    {
        m_returnPending = true;
        updateControls();
        return;
    }
    const auto result = m_game->save(m_actor);
    if (result.success)
    {
        if (m_committed)
            m_committed(result.value);
        m_error.clear();
        m_returnPending = true;
    }
    else
        m_error = "Unable to save: " + result.message + ". The game remains open.";
    updateControls();
}
void GameTimeScreen::returnToCallerIfReady()
{
    if (!m_returnPending)
        return;
    m_returnPending = false;
    // The callback may pop and destroy this screen. Call it only after widget
    // event/update dispatch has unwound, and never access members afterwards.
    const auto back = std::move(m_return);
    if (back)
        back();
}
bool GameTimeScreen::handleEvent(const Input::Event &event)
{
    // Every user exit follows the same save-before-return path.
    if (const auto *command = event.asCommand())
    {
        if (command->command.code == Input::CommandCode::Cancel ||
            command->command.code == Input::CommandCode::Back)
        {
            saveAndReturn();
            returnToCallerIfReady();
            return true;
        }
    }
    const auto before = m_log->viewport().scrollY();
    const bool handled = m_root.handleEvent(event);
    if (m_log->viewport().scrollY() != before)
        m_follow = !m_log->viewport().canScrollDown();
    returnToCallerIfReady();
    return handled;
}
void GameTimeScreen::update(const Animation::TickEvent &event)
{
    if (m_running)
        nextPlay(); // One bounded play per UI tick; rendering/input remain live.
    m_root.update(event);
    returnToCallerIfReady();
}
void GameTimeScreen::rebuildLog(int width)
{
    if (!m_dirty && width == m_logWidth)
        return;
    const int previous = m_log->viewport().scrollY();
    m_log->clearChildren();
    int row = 0;
    for (const auto &text : m_game->log())
    {
        const auto rows = wrapText(text, std::max(1, width - 2));
        for (const auto &line : rows)
        {
            auto label =
                std::make_unique<Label>(Rect{Point{0, row++}, Size{std::max(1, width - 2), 1}}, line);
            label->setTextStyle(text.rfind("===", 0) == 0   ? PFF_Colors::NormalTextHeadingColor
                                : text.rfind("---", 0) == 0 ? PFF_Colors::NormalTextColor.withBold()
                                                            : PFF_Colors::NormalTextColor);
            m_log->addChild(std::move(label));
        }
    }
    m_log->setContentSize(std::max(1, width - 2), row);
    if (m_follow)
        m_log->viewport().scrollToBottom();
    else
        m_log->viewport().scrollTo(0, previous);
    m_logWidth = width;
    m_dirty = false;
}
void GameTimeScreen::draw(Surface &surface)
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
    const auto &state = m_game->state();
    const auto &game = m_game->matchup();
    write(1, "GAME TIME - Generic Level Sim");
    write(3, game.awayName + "  " + std::to_string(state.teams[0].score()) + "    at    " + game.homeName +
                 "  " + std::to_string(state.teams[1].score()));
    std::ostringstream clock;
    clock << "Quarter " << state.quarter << "  " << state.remaining / 60 << ':' << std::setw(2)
          << std::setfill('0') << state.remaining % 60;

    std::string current_spot = "";
    if (state.spot < 50)
        current_spot = "  |  Own " + std::to_string(state.spot);
    else if (state.spot == 50)
        current_spot = "  |  The " + std::to_string(state.spot);
    else if (state.spot > 50)
        current_spot = "  |  Opp " + std::to_string(100 - state.spot);

    write(5, state.final ? "FINAL - Results and PBP saved"
                         : clock.str() + "  |  " + (state.offense ? game.homeName : game.awayName) +
                               "  |  Down " + std::to_string(state.down) + "  To go " +
                               std::to_string(state.lineToGain - state.spot) + current_spot);
    write(7, "Team                         Q1   Q2   Q3   Q4   Total");
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
    const int logHeight = std::max(1, bounds.size.height - 25);
    m_log->setBounds(Rect{Point{2, 19}, Size{width, logHeight}});
    rebuildLog(width);
    const int y = std::max(20, bounds.size.height - 5);
    m_next->setBounds(Rect{Point{2, y}, Size{20, 3}});
    m_run->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_pause->setBounds(Rect{Point{46, y}, Size{20, 3}});
    m_resume->setBounds(Rect{Point{24, y}, Size{20, 3}});
    m_returnButton->setBounds(Rect{Point{std::max(68, bounds.size.width - 22), y}, Size{20, 3}});
    updateControls();
    m_log->setVisible(!small);
    m_next->setVisible(!small);
    m_run->setVisible(!small && !m_game->interruption());
    m_resume->setVisible(!small && bool(m_game->interruption()));
    m_pause->setVisible(!small);
    m_returnButton->setVisible(!small);
    if (small)
        write(19, "Enlarge the window to at least 90 x 30 to view game controls.");
    if (m_game->interruption())
        write(18, "EVENT: " + m_game->interruption()->blocker);
    write(bounds.size.height - 2,
          m_error.empty() ? (m_game->interruption() ? "Waiting for event. Handle it, then Resume Game. Return keeps saved progress."
                             : m_running     ? "Running to final... Pause to inspect; Return saves and exits."
                             : state.final ? "Final saved. Scroll the play-by-play or select Return."
                                           : "Paused. Next Play advances once. Return saves and exits.")
                          : m_error);
    m_root.draw(surface);
}
